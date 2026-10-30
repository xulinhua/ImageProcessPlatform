/**
 * @file DxgiCapture.cpp
 * @brief DXGI Desktop Duplication
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DxgiCapture.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <cstring>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace Vision {

struct DxgiCapture::Impl {
    Impl()
        : device(0)
        , context(0)
        , dup(0)
        , stage(0)
        , stageW(0)
        , stageH(0)
        , monitor(-1)
        , width(0)
        , height(0)
        , originX(0)
        , originY(0)
        , heldW(0)
        , heldH(0)
    {}
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGIOutputDuplication* dup;
    ID3D11Texture2D* stage;             ///< 复用的 CPU 可读暂存纹理；每帧新建会把提交内存顶到近 GB
    int stageW;
    int stageH;
    int monitor;
    int width;
    int height;
    int originX;
    int originY;
    std::vector<unsigned char> held;
    int heldW;
    int heldH;

    /** @brief 把 bgra 收成与 held 同尺寸，再拷贝，避免 assign 反复扩容 */
    void copyHeldTo(CommBytes* bgra) const
    {
        if (!bgra) {
            return;
        }
        const size_t bytes = held.size();
        if (bgra->size() != bytes) {
            bgra->assign(held.begin(), held.end());
        } else if (bytes > 0) {
            std::memcpy(bgra->data(), held.data(), bytes);
        }
    }

    /** @brief 整帧拷贝。脏矩形不能当底图，首帧是黑的时没再脏到的地方会一直黑 */
    bool takeFrame(const unsigned char* src, int pitch, int w, int h, bool* changed, CommBytes* bgra)
    {
        const size_t rowBytes = static_cast<size_t>(w) * 4;
        const size_t bytes = rowBytes * static_cast<size_t>(h);
        bool changedPx = held.size() != bytes || heldW != w || heldH != h;
        if (!changedPx) {
            for (int y = 0; y < h; ++y) {
                if (std::memcmp(held.data() + static_cast<size_t>(y) * rowBytes,
                        src + static_cast<size_t>(y) * static_cast<size_t>(pitch), rowBytes) != 0) {
                    changedPx = true;
                    break;
                }
            }
        }
        if (!changedPx) {
            *changed = false;
            copyHeldTo(bgra);
            return true;
        }
        if (held.size() != bytes) {
            held.resize(bytes);
        }
        for (int y = 0; y < h; ++y) {
            std::memcpy(held.data() + static_cast<size_t>(y) * rowBytes,
                src + static_cast<size_t>(y) * static_cast<size_t>(pitch), rowBytes);
        }
        heldW = w;
        heldH = h;
        *changed = true;
        copyHeldTo(bgra);
        return true;
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

    void releaseDup()
    {
        if (dup) {
            dup->Release();
            dup = 0;
        }
    }
    void releaseAll()
    {
        releaseDup();
        releaseStage();
        if (context) {
            context->Release();
            context = 0;
        }
        if (device) {
            device->Release();
            device = 0;
        }
        monitor = -1;
        held.clear();
        held.shrink_to_fit();
        heldW = 0;
        heldH = 0;
    }

    /** @brief 按分辨率复用 staging，避免驱动堆里堆积未释放的纹理 */
    bool ensureStage(const D3D11_TEXTURE2D_DESC& srcDesc)
    {
        const int w = static_cast<int>(srcDesc.Width);
        const int h = static_cast<int>(srcDesc.Height);
        if (stage && stageW == w && stageH == h) {
            return true;
        }
        releaseStage();
        D3D11_TEXTURE2D_DESC td = srcDesc;
        td.BindFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        td.Usage = D3D11_USAGE_STAGING;
        td.MiscFlags = 0;
        if (device->CreateTexture2D(&td, 0, &stage) != S_OK || !stage) {
            return false;
        }
        stageW = w;
        stageH = h;
        return true;
    }
};

DxgiCapture::DxgiCapture()
    : m_impl(new Impl())
{}

DxgiCapture::~DxgiCapture()
{
    m_impl->releaseAll();
    delete m_impl;
}

void DxgiCapture::reset()
{
    m_impl->releaseAll();
}

