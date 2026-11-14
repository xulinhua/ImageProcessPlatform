/**
 * @file DeskDelta.cpp
 * @brief 桌面差分：平移用搬移，纯色用填充，只有新像素才压 JPEG
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeskDelta.h"

#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace Vision {

namespace {

const int kTile = 32;

}

void DeskDelta::putU8(CommBytes* out, uint8_t v)
{
    out->push_back(v);
}

void DeskDelta::putU16(CommBytes* out, uint16_t v)
{
    out->push_back(static_cast<unsigned char>(v & 0xff));
    out->push_back(static_cast<unsigned char>((v >> 8) & 0xff));
}

void DeskDelta::putI16(CommBytes* out, int v)
{
    putU16(out, static_cast<uint16_t>(v));
}

void DeskDelta::putU32(CommBytes* out, uint32_t v)
{
    putU16(out, static_cast<uint16_t>(v & 0xffff));
    putU16(out, static_cast<uint16_t>((v >> 16) & 0xffff));
}

bool DeskDelta::takeU8(const CommBytes& in, size_t* off, uint8_t* v)
{
    if (*off >= in.size()) {
        return false;
    }
    *v = in[*off];
    *off += 1;
    return true;
}

bool DeskDelta::takeU16(const CommBytes& in, size_t* off, uint16_t* v)
{
    if (*off + 2 > in.size()) {
        return false;
    }
    *v = static_cast<uint16_t>(in[*off] | (in[*off + 1] << 8));
    *off += 2;
    return true;
}

bool DeskDelta::takeI16(const CommBytes& in, size_t* off, int* v)
{
    uint16_t u = 0;
    if (!takeU16(in, off, &u)) {
        return false;
    }
    *v = static_cast<int16_t>(u);
    return true;
}

bool DeskDelta::takeU32(const CommBytes& in, size_t* off, uint32_t* v)
{
    uint16_t lo = 0;
    uint16_t hi = 0;
    if (!takeU16(in, off, &lo) || !takeU16(in, off, &hi)) {
        return false;
    }
    *v = static_cast<uint32_t>(lo) | (static_cast<uint32_t>(hi) << 16);
    return true;
}

DeskDelta::DeskDelta()
    : m_w(0)
    , m_h(0)
    , m_gdi(false)
{
}

void DeskDelta::reset()
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_prev.clear();
    m_w = 0;
    m_h = 0;
}

bool DeskDelta::ensureGdi()
{
    if (m_gdi) {
        return true;
    }
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    m_gdi = Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok;
    return m_gdi;
}

bool DeskDelta::encodeJpeg(const unsigned char* bgra, int width, int height, int quality, CommBytes* jpeg)
{
    if (!jpeg || !bgra || width <= 0 || height <= 0 || !ensureGdi()) {
        return false;
    }
    if (quality < 1) {
        quality = 1;
    }
    if (quality > 100) {
        quality = 100;
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
    std::vector<unsigned char> buf(size);
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
    ULONG q = static_cast<ULONG>(quality);
    params.Parameter[0].Value = &q;
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
    jpeg->assign(static_cast<const unsigned char*>(ptr), static_cast<const unsigned char*>(ptr) + bytes);
    GlobalUnlock(mem);
    stream->Release();
    return !jpeg->empty();
}

bool DeskDelta::decodeJpeg(const unsigned char* jpeg, int bytes, CommBytes* bgra, int* width, int* height)
{
    if (!jpeg || bytes <= 0 || !bgra || !width || !height || !ensureGdi()) {
        return false;
    }
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(bytes));
    if (!mem) {
        return false;
    }
    void* ptr = GlobalLock(mem);
    if (!ptr) {
        GlobalFree(mem);
        return false;
    }
    memcpy(ptr, jpeg, static_cast<size_t>(bytes));
    GlobalUnlock(mem);
    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(mem, TRUE, &stream) != S_OK || !stream) {
        GlobalFree(mem);
        return false;
    }
    Gdiplus::Bitmap bitmap(stream);
    stream->Release();
    if (bitmap.GetLastStatus() != Gdiplus::Ok || bitmap.GetWidth() == 0 || bitmap.GetHeight() == 0) {
        return false;
    }
    const int w = static_cast<int>(bitmap.GetWidth());
    const int h = static_cast<int>(bitmap.GetHeight());
    Gdiplus::BitmapData bits;
    Gdiplus::Rect rect(0, 0, w, h);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bits) != Gdiplus::Ok) {
        return false;
    }
    bgra->resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u);
    for (int y = 0; y < h; ++y) {
        const unsigned char* row = static_cast<const unsigned char*>(bits.Scan0) + static_cast<size_t>(y) * bits.Stride;
        memcpy(bgra->data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4u, row, static_cast<size_t>(w) * 4u);
    }
    bitmap.UnlockBits(&bits);
    *width = w;
    *height = h;
    return true;
}

bool DeskDelta::sameBlock(const unsigned char* prev, const unsigned char* cur, int imgW, int imgH,
    int x, int y, int ox, int oy, int w, int h) const
{
    const int sx = x + ox;
    const int sy = y + oy;
    if (x < 0 || y < 0 || sx < 0 || sy < 0 || w <= 0 || h <= 0) {
        return false;
    }
    if (x + w > imgW || y + h > imgH || sx + w > imgW || sy + h > imgH) {
        return false;
    }
    const int stride = imgW * 4;
    for (int row = 0; row < h; ++row) {
        const unsigned char* a = prev + static_cast<size_t>(sy + row) * stride + static_cast<size_t>(sx) * 4u;
        const unsigned char* b = cur + static_cast<size_t>(y + row) * stride + static_cast<size_t>(x) * 4u;
        if (memcmp(a, b, static_cast<size_t>(w) * 4u) != 0) {
            return false;
        }
    }
    return true;
}

int DeskDelta::probe(const unsigned char* prev, const unsigned char* cur, int imgW, int imgH,
    int x, int y, int ox, int oy, int w, int h) const
{
    const int sx = x + ox;
    const int sy = y + oy;
    if (sx < 0 || sy < 0 || sx + w > imgW || sy + h > imgH) {
        return 1 << 30;
    }
    int score = 0;
    const int stride = imgW * 4;
    for (int gy = 0; gy < 4; ++gy) {
        for (int gx = 0; gx < 4; ++gx) {
            const int py = y + (h * gy) / 4;
            const int px = x + (w * gx) / 4;
            const unsigned char* a = prev + (static_cast<size_t>(py + oy) * stride) + static_cast<size_t>(px + ox) * 4u;
            const unsigned char* b = cur + (static_cast<size_t>(py) * stride) + static_cast<size_t>(px) * 4u;
            score += abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]);
        }
    }
    return score;
}

bool DeskDelta::locate(const unsigned char* prev, const unsigned char* cur, int imgW, int imgH,
    int x, int y, int w, int h, int* ox, int* oy) const
{
    int cx = 0;
    int cy = 0;
    const int steps[6] = {64, 32, 16, 8, 4, 1};
    const int dir[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (int s = 0; s < 6; ++s) {
        const int step = steps[s];
        int best = probe(prev, cur, imgW, imgH, x, y, cx, cy, w, h);
        int bx = cx;
        int by = cy;
        for (int d = 0; d < 8; ++d) {
            const int nx = cx + dir[d][0] * step;
            const int ny = cy + dir[d][1] * step;
            const int score = probe(prev, cur, imgW, imgH, x, y, nx, ny, w, h);
            if (score < best) {
                best = score;
                bx = nx;
                by = ny;
            }
        }
        cx = bx;
        cy = by;
        if (best == 0) {
            break;
        }
    }
    if (!sameBlock(prev, cur, imgW, imgH, x, y, cx, cy, w, h)) {
        return false;
    }
    *ox = cx;
    *oy = cy;
    return true;
}

bool DeskDelta::solidBlock(const unsigned char* cur, int imgW, int x, int y, int w, int h, uint32_t* color) const
{
    const int stride = imgW * 4;
    const unsigned char* origin = cur + static_cast<size_t>(y) * stride + static_cast<size_t>(x) * 4u;
    const unsigned char b = origin[0];
    const unsigned char g = origin[1];
    const unsigned char r = origin[2];
    for (int row = 0; row < h; ++row) {
        const unsigned char* line = cur + static_cast<size_t>(y + row) * stride + static_cast<size_t>(x) * 4u;
        for (int col = 0; col < w; ++col) {
            if (line[col * 4] != b || line[col * 4 + 1] != g || line[col * 4 + 2] != r) {
                return false;
            }
        }
    }
    if (color) {
        *color = static_cast<uint32_t>(b) | (static_cast<uint32_t>(g) << 8)
            | (static_cast<uint32_t>(r) << 16) | 0xff000000u;
    }
    return true;
}

int DeskDelta::sharpQuality(int quality) const
{
    int q = quality > 0 ? quality : 82;
    if (q < 78) {
        q = 78;
    }
    if (q > 90) {
        q = 90;
    }
    return q;
}

bool DeskDelta::writeKeyframe(const unsigned char* bgra, int width, int height, int quality, CommBytes* payload)
{
    CommBytes jpeg;
    const int q = sharpQuality(quality);
    if (!encodeJpeg(bgra, width, height, q, &jpeg)) {
        return false;
    }
    if (jpeg.size() > 1600 * 1024 && q > 78) {
        CommBytes smaller;
        if (encodeJpeg(bgra, width, height, 78, &smaller) && !smaller.empty() && smaller.size() < jpeg.size()) {
            jpeg.swap(smaller);
        }
    }
    payload->clear();
    putU8(payload, 1);
    putU8(payload, 1);
    payload->insert(payload->end(), jpeg.begin(), jpeg.end());
    m_prev.assign(bgra, bgra + static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    m_w = width;
    m_h = height;
    return true;
}

void DeskDelta::blit(const CommBytes& src, CommBytes* dst, int imgW, int imgH, int sx, int sy, int dx, int dy, int w, int h) const
{
    if (sx < 0) {
        w += sx;
        dx -= sx;
        sx = 0;
    }
    if (sy < 0) {
        h += sy;
        dy -= sy;
        sy = 0;
    }
    if (dx < 0) {
        w += dx;
        sx -= dx;
        dx = 0;
    }
    if (dy < 0) {
        h += dy;
        sy -= dy;
        dy = 0;
    }
    if (w <= 0 || h <= 0 || sx + w > imgW || dx + w > imgW || sy + h > imgH || dy + h > imgH) {
        return;
    }
    const int stride = imgW * 4;
    for (int row = 0; row < h; ++row) {
        const unsigned char* from = src.data() + static_cast<size_t>(sy + row) * stride + static_cast<size_t>(sx) * 4u;
        unsigned char* to = dst->data() + static_cast<size_t>(dy + row) * stride + static_cast<size_t>(dx) * 4u;
        memcpy(to, from, static_cast<size_t>(w) * 4u);
    }
}

void DeskDelta::fill(CommBytes* dst, int imgW, int imgH, int x, int y, int w, int h, uint32_t color) const
{
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > imgW || y + h > imgH) {
        return;
    }
    unsigned char px[4];
    px[0] = static_cast<unsigned char>(color & 0xff);
    px[1] = static_cast<unsigned char>((color >> 8) & 0xff);
    px[2] = static_cast<unsigned char>((color >> 16) & 0xff);
    px[3] = 0xff;
    const int stride = imgW * 4;
    for (int row = 0; row < h; ++row) {
        unsigned char* line = dst->data() + static_cast<size_t>(y + row) * stride + static_cast<size_t>(x) * 4u;
        for (int col = 0; col < w; ++col) {
            memcpy(line + static_cast<size_t>(col) * 4u, px, 4);
        }
    }
}

bool DeskDelta::encode(const unsigned char* bgra, int width, int height, int quality, CommBytes* payload, bool* key)
{
    if (!bgra || !payload || width <= 0 || height <= 0 || width > 8192 || height > 8192) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    if (key) {
        *key = false;
    }
    const size_t need = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    if (m_prev.size() != need || m_w != width || m_h != height) {
        if (key) {
            *key = true;
        }
        return writeKeyframe(bgra, width, height, quality, payload);
    }
    struct Cell {
        int x;
        int y;
        int w;
        int h;
        int ox;
        int oy;
        uint32_t color;
        uint8_t kind;
        Cell() : x(0), y(0), w(0), h(0), ox(0), oy(0), color(0), kind(0) {}
    };
    const int cols = (width + kTile - 1) / kTile;
    const int rows = (height + kTile - 1) / kTile;
    const int count = cols * rows;
    std::vector<Cell> cells(static_cast<size_t>(count));
    std::vector<int> changed;
    changed.reserve(static_cast<size_t>(count / 4));
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            Cell& cell = cells[static_cast<size_t>(row * cols + col)];
            cell.x = col * kTile;
            cell.y = row * kTile;
            cell.w = (std::min)(kTile, width - cell.x);
            cell.h = (std::min)(kTile, height - cell.y);
            cell.kind = sameBlock(m_prev.data(), bgra, width, height, cell.x, cell.y, 0, 0, cell.w, cell.h) ? 0 : 3;
            if (cell.kind == 3) {
                changed.push_back(row * cols + col);
            }
        }
    }
    if (changed.empty()) {
        return false;
    }
    std::map<std::pair<int, int>, int> votes;
    const int samples = (std::min)(static_cast<int>(changed.size()), 24);
    for (int i = 0; i < samples; ++i) {
        const Cell& cell = cells[static_cast<size_t>(changed[static_cast<size_t>((i * changed.size()) / samples)])];
        uint32_t color = 0;
        if (solidBlock(bgra, width, cell.x, cell.y, cell.w, cell.h, &color)) {
            continue;
        }
        int ox = 0;
        int oy = 0;
        if (!locate(m_prev.data(), bgra, width, height, cell.x, cell.y, cell.w, cell.h, &ox, &oy)) {
            continue;
        }
        if (ox == 0 && oy == 0) {
            continue;
        }
        votes[std::make_pair(ox, oy)] += 1;
    }
    int bestOx = 0;
    int bestOy = 0;
    int bestVotes = 0;
    for (std::map<std::pair<int, int>, int>::const_iterator it = votes.begin(); it != votes.end(); ++it) {
        if (it->second > bestVotes) {
            bestVotes = it->second;
            bestOx = it->first.first;
            bestOy = it->first.second;
        }
    }
    if (bestVotes >= 4) {
        for (size_t i = 0; i < changed.size(); ++i) {
            Cell& cell = cells[static_cast<size_t>(changed[i])];
            if (sameBlock(m_prev.data(), bgra, width, height, cell.x, cell.y, bestOx, bestOy, cell.w, cell.h)) {
                cell.kind = 1;
                cell.ox = bestOx;
                cell.oy = bestOy;
            }
        }
    }
    int left = 0;
    for (size_t i = 0; i < changed.size(); ++i) {
        Cell& cell = cells[static_cast<size_t>(changed[i])];
        if (cell.kind != 3) {
            continue;
        }
        uint32_t color = 0;
        if (solidBlock(bgra, width, cell.x, cell.y, cell.w, cell.h, &color)) {
            cell.kind = 2;
            cell.color = color;
            continue;
        }
        left += 1;
    }
    if (left > (count * 85) / 100 && bestVotes < 4) {
        if (key) {
            *key = true;
        }
        return writeKeyframe(bgra, width, height, quality, payload);
    }
    int searched = 0;
    for (size_t i = 0; i < changed.size() && searched < 320; ++i) {
        Cell& cell = cells[static_cast<size_t>(changed[i])];
        if (cell.kind != 3) {
            continue;
        }
        int ox = 0;
        int oy = 0;
        searched += 1;
        if (locate(m_prev.data(), bgra, width, height, cell.x, cell.y, cell.w, cell.h, &ox, &oy)
            && !(ox == 0 && oy == 0)) {
            cell.kind = 1;
            cell.ox = ox;
            cell.oy = oy;
        }
    }
    std::vector<uint8_t> used(static_cast<size_t>(count), 0);
    struct Region {
        int x;
        int y;
        int w;
        int h;
        int ox;
        int oy;
        uint32_t color;
        uint8_t kind;
    };
    std::vector<Region> regions;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const int index = row * cols + col;
            if (used[static_cast<size_t>(index)] || cells[static_cast<size_t>(index)].kind == 0) {
                continue;
            }
            const Cell& seed = cells[static_cast<size_t>(index)];
            int endCol = col;
            while (endCol + 1 < cols) {
                const Cell& next = cells[static_cast<size_t>(row * cols + endCol + 1)];
                if (used[static_cast<size_t>(row * cols + endCol + 1)] || next.kind != seed.kind
                    || next.ox != seed.ox || next.oy != seed.oy || next.color != seed.color
                    || next.y != seed.y || next.h != seed.h
                    || next.x != cells[static_cast<size_t>(row * cols + endCol)].x + cells[static_cast<size_t>(row * cols + endCol)].w) {
                    break;
                }
                endCol += 1;
            }
            int endRow = row;
            bool grew = true;
            while (grew && endRow + 1 < rows) {
                grew = false;
                bool rowOk = true;
                for (int c = col; c <= endCol; ++c) {
                    const int below = (endRow + 1) * cols + c;
                    const Cell& next = cells[static_cast<size_t>(below)];
                    if (used[static_cast<size_t>(below)] || next.kind != seed.kind || next.ox != seed.ox
                        || next.oy != seed.oy || next.color != seed.color || next.w != cells[static_cast<size_t>(row * cols + c)].w) {
                        rowOk = false;
                        break;
                    }
                }
                if (rowOk) {
                    endRow += 1;
                    grew = true;
                }
            }
            Region region;
            region.x = seed.x;
            region.y = seed.y;
            region.w = cells[static_cast<size_t>(row * cols + endCol)].x + cells[static_cast<size_t>(row * cols + endCol)].w - seed.x;
            region.h = cells[static_cast<size_t>(endRow * cols + col)].y + cells[static_cast<size_t>(endRow * cols + col)].h - seed.y;
            region.ox = seed.ox;
            region.oy = seed.oy;
            region.color = seed.color;
            region.kind = seed.kind;
            regions.push_back(region);
            for (int r = row; r <= endRow; ++r) {
                for (int c = col; c <= endCol; ++c) {
                    used[static_cast<size_t>(r * cols + c)] = 1;
                }
            }
        }
    }
    int patchPixels = 0;
    for (size_t i = 0; i < regions.size(); ++i) {
        if (regions[i].kind == 3) {
            patchPixels += regions[i].w * regions[i].h;
        }
    }
    if (patchPixels > (width * height * 72) / 100 || regions.size() > 480) {
        if (key) {
            *key = true;
        }
        return writeKeyframe(bgra, width, height, quality, payload);
    }
    payload->clear();
    putU8(payload, 1);
    putU8(payload, 0);
    const size_t countAt = payload->size();
    putU16(payload, 0);
    int ops = 0;
    const int stride = width * 4;
    for (size_t i = 0; i < regions.size(); ++i) {
        const Region& region = regions[i];
        if (region.kind == 1) {
            putU8(payload, 1);
            putI16(payload, region.ox);
            putI16(payload, region.oy);
            putU16(payload, static_cast<uint16_t>(region.x));
            putU16(payload, static_cast<uint16_t>(region.y));
            putU16(payload, static_cast<uint16_t>(region.w));
            putU16(payload, static_cast<uint16_t>(region.h));
            ops += 1;
        } else if (region.kind == 2) {
            putU8(payload, 2);
            putU16(payload, static_cast<uint16_t>(region.x));
            putU16(payload, static_cast<uint16_t>(region.y));
            putU16(payload, static_cast<uint16_t>(region.w));
            putU16(payload, static_cast<uint16_t>(region.h));
            putU32(payload, region.color);
            ops += 1;
        } else if (region.kind == 3) {
            CommBytes packed(static_cast<size_t>(region.w) * static_cast<size_t>(region.h) * 4u);
            for (int y = 0; y < region.h; ++y) {
                memcpy(packed.data() + static_cast<size_t>(y) * static_cast<size_t>(region.w) * 4u,
                    bgra + static_cast<size_t>(region.y + y) * stride + static_cast<size_t>(region.x) * 4u,
                    static_cast<size_t>(region.w) * 4u);
            }
            CommBytes jpeg;
            if (!encodeJpeg(packed.data(), region.w, region.h, sharpQuality(quality), &jpeg)) {
                if (key) {
                    *key = true;
                }
                return writeKeyframe(bgra, width, height, quality, payload);
            }
            putU8(payload, 3);
            putU16(payload, static_cast<uint16_t>(region.x));
            putU16(payload, static_cast<uint16_t>(region.y));
            putU16(payload, static_cast<uint16_t>(region.w));
            putU16(payload, static_cast<uint16_t>(region.h));
            putU32(payload, static_cast<uint32_t>(jpeg.size()));
            payload->insert(payload->end(), jpeg.begin(), jpeg.end());
            ops += 1;
        }
    }
    if (ops == 0) {
        payload->clear();
        return false;
    }
    (*payload)[countAt] = static_cast<unsigned char>(ops & 0xff);
    (*payload)[countAt + 1] = static_cast<unsigned char>((ops >> 8) & 0xff);
    m_prev.assign(bgra, bgra + need);
    if (key) {
        *key = false;
    }
    return true;
}

bool DeskDelta::apply(const CommBytes& payload, CommBytes* bgra, int* width, int* height)
{
    if (!bgra || payload.size() < 2) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    if (payload[0] != 1) {
        return false;
    }
    if ((payload[1] & 1) != 0) {
        int w = 0;
        int h = 0;
        CommBytes frame;
        if (!decodeJpeg(payload.data() + 2, static_cast<int>(payload.size() - 2), &frame, &w, &h)) {
            return false;
        }
        m_prev.swap(frame);
        m_w = w;
        m_h = h;
        *bgra = m_prev;
        if (width) {
            *width = w;
        }
        if (height) {
            *height = h;
        }
        return true;
    }
    if (m_prev.empty() || m_w <= 0 || m_h <= 0) {
        return false;
    }
    size_t off = 2;
    uint16_t ops = 0;
    if (!takeU16(payload, &off, &ops) || ops > 400) {
        return false;
    }
    CommBytes next = m_prev;
    const CommBytes base = m_prev;
    for (uint16_t n = 0; n < ops; ++n) {
        uint8_t type = 0;
        if (!takeU8(payload, &off, &type)) {
            return false;
        }
        if (type == 1) {
            int ox = 0;
            int oy = 0;
            uint16_t x = 0;
            uint16_t y = 0;
            uint16_t w = 0;
            uint16_t h = 0;
            if (!takeI16(payload, &off, &ox) || !takeI16(payload, &off, &oy)
                || !takeU16(payload, &off, &x) || !takeU16(payload, &off, &y)
                || !takeU16(payload, &off, &w) || !takeU16(payload, &off, &h)) {
                return false;
            }
            blit(base, &next, m_w, m_h, static_cast<int>(x) + ox, static_cast<int>(y) + oy,
                static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h));
        } else if (type == 2) {
            uint16_t x = 0;
            uint16_t y = 0;
            uint16_t w = 0;
            uint16_t h = 0;
            uint32_t color = 0;
            if (!takeU16(payload, &off, &x) || !takeU16(payload, &off, &y)
                || !takeU16(payload, &off, &w) || !takeU16(payload, &off, &h) || !takeU32(payload, &off, &color)) {
                return false;
            }
            fill(&next, m_w, m_h, static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h), color);
        } else if (type == 3) {
            uint16_t x = 0;
            uint16_t y = 0;
            uint16_t w = 0;
            uint16_t h = 0;
            uint32_t len = 0;
            if (!takeU16(payload, &off, &x) || !takeU16(payload, &off, &y)
                || !takeU16(payload, &off, &w) || !takeU16(payload, &off, &h) || !takeU32(payload, &off, &len)
                || off + len > payload.size()) {
                return false;
            }
            CommBytes patch;
            int pw = 0;
            int ph = 0;
            if (!decodeJpeg(payload.data() + off, static_cast<int>(len), &patch, &pw, &ph)) {
                return false;
            }
            off += len;
            const int copyW = (std::min)(pw, static_cast<int>(w));
            const int copyH = (std::min)(ph, static_cast<int>(h));
            for (int row = 0; row < copyH; ++row) {
                if (static_cast<int>(y) + row >= m_h || static_cast<int>(x) >= m_w) {
                    break;
                }
                const int room = m_w - static_cast<int>(x);
                const int nbyte = (std::min)(copyW, room) * 4;
                memcpy(next.data() + (static_cast<size_t>(y) + static_cast<size_t>(row)) * static_cast<size_t>(m_w) * 4u
                        + static_cast<size_t>(x) * 4u,
                    patch.data() + static_cast<size_t>(row) * static_cast<size_t>(pw) * 4u,
                    static_cast<size_t>(nbyte));
            }
        } else {
            return false;
        }
    }
    m_prev.swap(next);
    *bgra = m_prev;
    if (width) {
        *width = m_w;
    }
    if (height) {
        *height = m_h;
    }
    return true;
}

}
