/**
 * @file WindowCapture.cpp
 * @brief PrintWindow / 桌面 StretchBlt；JPEG 优先 Qt，失败再用 GDI+（不依赖 qjpeg 插件）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "WindowCapture.h"

#include <QBuffer>
#include <QImage>

#include <cstring>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace Vision {

namespace {

/** @brief 进程内只启动一次 GDI+ */
class GdiPlusOnce {
public:
    static bool ready()
    {
        static GdiPlusOnce inst;
        return inst.ok_;
    }

private:
    GdiPlusOnce()
        : ok_(false)
        , token_(0)
    {
        Gdiplus::GdiplusStartupInput input;
        ok_ = (Gdiplus::GdiplusStartup(&token_, &input, nullptr) == Gdiplus::Ok);
    }
    ~GdiPlusOnce()
    {
        if (ok_) {
            Gdiplus::GdiplusShutdown(token_);
        }
    }
    bool ok_;
    ULONG_PTR token_;
};

bool encodeJpegGdiPlus(const unsigned char* bgra, int width, int height, int jpegQuality, CommBytes* jpeg)
{
    if (!jpeg || !bgra || width <= 0 || height <= 0 || !GdiPlusOnce::ready()) {
        return false;
    }
    if (jpegQuality < 1) {
        jpegQuality = 1;
    }
    if (jpegQuality > 100) {
        jpegQuality = 100;
    }
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    Gdiplus::Bitmap bitmap(&info, const_cast<unsigned char*>(bgra));
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        return false;
    }

    CLSID clsid = {};
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) {
        return false;
    }
    std::vector<BYTE> buf(size);
    Gdiplus::ImageCodecInfo* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(num, size, codecs);
    bool found = false;
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(codecs[i].MimeType, L"image/jpeg") == 0) {
            clsid = codecs[i].Clsid;
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK || !stream) {
        return false;
    }
    Gdiplus::EncoderParameters params;
    params.Count = 1;
    params.Parameter[0].Guid = Gdiplus::EncoderQuality;
    params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    params.Parameter[0].NumberOfValues = 1;
    ULONG quality = static_cast<ULONG>(jpegQuality);
    params.Parameter[0].Value = &quality;
    if (bitmap.Save(stream, &clsid, &params) != Gdiplus::Ok) {
        stream->Release();
        return false;
    }
    HGLOBAL mem = nullptr;
    if (GetHGlobalFromStream(stream, &mem) != S_OK || !mem) {
        stream->Release();
        return false;
    }
    const SIZE_T bytes = GlobalSize(mem);
    void* ptr = GlobalLock(mem);
    if (!ptr || bytes == 0) {
        if (ptr) {
            GlobalUnlock(mem);
        }
        stream->Release();
        return false;
    }
    jpeg->assign(static_cast<const unsigned char*>(ptr),
        static_cast<const unsigned char*>(ptr) + bytes);
    GlobalUnlock(mem);
    stream->Release();
    return !jpeg->empty();
}

}

bool WindowCapture::captureClient(HWND hwnd, int width, int height, CommBytes* bgra) const
{
    HDC windowDc = GetDC(hwnd);
    if (!windowDc) {
        return false;
    }
    HDC mem = CreateCompatibleDC(windowDc);
    HBITMAP bmp = CreateCompatibleBitmap(windowDc, width, height);
    HGDIOBJ old = SelectObject(mem, bmp);
    BOOL drawn = PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT);
    if (!drawn) {
        drawn = BitBlt(mem, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);
    }
    SelectObject(mem, old);
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bgra->assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);
    const int rows = GetDIBits(windowDc, bmp, 0, static_cast<UINT>(height), &(*bgra)[0], &info, DIB_RGB_COLORS);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(hwnd, windowDc);
    return drawn && rows > 0;
}

