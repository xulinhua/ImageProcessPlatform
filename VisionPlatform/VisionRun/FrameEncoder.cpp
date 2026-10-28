/**
 * @file FrameEncoder.cpp
 * @brief 用 WIC 把灰度和 BGR 编成 JPEG
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FrameEncoder.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace Vision {

namespace {

void ensureCom()
{
    static thread_local bool ready = false;
    if (!ready) {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        ready = true;
    }
}

}

bool FrameEncoder::encode(const HsCommData::HsImage& image, int quality, std::vector<unsigned char>* jpeg) const
{
    if (!jpeg) {
        return false;
    }
    jpeg->clear();
    if (image.isNull() || image.pixels.empty()) {
        return true;
    }
    GUID format = GUID_WICPixelFormat8bppGray;
    int channels = 1;
    if (image.encoding == HsCommData::EncodeBgr && image.kind == HsCommData::PixelByte) {
        format = GUID_WICPixelFormat24bppBGR;
        channels = 3;
    } else if (image.encoding == HsCommData::EncodeRgb && image.kind == HsCommData::PixelByte) {
        format = GUID_WICPixelFormat24bppRGB;
        channels = 3;
    } else if (!(image.encoding == HsCommData::EncodeMono && image.kind == HsCommData::PixelByte)) {
        return true;
    }
    if (quality < 1) {
        quality = 1;
    }
    if (quality > 100) {
        quality = 100;
    }
    ensureCom();
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        return true;
    }
    Microsoft::WRL::ComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream))) {
        return true;
    }
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatJpeg, NULL, &encoder))) {
        return true;
    }
    if (FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) {
        return true;
    }
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    IPropertyBag2* bag = 0;
    if (FAILED(encoder->CreateNewFrame(&frame, &bag))) {
        return true;
    }
    if (bag) {
        PROPBAG2 opt;
        ZeroMemory(&opt, sizeof(opt));
        opt.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT qualityVar;
        VariantInit(&qualityVar);
        qualityVar.vt = VT_R4;
        qualityVar.fltVal = static_cast<float>(quality) / 100.0f;
        bag->Write(1, &opt, &qualityVar);
    }
    if (FAILED(frame->Initialize(bag))) {
        if (bag) {
            bag->Release();
        }
        return true;
    }
    if (bag) {
        bag->Release();
    }
    frame->SetSize(static_cast<UINT>(image.width), static_cast<UINT>(image.height));
    WICPixelFormatGUID pixel = format;
    frame->SetPixelFormat(&pixel);
    const UINT stride = static_cast<UINT>(image.stride > 0 ? image.stride : image.width * channels);
    if (FAILED(frame->WritePixels(static_cast<UINT>(image.height), stride,
                                 static_cast<UINT>(image.pixels.size()),
                                 const_cast<BYTE*>(image.pixels.data())))) {
        return true;
    }
    frame->Commit();
    encoder->Commit();
    HGLOBAL mem = 0;
    if (FAILED(GetHGlobalFromStream(stream.Get(), &mem)) || !mem) {
        return true;
    }
    const SIZE_T n = GlobalSize(mem);
    void* bits = GlobalLock(mem);
    if (bits && n > 0) {
        jpeg->assign(reinterpret_cast<unsigned char*>(bits), reinterpret_cast<unsigned char*>(bits) + n);
    }
    if (bits) {
        GlobalUnlock(mem);
    }
    return true;
}

}
