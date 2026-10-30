/**
 * @file WgcCapture.cpp
 * @brief Windows.Graphics.Capture 抓窗口。系统不支持时 grab 返回 false
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "WgcCapture.h"

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <d3d11.h>
#include <dxgi.h>

#include <cstring>
#include <mutex>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\windowsapp.lib")

namespace Vision {

struct WgcCapture::Impl {
    Impl()
        : device(0)
        , context(0)
        , stage(0)
        , stageW(0)
        , stageH(0)
        , hwnd(0)
        , width(0)
        , height(0)
        , have(false)
        , failedHwnd(0)
        , winrtDevice(nullptr)
        , item(nullptr)
        , pool(nullptr)
        , session(nullptr)
    {
    }

    ~Impl()
    {
        stop();
        releaseStage();
        if (context) {
            context->Release();
        }
        if (device) {
            device->Release();
        }
    }

    void releaseStage()
    {
        if (stage) {
            stage->Release();
            stage = 0;
        }
        stageW = 0;
        stageH = 0;
    }

    void stop()
    {
        try {
            if (session) {
                session.Close();
            }
            if (pool) {
                pool.Close();
            }
        } catch (...) {
        }
        session = nullptr;
        pool = nullptr;
        item = nullptr;
        hwnd = 0;
        have = false;
        releaseStage();
    }

    bool ensureDevice()
    {
        if (device && winrtDevice) {
            return true;
        }
        D3D_FEATURE_LEVEL level;
        if (D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT, 0, 0,
                D3D11_SDK_VERSION, &device, &level, &context) != S_OK) {
            return false;
        }
        IDXGIDevice* dxgi = 0;
        if (device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi)) != S_OK) {
            return false;
        }
        HMODULE lib = GetModuleHandleW(L"d3d11.dll");
        typedef HRESULT(WINAPI* CreateFn)(IDXGIDevice*, IInspectable**);
        CreateFn create = lib ? reinterpret_cast<CreateFn>(GetProcAddress(lib, "CreateDirect3D11DeviceFromDXGIDevice")) : 0;
        winrt::com_ptr<IInspectable> inspectable;
        const HRESULT hr = create ? create(dxgi, inspectable.put()) : E_FAIL;
        dxgi->Release();
        if (hr != S_OK || !inspectable) {
            return false;
        }
        winrtDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
        return static_cast<bool>(winrtDevice);
    }

    bool start(HWND window)
    {
        stop();
        failedHwnd = 0;
        if (!ensureDevice()) {
            failedHwnd = window;
            return false;
        }
        try {
            if (!winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported()) {
                failedHwnd = window;
                return false;
            }
            winrt::com_ptr<IGraphicsCaptureItemInterop> interop =
                winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
            winrt::com_ptr<IInspectable> raw;
            if (interop->CreateForWindow(window, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(), raw.put_void()) != S_OK
                || !raw) {
                failedHwnd = window;
                return false;
            }
            item = raw.as<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
            const winrt::Windows::Graphics::SizeInt32 size = item.Size();
            width = size.Width;
            height = size.Height;
            pool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
                winrtDevice,
                winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
                2,
                size);
            pool.FrameArrived([this](auto& sender, auto&) { onFrame(sender); });
            session = pool.CreateCaptureSession(item);
            session.StartCapture();
            hwnd = window;
            return true;
        } catch (...) {
            failedHwnd = window;
            stop();
            return false;
        }
    }

    void onFrame(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender)
    {
        try {
            const auto frame = sender.TryGetNextFrame();
            if (!frame) {
                return;
            }
            const auto access = frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> tex;
            if (access->GetInterface(__uuidof(ID3D11Texture2D), tex.put_void()) != S_OK || !tex) {
                return;
            }
            D3D11_TEXTURE2D_DESC desc;
            tex->GetDesc(&desc);
            const int w = static_cast<int>(desc.Width);
            const int h = static_cast<int>(desc.Height);
            if (!stage || stageW != w || stageH != h) {
                releaseStage();
                D3D11_TEXTURE2D_DESC td = desc;
                td.BindFlags = 0;
                td.MiscFlags = 0;
                td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                td.Usage = D3D11_USAGE_STAGING;
                if (device->CreateTexture2D(&td, 0, &stage) != S_OK || !stage) {
                    return;
                }
                stageW = w;
                stageH = h;
            }
            context->CopyResource(stage, tex.get());
            D3D11_MAPPED_SUBRESOURCE map;
            if (context->Map(stage, 0, D3D11_MAP_READ, 0, &map) != S_OK) {
                return;
            }
            CommBytes copy(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
            for (int y = 0; y < h; ++y) {
                memcpy(copy.data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4,
                    static_cast<const unsigned char*>(map.pData) + static_cast<size_t>(y) * map.RowPitch,
                    static_cast<size_t>(w) * 4);
            }
            context->Unmap(stage, 0);
            std::lock_guard<std::mutex> lock(mu);
            latest.swap(copy);
            width = w;
            height = h;
            have = true;
        } catch (...) {
        }
    }

    ID3D11Device* device;
    ID3D11DeviceContext* context;
    ID3D11Texture2D* stage;     ///< 复用 staging，避免每帧 CreateTexture2D
    int stageW;
    int stageH;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice;
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool pool;
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session;
    HWND hwnd;
    int width;
    int height;
    bool have;
    HWND failedHwnd;
    std::mutex mu;
    CommBytes latest;
};

WgcCapture::WgcCapture()
    : m_impl(new Impl())
{
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {
    }
}

WgcCapture::~WgcCapture()
{
    delete m_impl;
}

void WgcCapture::reset()
{
    m_impl->stop();
    m_impl->failedHwnd = 0;
}

bool WgcCapture::grab(HWND hwnd, CommBytes* bgra, int* width, int* height)
{
    if (!hwnd || !bgra || !IsWindow(hwnd)) {
        return false;
    }
    if (m_impl->failedHwnd == hwnd) {
        return false;
    }
    if (m_impl->hwnd != hwnd) {
        if (!m_impl->start(hwnd)) {
            return false;
        }
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    if (!m_impl->have || m_impl->latest.empty()) {
        return false;
    }
    *bgra = m_impl->latest;
    if (width) {
        *width = m_impl->width;
    }
    if (height) {
        *height = m_impl->height;
    }
    return true;
}

}
