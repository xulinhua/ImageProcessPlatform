/**
 * @file HalConvert.cpp
 * @brief HalConvert实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalConvert.h"
#include <cstring>

namespace Vision {

// 把 Halcon 异常写成 UTF-8 说明
static void setHalconErr(std::string* err, const HalconCpp::HException& e)
{
    if (!err) {
        return;
    }
    const char* t = e.ErrorMessage().Text();
    *err = t ? std::string(t) : std::string("Halcon error");
}

// 先折成连续 Gray8，再交给 Halcon 拷贝一份，避免借用 FrameBuffer 指针
bool frameToHImageGray(const HsCommData::HsImage& fb, HalconCpp::HImage* out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = "HImage is null";
        }
        return false;
    }
    if (fb.isNull() || fb.width <= 0 || fb.height <= 0) {
        if (err) {
            *err = "empty frame";
        }
        return false;
    }
    HsCommData::HsImage gray = HsCommData::HsImage::make(fb.width, fb.height, HsCommData::PixelByte,
                                                        HsCommData::EncodeMono, fb.frameId);
    if (gray.pixels.empty()) {
        if (err) {
            *err = "alloc gray failed";
        }
        return false;
    }
    for (int y = 0; y < fb.height; ++y) {
        unsigned char* dst = gray.scanLine(y);
        for (int x = 0; x < fb.width; ++x) {
            double g = 0;
            if (!fb.sampleGray(x, y, &g)) {
                g = 0;
            }
            dst[x] = static_cast<unsigned char>(g + 0.5);
        }
    }
    try {
        // 自有缓冲写入 GenImageConst，避免 GenImage1 外挂指针在拷贝前被优化/失效
        HalconCpp::HImage tmp;
        tmp.GenImageConst("byte", fb.width, fb.height);
        HalconCpp::HString type;
        Hlong gw = 0;
        Hlong gh = 0;
        void* ptr = tmp.GetImagePointer1(&type, &gw, &gh);
        if (!ptr || gw != fb.width || gh != fb.height) {
            if (err) {
                *err = "GenImageConst failed";
            }
            return false;
        }
        const size_t n = static_cast<size_t>(fb.width) * static_cast<size_t>(fb.height);
        memcpy(ptr, &gray.pixels[0], n);
        *out = tmp;
        return true;
    } catch (const HalconCpp::HException& e) {
        setHalconErr(err, e);
        return false;
    }
}

// 按通道数写成 Gray8 或 RGB888；非 byte 先转类型
bool hImageToFrame(const HalconCpp::HImage& img, int64_t frameId, HsCommData::HsImage* out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = "FrameBuffer is null";
        }
        return false;
    }
    try {
        HalconCpp::HImage work = img;
        const Hlong nch = work.CountChannels().L();
        if (nch <= 0) {
            if (err) {
                *err = "empty HImage";
            }
            return false;
        }
        HalconCpp::HString type;
        Hlong w = 0;
        Hlong h = 0;
        if (nch >= 3) {
            void* pr = nullptr;
            void* pg = nullptr;
            void* pb = nullptr;
            work.GetImagePointer3(&pr, &pg, &pb, &type, &w, &h);
            if (!(type == HalconCpp::HString("byte"))) {
                work = work.ConvertImageType("byte");
                work.GetImagePointer3(&pr, &pg, &pb, &type, &w, &h);
            }
            if (w <= 0 || h <= 0 || !pr || !pg || !pb) {
                if (err) {
                    *err = "empty HImage";
                }
                return false;
            }
            HsCommData::HsImage fb = HsCommData::HsImage::make(static_cast<int>(w), static_cast<int>(h),
                                                                HsCommData::PixelByte, HsCommData::EncodeRgb, frameId);
            const unsigned char* r = static_cast<const unsigned char*>(pr);
            const unsigned char* g = static_cast<const unsigned char*>(pg);
            const unsigned char* b = static_cast<const unsigned char*>(pb);
            const int n = static_cast<int>(w) * static_cast<int>(h);
            for (int i = 0; i < n; ++i) {
                unsigned char* p = &fb.pixels[static_cast<size_t>(i) * 3];
                p[0] = r[i];
                p[1] = g[i];
                p[2] = b[i];
            }
            *out = fb;
            return true;
        }
        void* ptr = work.GetImagePointer1(&type, &w, &h);
        if (!(type == HalconCpp::HString("byte"))) {
            work = work.ConvertImageType("byte");
            ptr = work.GetImagePointer1(&type, &w, &h);
        }
        if (w <= 0 || h <= 0) {
            if (err) {
                *err = "empty HImage";
            }
            return false;
        }
        HsCommData::HsImage fb = HsCommData::HsImage::make(static_cast<int>(w), static_cast<int>(h),
                                                            HsCommData::PixelByte, HsCommData::EncodeMono, frameId);
        const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
        if (ptr && n > 0 && !fb.pixels.empty()) {
            memcpy(&fb.pixels[0], ptr, n);
        }
        *out = fb;
        return true;
    } catch (const HalconCpp::HException& e) {
        setHalconErr(err, e);
        return false;
    }
}

}
