/**
 * @file HalImageConvert.cpp
 * @brief HalImageConvert实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalImageConvert.h"

#include "AlgLibKind.h"
#include "TextEncoding.h"
#include <cstdint>
#include <cstring>
#include <vector>

namespace Vision {

namespace {

// 把 Halcon 异常写入 err（可空）
void setHalconErr(std::string* err, const HalconCpp::HException& e)
{
    if (!err) {
        return;
    }
    *err = hExceptionText(e);
}

void setErr(std::string* err, const char* msg)
{
    if (err) {
        *err = msg ? msg : "";
    }
}

int channelCount(HsCommData::PixelEncoding enc)
{
    if (enc == HsCommData::EncodeMono) {
        return 1;
    }
    if (enc == HsCommData::EncodeRgb || enc == HsCommData::EncodeBgr) {
        return 3;
    }
    if (enc == HsCommData::EncodeRgba || enc == HsCommData::EncodeBgra) {
        return 4;
    }
    return 0;
}

// 行方向连续时直接用原缓冲，否则按行收成一块
bool contiguousOrPack(const HsCommData::HsImage& img, int rowBytes, const unsigned char** ptr,
                      std::vector<unsigned char>* pack)
{
    if (img.stride == rowBytes) {
        *ptr = &img.pixels[0];
        return true;
    }
    if (img.stride < rowBytes) {
        return false;
    }
    pack->resize(static_cast<size_t>(rowBytes) * static_cast<size_t>(img.height));
    for (int y = 0; y < img.height; ++y) {
        std::memcpy(&(*pack)[static_cast<size_t>(y) * static_cast<size_t>(rowBytes)],
                    img.scanLine(y), static_cast<size_t>(rowBytes));
    }
    *ptr = &(*pack)[0];
    return true;
}

// byte 单通道 / 三通道 / 四通道，对齐 Machine ConvertMat2Halcon
bool byteToHObject(const HsCommData::HsImage& img, HalconCpp::HObject* out, std::string* err)
{
    if (img.pixels.empty()) {
        setErr(err, "empty image");
        return false;
    }
    const Hlong ptr = static_cast<Hlong>(reinterpret_cast<uintptr_t>(&img.pixels[0]));
    switch (img.encoding) {
    case HsCommData::EncodeMono:
        HalconCpp::GenImage1(out, "byte", img.width, img.height, ptr);
        return true;
    case HsCommData::EncodeRgb:
        HalconCpp::GenImageInterleaved(out, ptr, "rgb", img.width, img.height, 0, "byte",
                                       img.width, img.height, 0, 0, -1, 0);
        return true;
    case HsCommData::EncodeBgr:
        HalconCpp::GenImageInterleaved(out, ptr, "bgr", img.width, img.height, 0, "byte",
                                       img.width, img.height, 0, 0, -1, 0);
        return true;
    case HsCommData::EncodeRgba:
        HalconCpp::GenImageInterleaved(out, ptr, "rgbx", img.width, img.height, 0, "byte",
                                       img.width, img.height, 0, 0, -1, 0);
        return true;
    case HsCommData::EncodeBgra:
        HalconCpp::GenImageInterleaved(out, ptr, "bgrx", img.width, img.height, 0, "byte",
                                       img.width, img.height, 0, 0, -1, 0);
        return true;
    case HsCommData::EncodeRgb565:
        HalconCpp::GenImageInterleaved(out, ptr, "rgb565", img.width, img.height, 0, "byte",
                                       img.width, img.height, 0, 0, -1, 0);
        return true;
    case HsCommData::EncodeBgr565:
        HalconCpp::GenImageInterleaved(out, ptr, "bgr565", img.width, img.height, 0, "byte",
                                       img.width, img.height, 0, 0, -1, 0);
        return true;
    default:
        setErr(err, "unsupported byte encoding");
        return false;
    }
}

const char* halconTypeOf(HsCommData::PixelKind kind)
{
    switch (kind) {
    case HsCommData::PixelInt16:
        return "int2";
    case HsCommData::PixelUInt16:
        return "uint2";
    case HsCommData::PixelInt32:
        return "int4";
    case HsCommData::PixelReal32:
        return "real";
    case HsCommData::PixelComplex64:
        return "complex";
    default:
        return 0;
    }
}

// 交错三平面拆开。rOff/gOff/bOff 是样本下标
void splitPlanes(const unsigned char* src, int stride, int w, int h, int sb, int ch,
                 int rOff, int gOff, int bOff,
                 std::vector<unsigned char>* r, std::vector<unsigned char>* g, std::vector<unsigned char>* b)
{
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(sb);
    r->assign(n, 0);
    g->assign(n, 0);
    b->assign(n, 0);
    for (int y = 0; y < h; ++y) {
        const unsigned char* row = src + static_cast<size_t>(y) * static_cast<size_t>(stride);
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = row + static_cast<size_t>(x) * static_cast<size_t>(ch) * static_cast<size_t>(sb);
            const size_t dst = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * static_cast<size_t>(sb);
            std::memcpy(&(*r)[dst], p + static_cast<size_t>(rOff) * static_cast<size_t>(sb), static_cast<size_t>(sb));
            std::memcpy(&(*g)[dst], p + static_cast<size_t>(gOff) * static_cast<size_t>(sb), static_cast<size_t>(sb));
            std::memcpy(&(*b)[dst], p + static_cast<size_t>(bOff) * static_cast<size_t>(sb), static_cast<size_t>(sb));
        }
    }
}

// real64 收成 float 再交给 Halcon（没有 double 图像类型）
bool real64ToHObject(const HsCommData::HsImage& img, HalconCpp::HObject* out, std::string* err)
{
    const int ch = channelCount(img.encoding);
    if (ch != 1 && ch != 3 && ch != 4) {
        setErr(err, "unsupported real64 encoding");
        return false;
    }
    const int useCh = ch == 1 ? 1 : 3;
    int rOff = 0;
    int gOff = 1;
    int bOff = 2;
    if (img.encoding == HsCommData::EncodeBgr || img.encoding == HsCommData::EncodeBgra) {
        rOff = 2;
        bOff = 0;
    }
    if (ch == 1) {
        std::vector<float> buf(static_cast<size_t>(img.width) * static_cast<size_t>(img.height));
        for (int y = 0; y < img.height; ++y) {
            const unsigned char* row = img.scanLine(y);
            for (int x = 0; x < img.width; ++x) {
                double v = 0.0;
                std::memcpy(&v, row + static_cast<size_t>(x) * 8, 8);
                buf[static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)] = static_cast<float>(v);
            }
        }
        HalconCpp::GenImage1(out, "real", img.width, img.height, static_cast<Hlong>(reinterpret_cast<uintptr_t>(&buf[0])));
        return true;
    }
    std::vector<float> r(static_cast<size_t>(img.width) * static_cast<size_t>(img.height));
    std::vector<float> g(r.size());
    std::vector<float> b(r.size());
    const int offs[3] = { rOff, gOff, bOff };
    std::vector<float>* planes[3] = { &r, &g, &b };
    for (int y = 0; y < img.height; ++y) {
        const unsigned char* row = img.scanLine(y);
        for (int x = 0; x < img.width; ++x) {
            const unsigned char* p = row + static_cast<size_t>(x) * static_cast<size_t>(ch) * 8;
            const size_t i = static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x);
            for (int c = 0; c < useCh; ++c) {
                double v = 0.0;
                std::memcpy(&v, p + static_cast<size_t>(offs[c]) * 8, 8);
                (*planes[c])[i] = static_cast<float>(v);
            }
        }
    }
    HalconCpp::GenImage3(out, "real", img.width, img.height,
                         static_cast<Hlong>(reinterpret_cast<uintptr_t>(&r[0])),
                         static_cast<Hlong>(reinterpret_cast<uintptr_t>(&g[0])),
                         static_cast<Hlong>(reinterpret_cast<uintptr_t>(&b[0])));
    return true;
}

bool typedToHObject(const HsCommData::HsImage& img, HalconCpp::HObject* out, std::string* err)
{
    if (img.kind == HsCommData::PixelReal64) {
        return real64ToHObject(img, out, err);
    }
    const char* ht = halconTypeOf(img.kind);
    if (!ht) {
        setErr(err, "unsupported pixel kind");
        return false;
    }
    // 复数图只支持单通道；行有填充时先压成紧凑缓冲再交给 Halcon 拷贝
    if (img.kind == HsCommData::PixelComplex64) {
        if (img.encoding != HsCommData::EncodeMono) {
            setErr(err, "complex image must be mono");
            return false;
        }
        const unsigned char* ptr = nullptr;
        std::vector<unsigned char> pack;
        if (!contiguousOrPack(img, img.width * 8, &ptr, &pack)) {
            setErr(err, "bad stride");
            return false;
        }
        HalconCpp::GenImage1(out, "complex", img.width, img.height, static_cast<Hlong>(reinterpret_cast<uintptr_t>(ptr)));
        return true;
    }
    const int sb = HsCommData::HsImage::sampleBytes(img.kind);
    const int ch = channelCount(img.encoding);
    if (sb <= 0 || (ch != 1 && ch != 3 && ch != 4)) {
        setErr(err, "unsupported encoding");
        return false;
    }
    if (ch == 1) {
        const unsigned char* ptr = nullptr;
        std::vector<unsigned char> pack;
        if (!contiguousOrPack(img, img.width * sb, &ptr, &pack)) {
            setErr(err, "bad stride");
            return false;
        }
        HalconCpp::GenImage1(out, ht, img.width, img.height, static_cast<Hlong>(reinterpret_cast<uintptr_t>(ptr)));
        return true;
    }
    // 16 位彩色走交错接口，四通道时忽略 alpha
    if (img.kind == HsCommData::PixelUInt16) {
        const unsigned char* ptr = nullptr;
        std::vector<unsigned char> pack;
        if (!contiguousOrPack(img, img.width * ch * sb, &ptr, &pack)) {
            setErr(err, "bad stride");
            return false;
        }
        const char* fmt = "rgb48";
        if (img.encoding == HsCommData::EncodeBgr) {
            fmt = "bgr48";
        } else if (img.encoding == HsCommData::EncodeRgba) {
            fmt = "rgbx64";
        } else if (img.encoding == HsCommData::EncodeBgra) {
            fmt = "bgrx64";
        }
        HalconCpp::GenImageInterleaved(out, static_cast<Hlong>(reinterpret_cast<uintptr_t>(ptr)),
                                       fmt, img.width, img.height, 0, "uint2",
                                       img.width, img.height, 0, 0, -1, 0);
        return true;
    }
    // 其余彩色按 R、G、B 拆成三个平面，BGR 系交换 R/B 偏移
    int rOff = 0;
    int bOff = 2;
    if (img.encoding == HsCommData::EncodeBgr || img.encoding == HsCommData::EncodeBgra) {
        rOff = 2;
        bOff = 0;
    }
    std::vector<unsigned char> r;
    std::vector<unsigned char> g;
    std::vector<unsigned char> b;
    splitPlanes(&img.pixels[0], img.stride, img.width, img.height, sb, ch, rOff, 1, bOff, &r, &g, &b);
    HalconCpp::GenImage3(out, ht, img.width, img.height,
                         static_cast<Hlong>(reinterpret_cast<uintptr_t>(&r[0])),
                         static_cast<Hlong>(reinterpret_cast<uintptr_t>(&g[0])),
                         static_cast<Hlong>(reinterpret_cast<uintptr_t>(&b[0])));
    return true;
}

bool sameType(const HalconCpp::HString& type, const char* name)
{
    const char* t = type.Text();
    return t && name && std::strcmp(t, name) == 0;
}

bool kindFromHalconType(const HalconCpp::HString& type, HsCommData::PixelKind* kind, int* sb)
{
    if (sameType(type, "byte") || sameType(type, "direction") || sameType(type, "cyclic")) {
        *kind = HsCommData::PixelByte;
        *sb = 1;
        return true;
    }
    if (sameType(type, "int2")) {
        *kind = HsCommData::PixelInt16;
        *sb = 2;
        return true;
    }
    if (sameType(type, "uint2")) {
        *kind = HsCommData::PixelUInt16;
        *sb = 2;
        return true;
    }
    if (sameType(type, "int4")) {
        *kind = HsCommData::PixelInt32;
        *sb = 4;
        return true;
    }
    if (sameType(type, "real")) {
        *kind = HsCommData::PixelReal32;
        *sb = 4;
        return true;
    }
    if (sameType(type, "complex")) {
        *kind = HsCommData::PixelComplex64;
        *sb = 8;
        return true;
    }
    return false;
}

} // namespace

// Halcon 异常转说明字符串
std::string hExceptionText(const HalconCpp::HException& e)
{
    const char* t = e.ErrorMessage().Text();
    return t ? std::string(t) : std::string("Halcon error");
}

// HsImage 转 HObject
bool hsImageToHObject(const HsCommData::HsImage& img, HalconCpp::HObject*& out, std::string* err)
{
    if (!out) {
        setErr(err, "HObject is null");
        return false;
    }
    if (img.isNull() || img.width <= 0 || img.height <= 0) {
        setErr(err, "empty image");
        return false;
    }
    try {
        HalconCpp::GenEmptyObj(out);
        if (img.kind == HsCommData::PixelByte) {
            return byteToHObject(img, out, err);
        }
        return typedToHObject(img, out, err);
    } catch (const HalconCpp::HException& e) {
        setHalconErr(err, e);
        return false;
    }
}

// HObject 转 HsImage。单通道 GetImagePointer1；三通道 InterleaveChannels 收成 rgb
bool hObjectToHsImage(const HalconCpp::HObject& himg, int64_t frameId, HsCommData::HsImage*& out, std::string* err)
{
    if (!out) {
        setErr(err, "HsImage is null");
        return false;
    }
    try {
        HalconCpp::HTuple ch;
        HalconCpp::CountChannels(himg, &ch);
        if (ch.Length() < 1 || ch[0].L() < 1) {
            setErr(err, "empty HObject");
            return false;
        }
        switch (ch[0].L()) {
        // 单通道：int1 扩成 int16，其余类型按字节整块拷贝
        case 1: {
            HalconCpp::HTuple pointer;
            HalconCpp::HTuple type;
            HalconCpp::HTuple w;
            HalconCpp::HTuple h;
            HalconCpp::GetImagePointer1(himg, &pointer, &type, &w, &h);
            if (type.Length() < 1 || w.Length() < 1 || h.Length() < 1 || pointer.Length() < 1) {
                setErr(err, "empty HObject");
                return false;
            }
            const int iw = static_cast<int>(w[0].L());
            const int ih = static_cast<int>(h[0].L());
            if (iw <= 0 || ih <= 0) {
                setErr(err, "empty HObject");
                return false;
            }
            const void* src = reinterpret_cast<const void*>(static_cast<uintptr_t>(pointer[0].L()));
            const char* tn = type[0].S().Text();
            if (tn && std::strcmp(tn, "int1") == 0) {
                HsCommData::HsImage fb = HsCommData::HsImage::make(iw, ih, HsCommData::PixelInt16,
                                                                    HsCommData::EncodeMono, frameId);
                const signed char* bytes = static_cast<const signed char*>(src);
                for (int i = 0; i < iw * ih; ++i) {
                    const int16_t v = bytes[i];
                    std::memcpy(&fb.pixels[static_cast<size_t>(i) * 2], &v, 2);
                }
                *out = fb;
                return true;
            }
            HsCommData::PixelKind kind = HsCommData::PixelByte;
            int sb = 1;
            if (!kindFromHalconType(type[0].S(), &kind, &sb)) {
                setErr(err, "unsupported image type");
                return false;
            }
            HsCommData::HsImage fb = HsCommData::HsImage::make(iw, ih, kind, HsCommData::EncodeMono, frameId);
            const size_t n = static_cast<size_t>(iw) * static_cast<size_t>(ih) * static_cast<size_t>(sb);
            if (n > 0 && !fb.pixels.empty()) {
                std::memcpy(&fb.pixels[0], src, n);
            }
            *out = fb;
            return true;
        }
        // 三通道：先交错成 rgb 再整块拷贝；其它通道数不支持
        case 3: {
            HalconCpp::HTuple type;
            HalconCpp::HTuple w;
            HalconCpp::HTuple h;
            HalconCpp::GetImageType(himg, &type);
            HalconCpp::GetImageSize(himg, &w, &h);
            if (type.Length() < 1 || w.Length() < 1 || h.Length() < 1) {
                setErr(err, "empty HObject");
                return false;
            }
            HsCommData::PixelKind kind = HsCommData::PixelByte;
            int sb = 1;
            if (!kindFromHalconType(type[0].S(), &kind, &sb)) {
                setErr(err, "unsupported image type");
                return false;
            }
            const int iw = static_cast<int>(w[0].L());
            const int ih = static_cast<int>(h[0].L());
            if (iw <= 0 || ih <= 0) {
                setErr(err, "empty HObject");
                return false;
            }
            HalconCpp::HObject interleaved;
            HalconCpp::InterleaveChannels(himg, &interleaved, "rgb", "match", 0);
            HalconCpp::HTuple pointer;
            HalconCpp::HTuple itype;
            HalconCpp::HTuple iw2;
            HalconCpp::HTuple ih2;
            HalconCpp::GetImagePointer1(interleaved, &pointer, &itype, &iw2, &ih2);
            if (pointer.Length() < 1) {
                setErr(err, "empty HObject");
                return false;
            }
            HsCommData::HsImage fb = HsCommData::HsImage::make(iw, ih, kind, HsCommData::EncodeRgb, frameId);
            const size_t n = static_cast<size_t>(iw) * static_cast<size_t>(ih) * 3 * static_cast<size_t>(sb);
            if (n > 0 && !fb.pixels.empty()) {
                std::memcpy(&fb.pixels[0], reinterpret_cast<const void*>(static_cast<uintptr_t>(pointer[0].L())), n);
            }
            *out = fb;
            return true;
        }
        default:
            setErr(err, "unsupported channel count");
            return false;
        }
    } catch (const HalconCpp::HException& e) {
        setHalconErr(err, e);
        return false;
    }
}

bool hsImageToGray(const HsCommData::HsImage& img, HalconCpp::HImage*& gray, int* w, int* h, std::string* err)
{
    if (!gray || !w || !h || img.isNull()) {
        if (err) {
            *err = "无输入图";
        }
        return false;
    }
    HalconCpp::HObject ho;
    HalconCpp::HObject* pHo = &ho;
    if (!hsImageToHObject(img, pHo, err)) {
        return false;
    }
    try {
        HalconCpp::HTuple ch;
        HalconCpp::CountChannels(ho, &ch);
        if (ch.Length() >= 1 && ch[0].L() >= 3) {
            HalconCpp::HObject grayHo;
            HalconCpp::Rgb1ToGray(ho, &grayHo);
            ho = grayHo;
        }
        *gray = HalconCpp::HImage(ho);
        HalconCpp::HTuple tw;
        HalconCpp::HTuple th;
        gray->GetImageSize(&tw, &th);
        *w = static_cast<int>(tw.L());
        *h = static_cast<int>(th.L());
        return true;
    } catch (const HalconCpp::HException& e) {
        setHalconErr(err, e);
        return false;
    }
}

bool inputToHObject(void* native, int algLib, const HsCommData::HsImage& img, HalconCpp::HObject*& out, std::string* err)
{
    if (!out) {
        setErr(err, "no input image");
        return false;
    }
    if (native != nullptr && algLib == static_cast<int>(AlgLibHal)) {
        try {
            *out = *static_cast<const HalconCpp::HObject*>(native);
            return true;
        } catch (const HalconCpp::HException& e) {
            setHalconErr(err, e);
            return false;
        }
    }
    return hsImageToHObject(img, out, err);
}

bool inputToGray(void* native, int algLib, const HsCommData::HsImage& img, HalconCpp::HObject*& gray, int* w, int* h, std::string* err)
{
    if (!gray || !w || !h) {
        setErr(err, "no input image");
        return false;
    }
    HalconCpp::HObject ho;
    HalconCpp::HObject* held = &ho;
    if (!inputToHObject(native, algLib, img, held, err)) {
        return false;
    }
    try {
        HalconCpp::HTuple ch;
        HalconCpp::CountChannels(ho, &ch);
        if (ch.Length() >= 1 && ch[0].L() >= 3) {
            HalconCpp::HObject grayHo;
            HalconCpp::Rgb1ToGray(ho, &grayHo);
            ho = grayHo;
        }
        *gray = ho;
        HalconCpp::HTuple tw;
        HalconCpp::HTuple th;
        HalconCpp::GetImageSize(*gray, &tw, &th);
        *w = static_cast<int>(tw.L());
        *h = static_cast<int>(th.L());
        return true;
    } catch (const HalconCpp::HException& e) {
        setHalconErr(err, e);
        return false;
    }
}

}
