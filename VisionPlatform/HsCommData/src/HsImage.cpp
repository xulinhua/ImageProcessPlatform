/**
 * @file HsImage.cpp
 * @brief HsImage实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsImage.h>

#include <cmath>
#include <cstring>
#include <cstdint>
#include <string>

#include <objbase.h>
#include <wincodec.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace HsCommData {

namespace {

// 编码名比较，忽略大小写
bool encEq(const char* a, const char* b)
{
    if (!a || !b) {
        return false;
    }
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

// 交错通道数。YUYV 等打包格式返回 0
int encodingSamples(PixelEncoding encoding)
{
    switch (encoding) {
    case EncodeMono:
        return 1;
    case EncodeRgb:
    case EncodeBgr:
        return 3;
    case EncodeRgba:
    case EncodeBgra:
        return 4;
    default:
        return 0;
    }
}

// BT.601 亮度，压到 0-255
int luma8(int r, int g, int b)
{
    const double y = 0.299 * r + 0.587 * g + 0.114 * b;
    if (y < 0.0) {
        return 0;
    }
    if (y > 255.0) {
        return 255;
    }
    return static_cast<int>(y + 0.5);
}

// 四舍五入成 int，并夹到 32 位范围
int roundToInt(double v)
{
    if (v >= 2147483647.0) {
        return 2147483647;
    }
    if (v <= -2147483648.0) {
        return static_cast<int>(-2147483647 - 1);
    }
    return static_cast<int>(std::floor(v + 0.5));
}

// 夹到 0-255
int clip8(double v)
{
    if (v < 0.0) {
        return 0;
    }
    if (v > 255.0) {
        return 255;
    }
    return static_cast<int>(v + 0.5);
}

// 按 kind 读一个样本。复数和不认识的 kind 返回 false
bool readSample(const unsigned char* p, PixelKind kind, double* out)
{
    switch (kind) {
    case PixelByte:
        *out = p[0];
        return true;
    case PixelInt16: {
        int16_t v = 0;
        std::memcpy(&v, p, sizeof(v));
        *out = v;
        return true;
    }
    case PixelUInt16: {
        uint16_t v = 0;
        std::memcpy(&v, p, sizeof(v));
        *out = v;
        return true;
    }
    case PixelInt32: {
        int32_t v = 0;
        std::memcpy(&v, p, sizeof(v));
        *out = v;
        return true;
    }
    case PixelReal32: {
        float v = 0.f;
        std::memcpy(&v, p, sizeof(v));
        *out = v;
        return true;
    }
    case PixelReal64: {
        double v = 0.0;
        std::memcpy(&v, p, sizeof(v));
        *out = v;
        return true;
    }
    case PixelComplex64:
        return false;
    default:
        return false;
    }
}

// 8 位 YUV 转 RGB
void yuv8ToRgb(int y, int u, int v, int* r, int* g, int* b)
{
    const double du = static_cast<double>(u) - 128.0;
    const double dv = static_cast<double>(v) - 128.0;
    const double yd = static_cast<double>(y);
    *r = clip8(yd + 1.402 * dv);
    *g = clip8(yd - 0.344136 * du - 0.714136 * dv);
    *b = clip8(yd + 1.772 * du);
}

// 4:2:2 打包：YUYV / UYVY / YVYU
bool isYuv422(PixelEncoding encoding)
{
    return encoding == EncodeYuyv || encoding == EncodeUyvy || encoding == EncodeYvyu;
}

// 4:2:0 平面。存储行数是 height 的 3/2
bool isPlanar420(PixelEncoding encoding)
{
    return encoding == EncodeNv12 || encoding == EncodeNv21
        || encoding == EncodeI420 || encoding == EncodeYv12;
}

// Bayer 起始相位
bool isBayer(PixelEncoding encoding)
{
    return encoding == EncodeBayerRG || encoding == EncodeBayerGR
        || encoding == EncodeBayerGB || encoding == EncodeBayerBG;
}

// 一行有效字节数。不合法返回 0
int rowBytesOf(int width, PixelKind kind, PixelEncoding encoding)
{
    if (width <= 0) {
        return 0;
    }
    if (isYuv422(encoding) || encoding == EncodeRgb565 || encoding == EncodeBgr565) {
        return kind == PixelByte ? width * 2 : 0;
    }
    if (encoding == EncodeYuv411) {
        return (kind == PixelByte && (width % 4) == 0) ? (width / 4) * 6 : 0;
    }
    if (encoding == EncodeYuv444 || encoding == EncodeUyv) {
        return kind == PixelByte ? width * 3 : 0;
    }
    if (isPlanar420(encoding)) {
        return (kind == PixelByte && (width % 2) == 0) ? width : 0;
    }
    if (kind == PixelComplex64) {
        return encoding == EncodeMono ? width * 8 : 0;
    }
    if (isBayer(encoding) || encoding == EncodeMono) {
        const int sb = HsImage::sampleBytes(kind);
        return sb > 0 ? width * sb : 0;
    }
    const int bpp = HsImage::bytesPerPixel(kind, encoding);
    return bpp > 0 ? width * bpp : 0;
}

// 存储行数。4:2:0 含色度平面
int storageRowsOf(int height, PixelEncoding encoding)
{
    if (height <= 0) {
        return 0;
    }
    if (isPlanar420(encoding)) {
        return (height % 2) == 0 ? height + height / 2 : 0;
    }
    return height;
}

// 0=R 1=G 2=B
int bayerChannel(PixelEncoding encoding, int x, int y)
{
    const int xe = x & 1;
    const int ye = y & 1;
    if (encoding == EncodeBayerRG) {
        if (!ye && !xe) {
            return 0;
        }
        if (ye && xe) {
            return 2;
        }
        return 1;
    }
    if (encoding == EncodeBayerGR) {
        if (!ye && xe) {
            return 0;
        }
        if (ye && !xe) {
            return 2;
        }
        return 1;
    }
    if (encoding == EncodeBayerGB) {
        if (ye && !xe) {
            return 0;
        }
        if (!ye && xe) {
            return 2;
        }
        return 1;
    }
    if (ye && xe) {
        return 0;
    }
    if (!ye && !xe) {
        return 2;
    }
    return 1;
}

// 打包或平面取 RGB。不支持返回 false。yOut 有亮度时写出，否则 -1
bool tryPackedRgb(const unsigned char* pixels, int width, int height, int stride,
                  PixelKind kind, PixelEncoding encoding, int x, int y,
                  int* r, int* g, int* b, int* yOut)
{
    *yOut = -1;
    // YUV422：两像素共用 4 字节，按偶数列对齐取一组
    if (isYuv422(encoding)) {
        if (stride < (x & ~1) * 2 + 4) {
            return false;
        }
        const unsigned char* p = pixels + static_cast<size_t>(y) * static_cast<size_t>(stride)
            + static_cast<size_t>(x & ~1) * 2;
        int yy = 0;
        int uu = 0;
        int vv = 0;
        if (encoding == EncodeUyvy) {
            uu = p[0];
            vv = p[2];
            yy = (x & 1) ? p[3] : p[1];
        } else if (encoding == EncodeYvyu) {
            yy = (x & 1) ? p[2] : p[0];
            vv = p[1];
            uu = p[3];
        } else {
            yy = (x & 1) ? p[2] : p[0];
            uu = p[1];
            vv = p[3];
        }
        *yOut = yy;
        yuv8ToRgb(yy, uu, vv, r, g, b);
        return true;
    }
    // YUV411：四像素 6 字节，布局 U Y Y V Y Y
    if (encoding == EncodeYuv411) {
        if ((x / 4) * 6 + 6 > stride) {
            return false;
        }
        const unsigned char* p = pixels + static_cast<size_t>(y) * static_cast<size_t>(stride)
            + static_cast<size_t>(x / 4) * 6;
        static const int kYIndex[4] = { 1, 2, 4, 5 };
        const int yy = p[kYIndex[x & 3]];
        *yOut = yy;
        yuv8ToRgb(yy, p[0], p[3], r, g, b);
        return true;
    }
    // YUV444 / UYV：每像素 3 字节
    if (encoding == EncodeYuv444 || encoding == EncodeUyv) {
        if (x * 3 + 3 > stride) {
            return false;
        }
        const unsigned char* p = pixels + static_cast<size_t>(y) * static_cast<size_t>(stride)
            + static_cast<size_t>(x) * 3;
        int yy = 0;
        int uu = 0;
        int vv = 0;
        if (encoding == EncodeUyv) {
            uu = p[0];
            yy = p[1];
            vv = p[2];
        } else {
            yy = p[0];
            uu = p[1];
            vv = p[2];
        }
        *yOut = yy;
        yuv8ToRgb(yy, uu, vv, r, g, b);
        return true;
    }
    // 565：16 位小端，分量扩展到 0~255
    if (encoding == EncodeRgb565 || encoding == EncodeBgr565) {
        if (x * 2 + 2 > stride) {
            return false;
        }
        const unsigned char* p = pixels + static_cast<size_t>(y) * static_cast<size_t>(stride)
            + static_cast<size_t>(x) * 2;
        uint16_t px = 0;
        std::memcpy(&px, p, sizeof(px));
        int rr = (px >> 11) & 31;
        int gg = (px >> 5) & 63;
        int bb = px & 31;
        if (encoding == EncodeBgr565) {
            const int t = rr;
            rr = bb;
            bb = t;
        }
        *r = rr * 255 / 31;
        *g = gg * 255 / 63;
        *b = bb * 255 / 31;
        return true;
    }
    // Bayer：在 3x3 邻域里各取第一个遇到的 R、G、B 样本，越界钳到边缘
    if (isBayer(encoding)) {
        const int sb = kind == PixelByte ? 1 : (kind == PixelUInt16 || kind == PixelInt16 ? 2 : 0);
        if (sb == 0) {
            return false;
        }
        int ch[3] = { 0, 0, 0 };
        int got[3] = { 0, 0, 0 };
        for (int dy = -1; dy <= 1 && !(got[0] && got[1] && got[2]); ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int xx = x + dx;
                int yy = y + dy;
                if (xx < 0) {
                    xx = 0;
                }
                if (yy < 0) {
                    yy = 0;
                }
                if (xx >= width) {
                    xx = width - 1;
                }
                if (yy >= height) {
                    yy = height - 1;
                }
                const int c = bayerChannel(encoding, xx, yy);
                if (got[c]) {
                    continue;
                }
                const unsigned char* p = pixels + static_cast<size_t>(yy) * static_cast<size_t>(stride)
                    + static_cast<size_t>(xx) * static_cast<size_t>(sb);
                double v = 0.0;
                if (!readSample(p, kind, &v)) {
                    return false;
                }
                ch[c] = roundToInt(v);
                got[c] = 1;
            }
        }
        *r = ch[0];
        *g = ch[1];
        *b = ch[2];
        return got[0] && got[1] && got[2];
    }
    // 420 平面：Y 平面后接 NV12/NV21 交错 UV，或 I420/YV12 两个半宽平面
    if (isPlanar420(encoding)) {
        if (stride < (x & ~1) + 2) {
            return false;
        }
        const unsigned char* base = pixels;
        const int yy = base[static_cast<size_t>(y) * static_cast<size_t>(stride) + static_cast<size_t>(x)];
        *yOut = yy;
        int uu = 128;
        int vv = 128;
        const int cx = x / 2;
        const int cy = y / 2;
        if (encoding == EncodeNv12 || encoding == EncodeNv21) {
            const unsigned char* uv = base + static_cast<size_t>(stride) * static_cast<size_t>(height)
                + static_cast<size_t>(cy) * static_cast<size_t>(stride) + static_cast<size_t>(cx) * 2;
            if (encoding == EncodeNv12) {
                uu = uv[0];
                vv = uv[1];
            } else {
                vv = uv[0];
                uu = uv[1];
            }
        } else {
            const int cStride = stride / 2;
            const unsigned char* plane1 = base + static_cast<size_t>(stride) * static_cast<size_t>(height);
            const unsigned char* plane2 = plane1 + static_cast<size_t>(cStride) * static_cast<size_t>(height / 2);
            const unsigned char* uPlane = encoding == EncodeI420 ? plane1 : plane2;
            const unsigned char* vPlane = encoding == EncodeI420 ? plane2 : plane1;
            uu = uPlane[static_cast<size_t>(cy) * static_cast<size_t>(cStride) + static_cast<size_t>(cx)];
            vv = vPlane[static_cast<size_t>(cy) * static_cast<size_t>(cStride) + static_cast<size_t>(cx)];
        }
        yuv8ToRgb(yy, uu, vv, r, g, b);
        return true;
    }
    return false;
}

// 复数样本的模，实部和虚部按 float 读
bool readComplexMag(const unsigned char* p, double* out)
{
    float re = 0.f;
    float im = 0.f;
    std::memcpy(&re, p, sizeof(re));
    std::memcpy(&im, p + sizeof(re), sizeof(im));
    *out = std::sqrt(static_cast<double>(re) * re + static_cast<double>(im) * im);
    return true;
}

} // namespace

// 空图
HsImage::HsImage()
    : frameId(0)
    , width(0)
    , height(0)
    , stride(0)
    , kind(PixelKindUnknown)
    , encoding(EncodeUnknown)
{
}

// 深拷像素与元数据
HsImage::HsImage(const HsImage& other)
    : frameId(other.frameId)
    , width(other.width)
    , height(other.height)
    , stride(other.stride)
    , kind(other.kind)
    , encoding(other.encoding)
    , pixels(other.pixels)
{
}

// 深拷赋值
HsImage& HsImage::operator=(const HsImage& other)
{
    if (this != &other) {
        frameId = other.frameId;
        width = other.width;
        height = other.height;
        stride = other.stride;
        kind = other.kind;
        encoding = other.encoding;
        pixels = other.pixels;
    }
    return *this;
}

// 单样本字节数。未知 kind 返回 0
int HsImage::sampleBytes(PixelKind kind)
{
    switch (kind) {
    case PixelByte:
        return 1;
    case PixelInt16:
    case PixelUInt16:
        return 2;
    case PixelInt32:
    case PixelReal32:
        return 4;
    case PixelReal64:
    case PixelComplex64:
        return 8;
    default:
        return 0;
    }
}

// 每像素字节数。422/444/565 仅 byte。411 不按像素计，返回 0
int HsImage::bytesPerPixel(PixelKind kind, PixelEncoding encoding)
{
    if (kind == PixelComplex64) {
        return encoding == EncodeMono ? 8 : 0;
    }
    if (isYuv422(encoding) || encoding == EncodeRgb565 || encoding == EncodeBgr565) {
        return kind == PixelByte ? 2 : 0;
    }
    if (encoding == EncodeYuv444 || encoding == EncodeUyv) {
        return kind == PixelByte ? 3 : 0;
    }
    if (isPlanar420(encoding) || isBayer(encoding)) {
        return sampleBytes(kind);
    }
    if (encoding == EncodeYuv411) {
        return 0;
    }
    const int sb = sampleBytes(kind);
    const int n = encodingSamples(encoding);
    if (sb <= 0 || n <= 0) {
        return 0;
    }
    return sb * n;
}

// 每像素字节数
int HsImage::bytesPerPixel() const
{
    return bytesPerPixel(kind, encoding);
}

// 宽高非法或无缓冲
bool HsImage::isNull() const
{
    return pixels.empty() || width <= 0 || height <= 0;
}

// 第 y 行只读指针。越界抛异常
const unsigned char* HsImage::scanLine(int y) const
{
    if (y < 0 || y >= height || pixels.empty()) {
        raiseException(HsException(HS_ERR_IMAGE_ROW));
    }
    return &pixels[static_cast<size_t>(y) * static_cast<size_t>(stride)];
}

// 第 y 行可写指针。越界抛异常
unsigned char* HsImage::scanLine(int y)
{
    if (y < 0 || y >= height || pixels.empty()) {
        raiseException(HsException(HS_ERR_IMAGE_ROW));
    }
    return &pixels[static_cast<size_t>(y) * static_cast<size_t>(stride)];
}

// 分配空图像，像素填零
HsImage HsImage::make(int w, int h, PixelKind kind, PixelEncoding encoding, int64_t id)
{
    HsImage img;
    img.frameId = id;
    img.width = w;
    img.height = h;
    img.kind = kind;
    img.encoding = encoding;
    const int row = rowBytesOf(w, kind, encoding);
    const int rows = storageRowsOf(h, encoding);
    img.stride = row;
    if (row > 0 && rows > 0) {
        img.pixels.assign(static_cast<size_t>(row) * static_cast<size_t>(rows), 0);
    }
    return img;
}

// 从原始缓冲按行拷贝。失败返回空图
HsImage HsImage::fromRaw(const unsigned char* data, int w, int h, int strideBytes,
                         const char* encoding, int64_t id)
{
    if (!data || !encoding || w <= 0 || h <= 0 || strideBytes <= 0) {
        return HsImage();
    }
    // 编码名映射到像素类型与排列，不认识的返回空图。mono10/12/14 按 16 位存
    PixelKind kind = PixelKindUnknown;
    PixelEncoding enc = EncodeUnknown;
    if (encEq(encoding, "mono8") || encEq(encoding, "8uc1")) {
        kind = PixelByte;
        enc = EncodeMono;
    } else if (encEq(encoding, "rgb8")) {
        kind = PixelByte;
        enc = EncodeRgb;
    } else if (encEq(encoding, "bgr8")) {
        kind = PixelByte;
        enc = EncodeBgr;
    } else if (encEq(encoding, "rgba8")) {
        kind = PixelByte;
        enc = EncodeRgba;
    } else if (encEq(encoding, "bgra8")) {
        kind = PixelByte;
        enc = EncodeBgra;
    } else if (encEq(encoding, "yuyv") || encEq(encoding, "yuy2") || encEq(encoding, "yuv422")) {
        kind = PixelByte;
        enc = EncodeYuyv;
    } else if (encEq(encoding, "uyvy")) {
        kind = PixelByte;
        enc = EncodeUyvy;
    } else if (encEq(encoding, "yvyu")) {
        kind = PixelByte;
        enc = EncodeYvyu;
    } else if (encEq(encoding, "yuv444") || encEq(encoding, "yuv")) {
        kind = PixelByte;
        enc = EncodeYuv444;
    } else if (encEq(encoding, "uyv")) {
        kind = PixelByte;
        enc = EncodeUyv;
    } else if (encEq(encoding, "yuv411") || encEq(encoding, "uyyvyy")) {
        kind = PixelByte;
        enc = EncodeYuv411;
    } else if (encEq(encoding, "rgb565")) {
        kind = PixelByte;
        enc = EncodeRgb565;
    } else if (encEq(encoding, "bgr565")) {
        kind = PixelByte;
        enc = EncodeBgr565;
    } else if (encEq(encoding, "nv12")) {
        kind = PixelByte;
        enc = EncodeNv12;
    } else if (encEq(encoding, "nv21")) {
        kind = PixelByte;
        enc = EncodeNv21;
    } else if (encEq(encoding, "i420") || encEq(encoding, "iyuv") || encEq(encoding, "yu12")) {
        kind = PixelByte;
        enc = EncodeI420;
    } else if (encEq(encoding, "yv12")) {
        kind = PixelByte;
        enc = EncodeYv12;
    } else if (encEq(encoding, "bayerrg8") || encEq(encoding, "bayerrg")) {
        kind = PixelByte;
        enc = EncodeBayerRG;
    } else if (encEq(encoding, "bayergr8") || encEq(encoding, "bayergr")) {
        kind = PixelByte;
        enc = EncodeBayerGR;
    } else if (encEq(encoding, "bayergb8") || encEq(encoding, "bayergb")) {
        kind = PixelByte;
        enc = EncodeBayerGB;
    } else if (encEq(encoding, "bayerbg8") || encEq(encoding, "bayerbg")) {
        kind = PixelByte;
        enc = EncodeBayerBG;
    } else if (encEq(encoding, "bayerrg16")) {
        kind = PixelUInt16;
        enc = EncodeBayerRG;
    } else if (encEq(encoding, "bayergr16")) {
        kind = PixelUInt16;
        enc = EncodeBayerGR;
    } else if (encEq(encoding, "bayergb16")) {
        kind = PixelUInt16;
        enc = EncodeBayerGB;
    } else if (encEq(encoding, "bayerbg16")) {
        kind = PixelUInt16;
        enc = EncodeBayerBG;
    } else if (encEq(encoding, "mono10") || encEq(encoding, "mono12") || encEq(encoding, "mono14")) {
        kind = PixelUInt16;
        enc = EncodeMono;
    } else if (encEq(encoding, "mono16")) {
        kind = PixelUInt16;
        enc = EncodeMono;
    } else if (encEq(encoding, "mono16s")) {
        kind = PixelInt16;
        enc = EncodeMono;
    } else if (encEq(encoding, "mono32s")) {
        kind = PixelInt32;
        enc = EncodeMono;
    } else if (encEq(encoding, "real32") || encEq(encoding, "mono32f")) {
        kind = PixelReal32;
        enc = EncodeMono;
    } else if (encEq(encoding, "real64") || encEq(encoding, "mono64f")) {
        kind = PixelReal64;
        enc = EncodeMono;
    } else if (encEq(encoding, "complex64") || encEq(encoding, "complex")) {
        kind = PixelComplex64;
        enc = EncodeMono;
    } else if (encEq(encoding, "rgb16")) {
        kind = PixelUInt16;
        enc = EncodeRgb;
    } else if (encEq(encoding, "bgr16")) {
        kind = PixelUInt16;
        enc = EncodeBgr;
    } else if (encEq(encoding, "rgb32f")) {
        kind = PixelReal32;
        enc = EncodeRgb;
    } else {
        return HsImage();
    }
    const int row = rowBytesOf(w, kind, enc);
    const int rows = storageRowsOf(h, enc);
    if (row <= 0 || rows <= 0 || strideBytes < row) {
        return HsImage();
    }
    HsImage img = make(w, h, kind, enc, id);
    // 420 平面整块拷贝并沿用来源行跨度；其它格式逐行拷到紧凑行
    if (isPlanar420(enc)) {
        if ((strideBytes % 2) != 0) {
            return HsImage();
        }
        if (strideBytes != img.stride) {
            img.stride = strideBytes;
            img.pixels.assign(static_cast<size_t>(strideBytes) * static_cast<size_t>(rows), 0);
        }
        std::memcpy(img.pixels.data(), data,
                    static_cast<size_t>(strideBytes) * static_cast<size_t>(rows));
        return img;
    }
    for (int y = 0; y < h; ++y) {
        std::memcpy(img.scanLine(y), data + static_cast<size_t>(y) * static_cast<size_t>(strideBytes),
                    static_cast<size_t>(row));
    }
    return img;
}

// 取灰度
bool HsImage::sampleGray(int x, int y, double* gray) const
{
    if (!gray || isNull()) {
        return false;
    }
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }
    const unsigned char* line = scanLine(y);
    if (!line) {
        return false;
    }
    // YUV、565、Bayer 等特殊排列：有亮度直接用，否则由 RGB 算
    int pr = 0;
    int pg = 0;
    int pb = 0;
    int yOut = -1;
    if (tryPackedRgb(pixels.data(), width, height, stride, kind, encoding, x, y, &pr, &pg, &pb, &yOut)) {
        if (yOut >= 0) {
            *gray = static_cast<double>(yOut);
        } else {
            *gray = static_cast<double>(luma8(pr, pg, pb));
        }
        return true;
    }
    const int bpp = bytesPerPixel();
    if (bpp <= 0) {
        return false;
    }
    if (encoding == EncodeMono) {
        if (static_cast<size_t>(x) * static_cast<size_t>(bpp) + static_cast<size_t>(bpp)
            > static_cast<size_t>(stride)) {
            return false;
        }
        const unsigned char* p = line + static_cast<size_t>(x) * static_cast<size_t>(bpp);
        if (kind == PixelComplex64) {
            return readComplexMag(p, gray);
        }
        return readSample(p, kind, gray);
    }
    // 多通道：8 位用整数亮度，其它位深按原始样本加权
    int r = 0;
    int g = 0;
    int b = 0;
    if (!sampleRgb(x, y, &r, &g, &b)) {
        return false;
    }
    if (kind == PixelByte) {
        *gray = static_cast<double>(luma8(r, g, b));
        return true;
    }
    const int n = encodingSamples(encoding);
    if (n < 3) {
        return false;
    }
    double c0 = 0.0;
    double c1 = 0.0;
    double c2 = 0.0;
    const unsigned char* p = line + static_cast<size_t>(x) * static_cast<size_t>(bpp);
    const int sb = sampleBytes(kind);
    if (!readSample(p, kind, &c0) || !readSample(p + sb, kind, &c1) || !readSample(p + sb * 2, kind, &c2)) {
        return false;
    }
    if (encoding == EncodeBgr || encoding == EncodeBgra) {
        *gray = 0.114 * c0 + 0.587 * c1 + 0.299 * c2;
    } else {
        *gray = 0.299 * c0 + 0.587 * c1 + 0.114 * c2;
    }
    return true;
}

// 取灰度。坐标向下取整，越界返回 false
bool HsImage::sampleGray(double x, double y, double* gray) const
{
    return sampleGray(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), gray);
}

// 取 RGB
bool HsImage::sampleRgb(int x, int y, int* r, int* g, int* b) const
{
    if (!r || !g || !b || isNull()) {
        return false;
    }
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }
    const unsigned char* line = scanLine(y);
    if (!line) {
        return false;
    }
    // 特殊排列先走打包解码
    int yOut = -1;
    if (tryPackedRgb(pixels.data(), width, height, stride, kind, encoding, x, y, r, g, b, &yOut)) {
        (void)yOut;
        return true;
    }
    const int bpp = bytesPerPixel();
    if (bpp <= 0) {
        return false;
    }
    if (static_cast<size_t>(x) * static_cast<size_t>(bpp) + static_cast<size_t>(bpp)
        > static_cast<size_t>(stride)) {
        return false;
    }
    // 单通道三分量同值；多通道取前三个样本，BGR 顺序要对调
    const unsigned char* p = line + static_cast<size_t>(x) * static_cast<size_t>(bpp);
    if (encoding == EncodeMono) {
        double v = 0.0;
        const bool ok = (kind == PixelComplex64) ? readComplexMag(p, &v) : readSample(p, kind, &v);
        if (!ok) {
            return false;
        }
        const int iv = roundToInt(v);
        *r = iv;
        *g = iv;
        *b = iv;
        return true;
    }
    const int n = encodingSamples(encoding);
    if (n < 3) {
        return false;
    }
    double c0 = 0.0;
    double c1 = 0.0;
    double c2 = 0.0;
    const int sb = sampleBytes(kind);
    if (!readSample(p, kind, &c0) || !readSample(p + sb, kind, &c1) || !readSample(p + sb * 2, kind, &c2)) {
        return false;
    }
    if (encoding == EncodeBgr || encoding == EncodeBgra) {
        *r = roundToInt(c2);
        *g = roundToInt(c1);
        *b = roundToInt(c0);
    } else {
        *r = roundToInt(c0);
        *g = roundToInt(c1);
        *b = roundToInt(c2);
    }
    return true;
}

// 取 RGB。坐标向下取整
bool HsImage::sampleRgb(double x, double y, int* r, int* g, int* b) const
{
    return sampleRgb(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), r, g, b);
}

namespace {

struct ReleaseCom {
    IUnknown* ptr;
    explicit ReleaseCom(IUnknown* p) : ptr(p) {}
    ~ReleaseCom() { if (ptr) ptr->Release(); }
};

struct CoInitGuard {
    bool on;
    bool ok;
    CoInitGuard() : on(false), ok(false)
    {
        const HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        on = hr == S_OK;
        ok = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
    ~CoInitGuard()
    {
        if (on) {
            CoUninitialize();
        }
    }
};

std::wstring utf8ToWidePath(const std::string& utf8)
{
    if (utf8.empty()) {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, 0, 0);
    if (n <= 0) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], n);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}

std::string asciiLower(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = static_cast<char>(s[i] - 'A' + 'a');
        }
    }
    return s;
}

std::string fileExt(const std::string& path)
{
    const size_t slash = path.find_last_of("\\/");
    const size_t name = slash == std::string::npos ? 0 : slash + 1;
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot < name || dot + 1 >= path.size()) {
        return std::string();
    }
    return asciiLower(path.substr(dot + 1));
}

bool extMatches(const std::string& ext, ImageFileFormat format)
{
    if (format == ImageFormatBmp) {
        return ext == "bmp";
    }
    if (format == ImageFormatPng) {
        return ext == "png";
    }
    if (format == ImageFormatJpeg) {
        return ext == "jpg" || ext == "jpeg";
    }
    if (format == ImageFormatTiff) {
        return ext == "tif" || ext == "tiff";
    }
    return false;
}

const char* extFor(ImageFileFormat format)
{
    if (format == ImageFormatBmp) {
        return ".bmp";
    }
    if (format == ImageFormatPng) {
        return ".png";
    }
    if (format == ImageFormatJpeg) {
        return ".jpg";
    }
    if (format == ImageFormatTiff) {
        return ".tif";
    }
    return 0;
}

int scaleTo8(int v, PixelKind kind)
{
    if (kind == PixelUInt16) {
        if (v < 0) {
            v = 0;
        }
        if (v > 65535) {
            v = 65535;
        }
        return v >> 8;
    }
    if (kind == PixelInt16 || kind == PixelInt32) {
        if (v < 0) {
            return 0;
        }
        if (v <= 255) {
            return v;
        }
        if (v <= 65535) {
            return v >> 8;
        }
        return 255;
    }
    return clip8(v);
}

int realTo8(double v)
{
    if (v >= 0.0 && v <= 1.0) {
        return clip8(static_cast<int>(v * 255.0 + 0.5));
    }
    return clip8(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
}

unsigned short realTo16(double v)
{
    double s = v;
    if (v >= 0.0 && v <= 1.0) {
        s = v * 65535.0;
    }
    if (s < 0.0) {
        s = 0.0;
    }
    if (s > 65535.0) {
        s = 65535.0;
    }
    return static_cast<unsigned short>(s + 0.5);
}

} // namespace

bool HsImage::save(const std::string& utf8Path, ImageFileFormat format, int jpegQuality) const
{
    if (isNull() || utf8Path.empty() || kind == PixelComplex64) {
        return false;
    }
    const char* suffix = extFor(format);
    if (suffix == 0) {
        return false;
    }
    const std::string ext = fileExt(utf8Path);
    std::string path = utf8Path;
    if (ext.empty()) {
        path += suffix;
    } else if (!extMatches(ext, format)) {
        return false;
    }
    const std::wstring wide = utf8ToWidePath(path);
    if (wide.empty()) {
        return false;
    }
    if (jpegQuality < 1) {
        jpegQuality = 1;
    }
    if (jpegQuality > 100) {
        jpegQuality = 100;
    }

    // 选 WIC 源格式：PNG/TIFF 的单通道高位深存 16 位灰，带透明的 8 位存 BGRA，其余转 8 位灰或 24 位 BGR
    const bool keep16 = (format == ImageFormatPng || format == ImageFormatTiff)
        && encoding == EncodeMono
        && (kind == PixelUInt16 || kind == PixelInt16 || kind == PixelReal32 || kind == PixelReal64);
    const bool keepAlpha = kind == PixelByte
        && (encoding == EncodeRgba || encoding == EncodeBgra)
        && (format == ImageFormatPng || format == ImageFormatTiff);
    const bool gray8 = encoding == EncodeMono && kind == PixelByte && !keep16;

    WICPixelFormatGUID srcFormat = GUID_WICPixelFormat24bppBGR;
    int channels = 3;
    if (keep16) {
        srcFormat = GUID_WICPixelFormat16bppGray;
        channels = 2;
    } else if (gray8) {
        srcFormat = GUID_WICPixelFormat8bppGray;
        channels = 1;
    } else if (keepAlpha) {
        srcFormat = GUID_WICPixelFormat32bppBGRA;
        channels = 4;
    }
    // 逐像素填 4 字节对齐的中间栅格
    const UINT stride = (static_cast<UINT>(width) * static_cast<UINT>(channels) + 3u) & ~3u;
    std::vector<unsigned char> raster(static_cast<size_t>(stride) * static_cast<size_t>(height), 0);
    for (int y = 0; y < height; ++y) {
        unsigned char* dst = raster.data() + static_cast<size_t>(y) * static_cast<size_t>(stride);
        for (int x = 0; x < width; ++x) {
            if (keep16) {
                double g = 0.0;
                if (!sampleGray(x, y, &g)) {
                    return false;
                }
                const unsigned short s = (kind == PixelReal32 || kind == PixelReal64) ? realTo16(g)
                    : static_cast<unsigned short>(g < 0 ? 0 : (g > 65535 ? 65535 : g));
                dst[static_cast<size_t>(x) * 2] = static_cast<unsigned char>(s);
                dst[static_cast<size_t>(x) * 2 + 1] = static_cast<unsigned char>(s >> 8);
            } else if (gray8) {
                double g = 0.0;
                if (!sampleGray(x, y, &g)) {
                    return false;
                }
                dst[x] = static_cast<unsigned char>(clip8(static_cast<int>(g + 0.5)));
            } else if (keepAlpha) {
                const unsigned char* p = scanLine(y) + static_cast<size_t>(x) * 4;
                unsigned char* q = dst + static_cast<size_t>(x) * 4;
                if (encoding == EncodeRgba) {
                    q[0] = p[2];
                    q[1] = p[1];
                    q[2] = p[0];
                    q[3] = p[3];
                } else {
                    q[0] = p[0];
                    q[1] = p[1];
                    q[2] = p[2];
                    q[3] = p[3];
                }
            } else {
                int r = 0;
                int g = 0;
                int b = 0;
                if (!sampleRgb(x, y, &r, &g, &b)) {
                    return false;
                }
                if (kind == PixelReal32 || kind == PixelReal64) {
                    r = realTo8(r);
                    g = realTo8(g);
                    b = realTo8(b);
                } else {
                    r = scaleTo8(r, kind);
                    g = scaleTo8(g, kind);
                    b = scaleTo8(b, kind);
                }
                unsigned char* q = dst + static_cast<size_t>(x) * 3;
                q[0] = static_cast<unsigned char>(b);
                q[1] = static_cast<unsigned char>(g);
                q[2] = static_cast<unsigned char>(r);
            }
        }
    }

    // WIC 编码：建工厂、文件流和编码器
    CoInitGuard com;
    if (!com.ok) {
        return false;
    }
    IWICImagingFactory* factory = 0;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || factory == 0) {
        return false;
    }
    ReleaseCom factoryRel(factory);
    IWICStream* stream = 0;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) {
        return false;
    }
    ReleaseCom streamRel(stream);
    hr = stream->InitializeFromFilename(wide.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        return false;
    }
    GUID container = GUID_ContainerFormatPng;
    if (format == ImageFormatBmp) {
        container = GUID_ContainerFormatBmp;
    } else if (format == ImageFormatJpeg) {
        container = GUID_ContainerFormatJpeg;
    } else if (format == ImageFormatTiff) {
        container = GUID_ContainerFormatTiff;
    }
    IWICBitmapEncoder* encoder = 0;
    hr = factory->CreateEncoder(container, 0, &encoder);
    if (FAILED(hr)) {
        return false;
    }
    ReleaseCom encoderRel(encoder);
    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        return false;
    }
    // 建帧，JPEG 写入质量
    IWICBitmapFrameEncode* frame = 0;
    IPropertyBag2* bag = 0;
    hr = encoder->CreateNewFrame(&frame, &bag);
    if (FAILED(hr)) {
        return false;
    }
    ReleaseCom frameRel(frame);
    ReleaseCom bagRel(bag);
    if (format == ImageFormatJpeg && bag != 0) {
        PROPBAG2 opt;
        std::memset(&opt, 0, sizeof(opt));
        opt.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT quality;
        VariantInit(&quality);
        quality.vt = VT_R4;
        quality.fltVal = static_cast<float>(jpegQuality) / 100.f;
        bag->Write(1, &opt, &quality);
        VariantClear(&quality);
    }
    hr = frame->Initialize(bag);
    if (FAILED(hr)) {
        return false;
    }
    hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    if (FAILED(hr)) {
        return false;
    }
    WICPixelFormatGUID accepted = srcFormat;
    hr = frame->SetPixelFormat(&accepted);
    if (FAILED(hr)) {
        return false;
    }
    // 编码器不接受源格式时先经格式转换器再写
    IWICBitmap* bitmap = 0;
    hr = factory->CreateBitmapFromMemory(static_cast<UINT>(width), static_cast<UINT>(height), srcFormat,
                                          stride, static_cast<UINT>(raster.size()), raster.data(), &bitmap);
    if (FAILED(hr)) {
        return false;
    }
    ReleaseCom bitmapRel(bitmap);
    if (!IsEqualGUID(accepted, srcFormat)) {
        IWICFormatConverter* converter = 0;
        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) {
            return false;
        }
        ReleaseCom converterRel(converter);
        hr = converter->Initialize(bitmap, accepted, WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            return false;
        }
        hr = frame->WriteSource(converter, 0);
    } else {
        hr = frame->WriteSource(bitmap, 0);
    }
    if (FAILED(hr)) {
        return false;
    }
    hr = frame->Commit();
    if (SUCCEEDED(hr)) {
        hr = encoder->Commit();
    }
    return SUCCEEDED(hr);
}

}