bool DxgiCapture::grab(int monitor, CommBytes* bgra, int* width, int* height, int* originX, int* originY, bool* changed)
{
    if (changed) {
        *changed = false;
    }
    if (!bgra || monitor < 0) {
        return false;
    }
    if (m_impl->monitor != monitor || !m_impl->dup) {
        m_impl->releaseAll();
        D3D_FEATURE_LEVEL level;
        if (D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, D3D11_CREATE_DEVICE_BGRA_SUPPORT, 0, 0,
                D3D11_SDK_VERSION, &m_impl->device, &level, &m_impl->context) != S_OK) {
            return false;
        }
        IDXGIDevice* dxgi = 0;
        if (m_impl->device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi)) != S_OK) {
            m_impl->releaseAll();
            return false;
        }
        IDXGIAdapter* adapter = 0;
        if (dxgi->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&adapter)) != S_OK) {
            dxgi->Release();
            m_impl->releaseAll();
            return false;
        }
        IDXGIOutput* output = 0;
        if (adapter->EnumOutputs(static_cast<UINT>(monitor), &output) != S_OK) {
            adapter->Release();
            dxgi->Release();
            m_impl->releaseAll();
            return false;
        }
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);
        m_impl->originX = desc.DesktopCoordinates.left;
        m_impl->originY = desc.DesktopCoordinates.top;
        m_impl->width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
        m_impl->height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
        IDXGIOutput1* output1 = 0;
        HRESULT hr = output->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&output1));
        output->Release();
        adapter->Release();
        dxgi->Release();
        if (hr != S_OK) {
            m_impl->releaseAll();
            return false;
        }
        hr = output1->DuplicateOutput(m_impl->device, &m_impl->dup);
        output1->Release();
        if (hr != S_OK) {
            m_impl->releaseAll();
            return false;
        }
        m_impl->monitor = monitor;
    }
    if (originX) {
        *originX = m_impl->originX;
    }
    if (originY) {
        *originY = m_impl->originY;
    }
    if (width) {
        *width = m_impl->width;
    }
    if (height) {
        *height = m_impl->height;
    }

    IDXGIResource* resource = 0;
    DXGI_OUTDUPL_FRAME_INFO info;
    const HRESULT hr = m_impl->dup->AcquireNextFrame(0, &info, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // 画面没变时也把上一帧交出去。主码流刚打开时要能补一帧，不能干等下一次变化
        if (bgra && m_impl->heldW > 0 && m_impl->heldH > 0 && !m_impl->held.empty()) {
            bgra->assign(m_impl->held.begin(), m_impl->held.end());
            if (width) {
                *width = m_impl->heldW;
            }
            if (height) {
                *height = m_impl->heldH;
            }
        }
        return true;
    }
    if (hr == DXGI_ERROR_ACCESS_LOST) {
        m_impl->releaseDup();
        m_impl->monitor = -1;
        return false;
    }
    if (hr != S_OK || !resource) {
        return false;
    }
    bool dirty = info.TotalMetadataBufferSize > 0 || info.LastPresentTime.QuadPart != 0;
    ID3D11Texture2D* tex = 0;
    resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex));
    resource->Release();
    if (!tex) {
        m_impl->dup->ReleaseFrame();
        return false;
    }
    D3D11_TEXTURE2D_DESC td;
    tex->GetDesc(&td);
    if (!m_impl->ensureStage(td)) {
        tex->Release();
        m_impl->dup->ReleaseFrame();
        return false;
    }
    m_impl->context->CopyResource(m_impl->stage, tex);
    tex->Release();
    D3D11_MAPPED_SUBRESOURCE map;
    if (m_impl->context->Map(m_impl->stage, 0, D3D11_MAP_READ, 0, &map) != S_OK) {
        m_impl->dup->ReleaseFrame();
        return false;
    }
    const int w = static_cast<int>(td.Width);
    const int h = static_cast<int>(td.Height);
    bool frameChanged = true;
    m_impl->takeFrame(static_cast<const unsigned char*>(map.pData), static_cast<int>(map.RowPitch), w, h, &frameChanged, bgra);
    m_impl->context->Unmap(m_impl->stage, 0);
    m_impl->dup->ReleaseFrame();
    m_impl->width = w;
    m_impl->height = h;
    dirty = frameChanged;
    if (width) {
        *width = w;
    }
    if (height) {
        *height = h;
    }
    if (changed) {
        *changed = dirty;
    }
    return true;
}

}
