/**
 * @file H264Codec.cpp
 * @brief Media Foundation H.264，打不开再试 OpenH264
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "H264Codec.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <codecapi.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <strmif.h>

#include <cstdio>
#include <cstring>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

namespace Vision {

H264Codec::H264Codec()
    : m_mf(0)
    , m_mfEvents(0)
    , m_dec(0)
    , m_open(0)
    , m_openLib(0)
    , m_encW(0)
    , m_encH(0)
    , m_decW(0)
    , m_decH(0)
    , m_fps(15)
    , m_bitrate(2000000)
    , m_frames(0)
    , m_mfAsync(false)
    , m_useOpen(false)
    , m_broken(false)
    , m_primeFails(0)
    , m_pts(0)
{
    m_err[0] = 0;
}

H264Codec::~H264Codec()
{
    closeEncoder();
    closeDecoder();
    if (m_openLib) {
        FreeLibrary(static_cast<HMODULE>(m_openLib));
        m_openLib = 0;
    }
}

void H264Codec::requestKey()
{
    m_frames = 0;
}

bool H264Codec::encodeReady() const
{
    return !m_broken && (m_mf != 0 || m_open != 0);
}

bool H264Codec::isBroken() const
{
    return m_broken;
}

const char* H264Codec::lastError() const
{
    return m_err;
}

bool H264Codec::scaleBgra(const unsigned char* src, int srcW, int srcH, int longEdge,
    std::vector<unsigned char>* dst, int* dstW, int* dstH) const
{
    return fit(src, srcW, srcH, longEdge, dst, dstW, dstH);
}

void H264Codec::closeEncoder()
{
    if (m_mf) {
        static_cast<IMFTransform*>(m_mf)->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        static_cast<IMFTransform*>(m_mf)->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        static_cast<IMFTransform*>(m_mf)->Release();
        m_mf = 0;
    }
    if (m_mfEvents) {
        static_cast<IMFMediaEventGenerator*>(m_mfEvents)->Release();
        m_mfEvents = 0;
    }
    if (m_open) {
        OpenEncoder* enc = static_cast<OpenEncoder*>(m_open);
        enc->Uninitialize();
        if (m_openLib) {
            OpenDestroy destroy = reinterpret_cast<OpenDestroy>(GetProcAddress(static_cast<HMODULE>(m_openLib), "WelsDestroySVCEncoder"));
            if (destroy) {
                destroy(enc);
            }
        }
        m_open = 0;
    }
    m_useOpen = false;
    m_mfAsync = false;
    m_broken = false;
    m_primeFails = 0;
    m_encW = 0;
    m_encH = 0;
    m_frames = 0;
    m_pts = 0;
    m_err[0] = 0;
}

void H264Codec::closeDecoder()
{
    if (m_dec) {
        static_cast<IMFTransform*>(m_dec)->Release();
        m_dec = 0;
    }
    m_decW = 0;
    m_decH = 0;
}

bool H264Codec::fit(const unsigned char* src, int srcW, int srcH, int longEdge, std::vector<unsigned char>* dst, int* dstW, int* dstH) const
{
    if (!src || srcW < 2 || srcH < 2 || !dst) {
        return false;
    }
    int outW = srcW;
    int outH = srcH;
    const int edge = outW > outH ? outW : outH;
    if (longEdge > 0 && edge > longEdge) {
        outW = outW * longEdge / edge;
        outH = outH * longEdge / edge;
    }
    // H.264 宏块对齐到 16；再保底偶数列行
    outW &= ~15;
    outH &= ~15;
    if (outW < 16 || outH < 16) {
        return false;
    }
    dst->resize(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4);
    if (outW == srcW && outH == srcH) {
        std::memcpy(dst->data(), src, dst->size());
    } else {
        for (int y = 0; y < outH; ++y) {
            const int sy = y * srcH / outH;
            unsigned char* row = dst->data() + static_cast<size_t>(y) * static_cast<size_t>(outW) * 4;
            const unsigned char* srcRow = src + static_cast<size_t>(sy) * static_cast<size_t>(srcW) * 4;
            for (int x = 0; x < outW; ++x) {
                const int sx = x * srcW / outW;
                std::memcpy(row + static_cast<size_t>(x) * 4, srcRow + static_cast<size_t>(sx) * 4, 4);
            }
        }
    }
    if (dstW) {
        *dstW = outW;
    }
    if (dstH) {
        *dstH = outH;
    }
    return true;
}

void H264Codec::toNv12(const unsigned char* bgra, int width, int height, std::vector<unsigned char>* nv12) const
{
    nv12->assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 3 / 2, 0);
    unsigned char* yPlane = nv12->data();
    unsigned char* uv = yPlane + static_cast<size_t>(width) * static_cast<size_t>(height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned char* p = bgra + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            const int b = p[0];
            const int g = p[1];
            const int r = p[2];
            yPlane[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] =
                static_cast<unsigned char>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
            if ((y & 1) == 0 && (x & 1) == 0) {
                const size_t uvAt = static_cast<size_t>(y / 2) * static_cast<size_t>(width) + static_cast<size_t>(x);
                uv[uvAt] = static_cast<unsigned char>(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
                uv[uvAt + 1] = static_cast<unsigned char>(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
            }
        }
    }
}

void H264Codec::toI420(const unsigned char* bgra, int width, int height, std::vector<unsigned char>* i420) const
{
    const size_t ySize = static_cast<size_t>(width) * static_cast<size_t>(height);
    i420->assign(ySize * 3 / 2, 0);
    unsigned char* yPlane = i420->data();
    unsigned char* uPlane = yPlane + ySize;
    unsigned char* vPlane = uPlane + ySize / 4;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned char* p = bgra + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            const int b = p[0];
            const int g = p[1];
            const int r = p[2];
            yPlane[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] =
                static_cast<unsigned char>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
            if ((y & 1) == 0 && (x & 1) == 0) {
                const size_t c = static_cast<size_t>(y / 2) * static_cast<size_t>(width / 2) + static_cast<size_t>(x / 2);
                uPlane[c] = static_cast<unsigned char>(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
                vPlane[c] = static_cast<unsigned char>(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
            }
        }
    }
}

void H264Codec::fromNv12(const unsigned char* nv12, int width, int height, std::vector<unsigned char>* bgra) const
{
    bgra->resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    const unsigned char* yPlane = nv12;
    const unsigned char* uv = nv12 + static_cast<size_t>(width) * static_cast<size_t>(height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int yy = static_cast<int>(yPlane[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)]) - 16;
            const size_t uvAt = static_cast<size_t>(y / 2) * static_cast<size_t>(width) + static_cast<size_t>(x & ~1);
            const int u = static_cast<int>(uv[uvAt]) - 128;
            const int v = static_cast<int>(uv[uvAt + 1]) - 128;
            int r = (298 * yy + 409 * v + 128) >> 8;
            int g = (298 * yy - 100 * u - 208 * v + 128) >> 8;
            int b = (298 * yy + 516 * u + 128) >> 8;
            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            unsigned char* p = bgra->data() + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            p[0] = static_cast<unsigned char>(b);
            p[1] = static_cast<unsigned char>(g);
            p[2] = static_cast<unsigned char>(r);
            p[3] = 255;
        }
    }
}

bool H264Codec::openMf(int width, int height, int fps, int bitrate)
{
    CoInitializeEx(0, COINIT_MULTITHREADED);
    if (MFStartup(MF_VERSION, MFSTARTUP_LITE) != S_OK) {
        sprintf_s(m_err, "MFStartup失败");
        return false;
    }
    IMFTransform* transform = 0;
    // 优先微软同步软编（CLSID 固定），再枚举其它同步 MFT；跳过异步硬编
    if (CoCreateInstance(CLSID_CMSH264EncoderMFT, 0, CLSCTX_INPROC_SERVER,
            __uuidof(IMFTransform), reinterpret_cast<void**>(&transform)) != S_OK) {
        transform = 0;
    }
    if (!transform) {
        MFT_REGISTER_TYPE_INFO inInfo = { MFMediaType_Video, MFVideoFormat_NV12 };
        MFT_REGISTER_TYPE_INFO outInfo = { MFMediaType_Video, MFVideoFormat_H264 };
        IMFActivate** items = 0;
        UINT32 count = 0;
        const DWORD flagsSync = MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER;
        if (MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flagsSync, &inInfo, &outInfo, &items, &count) == S_OK && count > 0) {
            for (UINT32 i = 0; i < count && !transform; ++i) {
                IMFTransform* cand = 0;
                if (items[i]->ActivateObject(__uuidof(IMFTransform), reinterpret_cast<void**>(&cand)) != S_OK || !cand) {
                    continue;
                }
                IMFAttributes* attr = 0;
                bool async = false;
                if (cand->GetAttributes(&attr) == S_OK && attr) {
                    UINT32 flag = 0;
                    if (attr->GetUINT32(MF_TRANSFORM_ASYNC, &flag) == S_OK && flag) {
                        async = true;
                    }
                    attr->Release();
                }
                if (async) {
                    cand->Release();
                    continue;
                }
                transform = cand;
            }
            for (UINT32 i = 0; i < count; ++i) {
                items[i]->Release();
            }
            CoTaskMemFree(items);
        }
    }
    if (!transform) {
        sprintf_s(m_err, "没有同步H264编码器");
        return false;
    }
    m_mfAsync = false;
    IMFMediaType* outType = 0;
    MFCreateMediaType(&outType);
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height));
    MFSetAttributeRatio(outType, MF_MT_FRAME_RATE, static_cast<UINT32>(fps < 1 ? 15 : fps), 1);
    MFSetAttributeRatio(outType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    outType->SetUINT32(MF_MT_AVG_BITRATE, static_cast<UINT32>(bitrate < 100000 ? 100000 : bitrate));
    if (transform->SetOutputType(0, outType, 0) != S_OK) {
        outType->Release();
        transform->Release();
        sprintf_s(m_err, "MF SetOutputType失败");
        return false;
    }
    outType->Release();
    IMFMediaType* inType = 0;
    MFCreateMediaType(&inType);
    inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(inType, MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height));
    MFSetAttributeRatio(inType, MF_MT_FRAME_RATE, static_cast<UINT32>(fps < 1 ? 15 : fps), 1);
    MFSetAttributeRatio(inType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (transform->SetInputType(0, inType, 0) != S_OK) {
        inType->Release();
        transform->Release();
        sprintf_s(m_err, "MF SetInputType失败");
        return false;
    }
    inType->Release();
    ICodecAPI* api = 0;
    if (transform->QueryInterface(__uuidof(ICodecAPI), reinterpret_cast<void**>(&api)) == S_OK && api) {
        VARIANT low;
        VariantInit(&low);
        low.vt = VT_BOOL;
        low.boolVal = VARIANT_TRUE;
        api->SetValue(&CODECAPI_AVLowLatencyMode, &low);
        api->Release();
    }
    transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    m_mf = transform;
    m_useOpen = false;
    sprintf_s(m_err, "MF同步编码器 %dx%d", width, height);
    return true;
}

bool H264Codec::openOpen(int width, int height, int fps, int bitrate)
{
    if (!m_openLib) {
        m_openLib = LoadLibraryW(L"openh264.dll");
    }
    if (!m_openLib) {
        sprintf_s(m_err, "无openh264.dll");
        return false;
    }
    OpenCreate create = reinterpret_cast<OpenCreate>(GetProcAddress(static_cast<HMODULE>(m_openLib), "WelsCreateSVCEncoder"));
    if (!create) {
        return false;
    }
    OpenEncoder* enc = 0;
    if (create(&enc) != 0 || !enc) {
        return false;
    }
    OpenParam param;
    param.usage = 0;
    param.width = width;
    param.height = height;
    param.bitrate = bitrate < 100000 ? 100000 : bitrate;
    param.rc = 1;
    param.fps = fps < 1 ? 15.0f : static_cast<float>(fps);
    if (enc->Initialize(&param) != 0) {
        OpenDestroy destroy = reinterpret_cast<OpenDestroy>(GetProcAddress(static_cast<HMODULE>(m_openLib), "WelsDestroySVCEncoder"));
        if (destroy) {
            destroy(enc);
        }
        return false;
    }
    m_open = enc;
    m_useOpen = true;
    return true;
}

bool H264Codec::sampleBytes(void* samplePtr, std::vector<unsigned char>* annexb) const
{
    IMFSample* sample = static_cast<IMFSample*>(samplePtr);
    IMFMediaBuffer* buffer = 0;
    if (sample->ConvertToContiguousBuffer(&buffer) != S_OK || !buffer) {
        return false;
    }
    BYTE* data = 0;
    DWORD length = 0;
    if (buffer->Lock(&data, 0, &length) != S_OK || !data || length < 4) {
        buffer->Release();
        return false;
    }
    const bool annex = data[0] == 0 && data[1] == 0 && (data[2] == 1 || (data[2] == 0 && data[3] == 1));
    if (annex) {
        annexb->assign(data, data + length);
    } else {
        annexb->clear();
        DWORD at = 0;
        while (at + 4 <= length) {
            const DWORD nal = (static_cast<DWORD>(data[at]) << 24) | (static_cast<DWORD>(data[at + 1]) << 16)
                | (static_cast<DWORD>(data[at + 2]) << 8) | static_cast<DWORD>(data[at + 3]);
            at += 4;
            if (nal == 0 || at + nal > length) {
                break;
            }
            const unsigned char start[4] = { 0, 0, 0, 1 };
            annexb->insert(annexb->end(), start, start + 4);
            annexb->insert(annexb->end(), data + at, data + at + nal);
            at += nal;
        }
    }
    buffer->Unlock();
    buffer->Release();
    return !annexb->empty();
}

bool H264Codec::encodeFrame(const unsigned char* bgra, int width, int height, bool forceKey, std::vector<unsigned char>* annexb)
{
    if (m_useOpen && m_open) {
        OpenEncoder* enc = static_cast<OpenEncoder*>(m_open);
        if (forceKey) {
            enc->ForceIntraFrame(true);
        }
        std::vector<unsigned char> i420;
        toI420(bgra, width, height, &i420);
        OpenPicture pic;
        std::memset(&pic, 0, sizeof(pic));
        pic.format = 23;
        pic.width = width;
        pic.height = height;
        pic.stride[0] = width;
        pic.stride[1] = width / 2;
        pic.stride[2] = width / 2;
        pic.data[0] = i420.data();
        pic.data[1] = i420.data() + static_cast<size_t>(width) * static_cast<size_t>(height);
        pic.data[2] = pic.data[1] + static_cast<size_t>(width) * static_cast<size_t>(height) / 4;
        OpenFrame info;
        std::memset(&info, 0, sizeof(info));
        if (enc->EncodeFrame(&pic, &info) != 0 || info.bytes <= 0) {
            return false;
        }
        annexb->clear();
        for (int layer = 0; layer < info.layers && layer < 128; ++layer) {
            unsigned char* bits = info.layer[layer].bits;
            int at = 0;
            for (int nal = 0; nal < info.layer[layer].nalCount; ++nal) {
                const int n = info.layer[layer].nalBytes[nal];
                if (n > 0 && bits) {
                    annexb->insert(annexb->end(), bits + at, bits + at + n);
                    at += n;
                }
            }
        }
        return !annexb->empty();
    }
    IMFTransform* transform = static_cast<IMFTransform*>(m_mf);
    if (!transform) {
        return false;
    }
    std::vector<unsigned char> nv12;
    toNv12(bgra, width, height, &nv12);
    IMFSample* sample = 0;
    IMFMediaBuffer* buffer = 0;
    if (MFCreateSample(&sample) != S_OK || MFCreateMemoryBuffer(static_cast<DWORD>(nv12.size()), &buffer) != S_OK) {
        if (sample) {
            sample->Release();
        }
        return false;
    }
    BYTE* dest = 0;
    if (buffer->Lock(&dest, 0, 0) != S_OK) {
        buffer->Release();
        sample->Release();
        return false;
    }
    std::memcpy(dest, nv12.data(), nv12.size());
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(nv12.size()));
    sample->AddBuffer(buffer);
    buffer->Release();
    const long long duration = 10000000ll / (m_fps < 1 ? 15 : m_fps);
    sample->SetSampleTime(m_pts);
    sample->SetSampleDuration(duration);
    m_pts += duration;
    if (forceKey) {
        sample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
    }
    HRESULT hr = transform->ProcessInput(0, sample, 0);
    sample->Release();
    if (hr != S_OK) {
        sprintf_s(m_err, "MF ProcessInput 0x%08lx", static_cast<unsigned long>(hr));
        return false;
    }
    if (m_mfAsync && m_mfEvents) {
        IMFMediaEventGenerator* events = static_cast<IMFMediaEventGenerator*>(m_mfEvents);
        IMFMediaEvent* ev = 0;
        if (events->GetEvent(MF_EVENT_FLAG_NO_WAIT, &ev) != S_OK) {
            sprintf_s(m_err, "MF异步无输出事件");
            return false;
        }
        MediaEventType kind = MEUnknown;
        ev->GetType(&kind);
        ev->Release();
        if (kind != METransformHaveOutput) {
            sprintf_s(m_err, "MF异步事件=%u", static_cast<unsigned>(kind));
            return false;
        }
    }
    MFT_OUTPUT_STREAM_INFO streamInfo;
    std::memset(&streamInfo, 0, sizeof(streamInfo));
    transform->GetOutputStreamInfo(0, &streamInfo);
    // 同步编码器偶发 NEED_MORE_INPUT，多取几次；STREAM_CHANGE 后重配输出类型再取
    for (int tryOut = 0; tryOut < 6; ++tryOut) {
        MFT_OUTPUT_DATA_BUFFER out;
        std::memset(&out, 0, sizeof(out));
        IMFSample* outSample = 0;
        if ((streamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0) {
            MFCreateSample(&outSample);
            IMFMediaBuffer* outBuf = 0;
            const DWORD cap = streamInfo.cbSize > 0 ? streamInfo.cbSize : 1024 * 1024;
            MFCreateMemoryBuffer(cap, &outBuf);
            outSample->AddBuffer(outBuf);
            outBuf->Release();
            out.pSample = outSample;
        }
        DWORD status = 0;
        hr = transform->ProcessOutput(0, 1, &out, &status);
        if (hr == S_OK && out.pSample) {
            const bool ok = sampleBytes(out.pSample, annexb);
            out.pSample->Release();
            if (ok) {
                return true;
            }
        } else if (outSample) {
            outSample->Release();
        }
        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            // 软编常有 1～2 帧延迟，本帧先交出去，下一帧再取；不算硬失败
            sprintf_s(m_err, "MF还需输入(预热)");
            return false;
        }
        if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
            IMFMediaType* type = 0;
            if (transform->GetOutputAvailableType(0, 0, &type) == S_OK && type) {
                transform->SetOutputType(0, type, 0);
                type->Release();
            }
            transform->GetOutputStreamInfo(0, &streamInfo);
            continue;
        }
        sprintf_s(m_err, "MF ProcessOutput 0x%08lx", static_cast<unsigned long>(hr));
        return false;
    }
    sprintf_s(m_err, "MF无编码输出");
    return false;
}

bool H264Codec::encode(const std::vector<unsigned char>& bgra, int width, int height, int longEdge, int fps, int bitrate,
    std::vector<unsigned char>* annexb, int* outW, int* outH, uint8_t* key)
{
    if (!annexb || bgra.empty()) {
        return false;
    }
    if (m_broken) {
        return false;
    }
    std::vector<unsigned char> fitted;
    int fw = 0;
    int fh = 0;
    if (!fit(bgra.data(), width, height, longEdge, &fitted, &fw, &fh)) {
        sprintf_s(m_err, "缩放失败");
        return false;
    }
    if (fps < 1) {
        fps = 15;
    }
    if (bitrate < 100000) {
        bitrate = 1500000;
    }
    if ((!m_mf && !m_open) || fw != m_encW || fh != m_encH || fps != m_fps || bitrate != m_bitrate) {
        closeEncoder();
        m_fps = fps;
        m_bitrate = bitrate;
        // OpenH264 更稳；没有 dll 再走 MF 软编
        if (!openOpen(fw, fh, fps, bitrate) && !openMf(fw, fh, fps, bitrate)) {
            m_broken = true;
            return false;
        }
        m_encW = fw;
        m_encH = fh;
    }
    const bool force = m_frames == 0 || m_primeFails > 0 || (m_frames % (fps * 2) == 0);
    if (!encodeFrame(fitted.data(), fw, fh, force, annexb)) {
        m_primeFails += 1;
        // MF 预热期允许连续空输出；之后换另一后端再试一轮
        if (m_primeFails < 24 && (m_mf || m_open)) {
            return false;
        }
        const bool wasOpen = m_useOpen;
        closeEncoder();
        m_fps = fps;
        m_bitrate = bitrate;
        const bool opened = wasOpen ? openMf(fw, fh, fps, bitrate) : openOpen(fw, fh, fps, bitrate);
        if (!opened || !encodeFrame(fitted.data(), fw, fh, true, annexb)) {
            // 再试一次另一后端
            closeEncoder();
            m_fps = fps;
            m_bitrate = bitrate;
            const bool again = wasOpen ? openOpen(fw, fh, fps, bitrate) : openMf(fw, fh, fps, bitrate);
            if (!again || !encodeFrame(fitted.data(), fw, fh, true, annexb)) {
                closeEncoder();
                m_broken = true;
                if (m_err[0] == 0) {
                    sprintf_s(m_err, "编码输出空");
                }
                return false;
            }
        }
        m_encW = fw;
        m_encH = fh;
        m_primeFails = 0;
    } else {
        m_primeFails = 0;
    }
    m_frames += 1;
    if (outW) {
        *outW = fw;
    }
    if (outH) {
        *outH = fh;
    }
    if (key) {
        *key = force ? 1 : 0;
    }
    return true;
}

bool H264Codec::decode(const unsigned char* data, int size, std::vector<unsigned char>* bgra, int* width, int* height)
{
    if (!data || size < 5 || !bgra) {
        return false;
    }
    if (!m_dec) {
        CoInitializeEx(0, COINIT_MULTITHREADED);
        MFStartup(MF_VERSION, MFSTARTUP_LITE);
        IMFTransform* transform = 0;
        if (CoCreateInstance(CLSID_CMSH264DecoderMFT, 0, CLSCTX_INPROC_SERVER, __uuidof(IMFTransform), reinterpret_cast<void**>(&transform)) != S_OK
            || !transform) {
            return false;
        }
        IMFMediaType* inType = 0;
        MFCreateMediaType(&inType);
        inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        if (transform->SetInputType(0, inType, 0) != S_OK) {
            inType->Release();
            transform->Release();
            return false;
        }
        inType->Release();
        IMFMediaType* outType = 0;
        MFCreateMediaType(&outType);
        outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        if (transform->SetOutputType(0, outType, 0) != S_OK) {
            outType->Release();
            transform->Release();
            return false;
        }
        outType->Release();
        transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
        transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
        m_dec = transform;
    }
    IMFTransform* transform = static_cast<IMFTransform*>(m_dec);
    IMFSample* sample = 0;
    IMFMediaBuffer* buffer = 0;
    if (MFCreateSample(&sample) != S_OK || MFCreateMemoryBuffer(static_cast<DWORD>(size), &buffer) != S_OK) {
        if (sample) {
            sample->Release();
        }
        return false;
    }
    BYTE* dest = 0;
    buffer->Lock(&dest, 0, 0);
    std::memcpy(dest, data, static_cast<size_t>(size));
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(size));
    sample->AddBuffer(buffer);
    buffer->Release();
    sample->SetSampleTime(0);
    sample->SetSampleDuration(333333);
    const HRESULT pushed = transform->ProcessInput(0, sample, 0);
    sample->Release();
    if (pushed != S_OK && pushed != MF_E_NOTACCEPTING) {
        return false;
    }
    MFT_OUTPUT_STREAM_INFO streamInfo;
    std::memset(&streamInfo, 0, sizeof(streamInfo));
    transform->GetOutputStreamInfo(0, &streamInfo);
    MFT_OUTPUT_DATA_BUFFER out;
    std::memset(&out, 0, sizeof(out));
    IMFSample* outSample = 0;
    if ((streamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0) {
        MFCreateSample(&outSample);
        IMFMediaBuffer* outBuf = 0;
        MFCreateMemoryBuffer(streamInfo.cbSize > 0 ? streamInfo.cbSize : 8 * 1024 * 1024, &outBuf);
        outSample->AddBuffer(outBuf);
        outBuf->Release();
        out.pSample = outSample;
    }
    DWORD status = 0;
    if (transform->ProcessOutput(0, 1, &out, &status) != S_OK || !out.pSample) {
        if (outSample) {
            outSample->Release();
        }
        return false;
    }
    IMFMediaType* current = 0;
    int w = m_decW;
    int h = m_decH;
    if (transform->GetOutputCurrentType(0, &current) == S_OK && current) {
        UINT32 uw = 0;
        UINT32 uh = 0;
        if (MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &uw, &uh) == S_OK) {
            w = static_cast<int>(uw);
            h = static_cast<int>(uh);
        }
        current->Release();
    }
    IMFMediaBuffer* outBuf = 0;
    bool ok = false;
    if (w >= 2 && h >= 2 && out.pSample->ConvertToContiguousBuffer(&outBuf) == S_OK && outBuf) {
        BYTE* nv = 0;
        DWORD length = 0;
        if (outBuf->Lock(&nv, 0, &length) == S_OK && nv && length >= static_cast<DWORD>(w * h * 3 / 2)) {
            fromNv12(nv, w, h, bgra);
            outBuf->Unlock();
            ok = true;
            m_decW = w;
            m_decH = h;
            if (width) {
                *width = w;
            }
            if (height) {
                *height = h;
            }
        }
        outBuf->Release();
    }
    out.pSample->Release();
    return ok;
}

}