PictureMsg WindowCapture::encode(CommBytes& bgra, int width, int height, bool visible, bool operable,
    int jpegQuality, int longEdgeLimit) const
{
    PictureMsg msg;
    msg.visible = visible ? 1 : 0;
    msg.operable = operable ? 1 : 0;
    msg.clientW = static_cast<uint32_t>(width);
    msg.clientH = static_cast<uint32_t>(height);
    if (!visible || bgra.empty() || width <= 0 || height <= 0) {
        return msg;
    }
    if (jpegQuality < 1) {
        jpegQuality = RemoteConst::kJpegQuality;
    }
    if (jpegQuality > 100) {
        jpegQuality = 100;
    }
    if (longEdgeLimit < 320) {
        longEdgeLimit = RemoteConst::kLongEdge;
    }
    for (size_t i = 3; i < bgra.size(); i += 4) {
        bgra[i] = 255;
    }

    int outW = width;
    int outH = height;
    CommBytes pixels = bgra;
    const int longEdge = width > height ? width : height;
    if (longEdge > longEdgeLimit) {
        const double scale = static_cast<double>(longEdgeLimit) / static_cast<double>(longEdge);
        outW = static_cast<int>(width * scale);
        outH = static_cast<int>(height * scale);
        if (outW < 1) {
            outW = 1;
        }
        if (outH < 1) {
            outH = 1;
        }
        QImage image(bgra.data(), width, height, width * 4, QImage::Format_ARGB32);
        const QImage scaled = image.copy().scaled(outW, outH, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                                  .convertToFormat(QImage::Format_ARGB32);
        pixels.resize(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4);
        if (!scaled.isNull() && scaled.bits()) {
            std::memcpy(pixels.data(), scaled.bits(), pixels.size());
        }
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (buffer.open(QIODevice::WriteOnly)) {
        QImage image(pixels.data(), outW, outH, outW * 4, QImage::Format_ARGB32);
        // 直接存 ARGB 时 Qt JPEG 插件可能写出全黑，先收成 RGB32
        QImage copy = image.copy().convertToFormat(QImage::Format_RGB32);
        if (!copy.save(&buffer, "JPEG", jpegQuality)) {
            buffer.close();
            bytes.clear();
            if (buffer.open(QIODevice::WriteOnly)) {
                copy.convertToFormat(QImage::Format_RGB32).save(&buffer, "JPEG", jpegQuality);
            }
        }
        buffer.close();
    }
    if (bytes.isEmpty()) {
        encodeJpegGdiPlus(pixels.data(), outW, outH, jpegQuality, &msg.jpeg);
    } else {
        msg.jpeg.assign(bytes.begin(), bytes.end());
    }
    msg.imageW = static_cast<uint32_t>(outW);
    msg.imageH = static_cast<uint32_t>(outH);
    return msg;
}

PictureMsg WindowCapture::fromBgra(CommBytes& bgra, int width, int height, int clientW, int clientH, bool operable,
    int jpegQuality, int longEdge) const
{
    PictureMsg shot = encode(bgra, width, height, true, operable, jpegQuality, longEdge);
    if (clientW > 0) {
        shot.clientW = static_cast<uint32_t>(clientW);
    }
    if (clientH > 0) {
        shot.clientH = static_cast<uint32_t>(clientH);
    }
    return shot;
}

bool WindowCapture::blank(const CommBytes& bgra, int width, int height) const
{
    if (width < 8 || height < 8) {
        return true;
    }
    const size_t need = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (bgra.size() < need) {
        return true;
    }
    const int xs[4] = { width / 8, width / 2, width * 3 / 4, width - 4 };
    const int ys[4] = { height / 8, height / 2, height * 3 / 4, height - 4 };
    for (int yi = 0; yi < 4; ++yi) {
        for (int xi = 0; xi < 4; ++xi) {
            const unsigned char* p = bgra.data()
                + (static_cast<size_t>(ys[yi]) * static_cast<size_t>(width) + static_cast<size_t>(xs[xi])) * 4;
            if (p[0] > 12 || p[1] > 12 || p[2] > 12) {
                return false;
            }
        }
    }
    return true;
}

bool WindowCapture::grabBgra(HWND hwnd, CommBytes* bgra, int* width, int* height) const
{
    if (!hwnd || !bgra || !IsWindow(hwnd) || IsIconic(hwnd)) {
        return false;
    }
    int w = 0;
    int h = 0;
    if (m_wgc.grab(hwnd, bgra, &w, &h) && w > 0 && h > 0) {
        if (width) {
            *width = w;
        }
        if (height) {
            *height = h;
        }
        return true;
    }
    RECT rect;
    GetClientRect(hwnd, &rect);
    w = rect.right - rect.left;
    h = rect.bottom - rect.top;
    if (w <= 0 || h <= 0 || !captureClient(hwnd, w, h, bgra)) {
        return false;
    }
    if (width) {
        *width = w;
    }
    if (height) {
        *height = h;
    }
    return true;
}

bool WindowCapture::pngFromBgra(const unsigned char* bgra, int width, int height, CommBytes* png) const
{
    if (!bgra || !png || width <= 0 || height <= 0 || width > 128 || height > 128) {
        return false;
    }
    const QImage image(bgra, width, height, width * 4, QImage::Format_ARGB32);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.copy().save(&buffer, "PNG")) {
        return false;
    }
    png->assign(bytes.begin(), bytes.end());
    return !png->empty();
}

PictureMsg WindowCapture::grab(HWND hwnd, bool operable, int jpegQuality, int longEdge) const
{
    PictureMsg empty;
    empty.operable = operable ? 1 : 0;
    if (!hwnd || !IsWindow(hwnd)) {
        return empty;
    }
    CommBytes bgra;
    int width = 0;
    int height = 0;
    RECT rect;
    GetClientRect(hwnd, &rect);
    empty.clientW = rect.right > rect.left ? static_cast<uint32_t>(rect.right - rect.left) : 0;
    empty.clientH = rect.bottom > rect.top ? static_cast<uint32_t>(rect.bottom - rect.top) : 0;
    if (!grabBgra(hwnd, &bgra, &width, &height)) {
        return empty;
    }
    return encode(bgra, width, height, true, operable, jpegQuality, longEdge);
}

/**
 * @class MonitorPick
 * @brief onMonitor 的出入参：按枚举顺序找第 want 块显示器
 */
struct MonitorPick {
    int want;       ///< 要找的显示器序号，从 0 起
    int seen;       ///< 已枚举的块数
    RECT rect;      ///< 找到的显示器在虚拟屏幕上的矩形
    bool found;     ///< 是否找到
};

BOOL CALLBACK WindowCapture::onMonitor(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM param)
{
    (void)monitor;
    (void)dc;
    MonitorPick* pick = reinterpret_cast<MonitorPick*>(param);
    if (pick->seen == pick->want) {
        pick->rect = *rect;
        pick->found = true;
    }
    pick->seen += 1;
    return TRUE;
}

int WindowCapture::monitorCount()
{
    const int count = GetSystemMetrics(SM_CMONITORS);
    return count < 1 ? 1 : (count > 255 ? 255 : count);
}

PictureMsg WindowCapture::grabDesktop(int monitor, bool operable, int* originX, int* originY,
    int jpegQuality, int longEdgeLimit) const
{
    PictureMsg empty;
    empty.operable = operable ? 1 : 0;
    MonitorPick pick;
    pick.want = monitor < 0 ? 0 : monitor;
    pick.seen = 0;
    pick.found = false;
    ZeroMemory(&pick.rect, sizeof(pick.rect));
    if (monitor == 255) {
        pick.rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        pick.rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        pick.rect.right = pick.rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        pick.rect.bottom = pick.rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
        pick.found = pick.rect.right > pick.rect.left && pick.rect.bottom > pick.rect.top;
    } else {
        EnumDisplayMonitors(0, 0, onMonitor, reinterpret_cast<LPARAM>(&pick));
    }
    if (!pick.found) {
        return empty;
    }
    const int width = pick.rect.right - pick.rect.left;
    const int height = pick.rect.bottom - pick.rect.top;
    if (originX) {
        *originX = pick.rect.left;
    }
    if (originY) {
        *originY = pick.rect.top;
    }
    empty.clientW = width > 0 ? static_cast<uint32_t>(width) : 0;
    empty.clientH = height > 0 ? static_cast<uint32_t>(height) : 0;
    if (width <= 0 || height <= 0) {
        return empty;
    }
    if (longEdgeLimit < 320) {
        longEdgeLimit = RemoteConst::kLongEdge;
    }
    HDC screen = GetDC(nullptr);
    if (!screen) {
        return empty;
    }
    int drawW = width;
    int drawH = height;
    const int longEdge = width > height ? width : height;
    if (longEdge > longEdgeLimit) {
        const double scale = static_cast<double>(longEdgeLimit) / static_cast<double>(longEdge);
        drawW = static_cast<int>(width * scale);
        drawH = static_cast<int>(height * scale);
        if (drawW < 1) {
            drawW = 1;
        }
        if (drawH < 1) {
            drawH = 1;
        }
    }
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = drawW;
    info.bmiHeader.biHeight = -drawH;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (dib) {
            DeleteObject(dib);
        }
        ReleaseDC(nullptr, screen);
        return empty;
    }
    HDC mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, dib);
    SetStretchBltMode(mem, COLORONCOLOR);
    BOOL drawn = StretchBlt(mem, 0, 0, drawW, drawH, screen, pick.rect.left, pick.rect.top, width, height, SRCCOPY | CAPTUREBLT);
    if (!drawn) {
        drawn = StretchBlt(mem, 0, 0, drawW, drawH, screen, pick.rect.left, pick.rect.top, width, height, SRCCOPY);
    }
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    if (!drawn) {
        DeleteObject(dib);
        return empty;
    }
    CommBytes bgra(static_cast<size_t>(drawW) * static_cast<size_t>(drawH) * 4);
    std::memcpy(&bgra[0], bits, bgra.size());
    DeleteObject(dib);
    PictureMsg shot = encode(bgra, drawW, drawH, true, operable, jpegQuality, longEdgeLimit);
    shot.clientW = static_cast<uint32_t>(width);
    shot.clientH = static_cast<uint32_t>(height);
    return shot;
}

}
