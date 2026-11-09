/**
 * @file Mp4Record.cpp
 * @brief Media Foundation 写 H.264 MP4
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Mp4Record.h"

#include <QDateTime>
#include <QImage>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <cstring>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace Vision {

Mp4Record::Mp4Record()
    : m_writer(0)
    , m_stream(0)
    , m_width(0)
    , m_height(0)
    , m_fps(5)
    , m_pts(0)
    , m_lastMs(0)
    , m_on(false)
{
}

Mp4Record::~Mp4Record()
{
    stop();
}

bool Mp4Record::active() const
{
    return m_on;
}

bool Mp4Record::start(const std::wstring& path, int width, int height, int fps)
{
    stop();
    if (path.empty() || width < 2 || height < 2) {
        return false;
    }
    width &= ~1;
    height &= ~1;
    if (fps < 1) {
        fps = 5;
    }
    CoInitializeEx(0, COINIT_MULTITHREADED);
    if (MFStartup(MF_VERSION, MFSTARTUP_LITE) != S_OK) {
        return false;
    }
    IMFSinkWriter* writer = 0;
    if (MFCreateSinkWriterFromURL(path.c_str(), 0, 0, &writer) != S_OK || !writer) {
        return false;
    }
    IMFMediaType* outType = 0;
    MFCreateMediaType(&outType);
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    outType->SetUINT32(MF_MT_AVG_BITRATE, 2000000);
    MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height));
    MFSetAttributeRatio(outType, MF_MT_FRAME_RATE, static_cast<UINT32>(fps), 1);
    MFSetAttributeRatio(outType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    DWORD stream = 0;
    HRESULT hr = writer->AddStream(outType, &stream);
    outType->Release();
    if (hr != S_OK) {
        writer->Release();
        return false;
    }
    IMFMediaType* inType = 0;
    MFCreateMediaType(&inType);
    inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    MFSetAttributeSize(inType, MF_MT_FRAME_SIZE, static_cast<UINT32>(width), static_cast<UINT32>(height));
    MFSetAttributeRatio(inType, MF_MT_FRAME_RATE, static_cast<UINT32>(fps), 1);
    MFSetAttributeRatio(inType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    hr = writer->SetInputMediaType(stream, inType, 0);
    inType->Release();
    if (hr != S_OK || writer->BeginWriting() != S_OK) {
        writer->Release();
        return false;
    }
    m_writer = writer;
    m_stream = stream;
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_pts = 0;
    m_lastMs = 0;
    m_on = true;
    return true;
}

bool Mp4Record::write(const QImage& image)
{
    if (!m_on || !m_writer || image.isNull()) {
        return false;
    }
    const long long now = QDateTime::currentMSecsSinceEpoch();
    const long long gap = 1000 / (m_fps < 1 ? 5 : m_fps);
    if (m_lastMs != 0 && now - m_lastMs < gap) {
        return true;
    }
    QImage frame = image.convertToFormat(QImage::Format_RGB32);
    if (frame.width() != m_width || frame.height() != m_height) {
        frame = frame.scaled(m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    const int bytes = m_width * m_height * 4;
    IMFSample* sample = 0;
    IMFMediaBuffer* buffer = 0;
    if (MFCreateSample(&sample) != S_OK || MFCreateMemoryBuffer(static_cast<DWORD>(bytes), &buffer) != S_OK) {
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
    for (int y = 0; y < m_height; ++y) {
        std::memcpy(dest + static_cast<size_t>(y) * static_cast<size_t>(m_width) * 4,
            frame.constScanLine(y), static_cast<size_t>(m_width) * 4);
    }
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(bytes));
    sample->AddBuffer(buffer);
    buffer->Release();
    const long long duration = 10000000ll / m_fps;
    sample->SetSampleTime(m_pts);
    sample->SetSampleDuration(duration);
    m_pts += duration;
    IMFSinkWriter* writer = static_cast<IMFSinkWriter*>(m_writer);
    const HRESULT hr = writer->WriteSample(m_stream, sample);
    sample->Release();
    if (hr != S_OK) {
        return false;
    }
    m_lastMs = now;
    return true;
}

void Mp4Record::stop()
{
    if (m_writer) {
        IMFSinkWriter* writer = static_cast<IMFSinkWriter*>(m_writer);
        writer->Finalize();
        writer->Release();
        m_writer = 0;
    }
    m_on = false;
}

}
