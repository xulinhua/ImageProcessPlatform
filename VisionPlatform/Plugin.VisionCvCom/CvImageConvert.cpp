/**
 * @file CvImageConvert.cpp
 * @brief CvImageConvert实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvImageConvert.h"

#include <cstring>

namespace Vision {

// 是否为可直接拆成 BGR 的交错编码
static bool isInterleavedRgbFamily(HsCommData::PixelEncoding enc)
{
    return enc == HsCommData::EncodeRgb
        || enc == HsCommData::EncodeBgr
        || enc == HsCommData::EncodeRgba
        || enc == HsCommData::EncodeBgra;
}

// HsImage 转灰度 Mat（彩色按亮度折灰后拷贝）
bool hsImageToMatGray(const HsCommData::HsImage& img, cv::Mat*& out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = "Mat is null";
        }
        return false;
    }
    if (img.isNull() || img.width <= 0 || img.height <= 0) {
        if (err) {
            *err = "empty image";
        }
        return false;
    }
    cv::Mat gray(img.height, img.width, CV_8UC1);
    for (int y = 0; y < img.height; ++y) {
        unsigned char* dst = gray.ptr<unsigned char>(y);
        for (int x = 0; x < img.width; ++x) {
            double g = 0;
            if (!img.sampleGray(x, y, &g)) {
                g = 0;
            }
            dst[x] = static_cast<unsigned char>(g + 0.5);
        }
    }
    *out = gray;
    return true;
}

// HsImage 转 Mat。单通道→灰度；三/四通道交错→BGR
bool hsImageToMat(const HsCommData::HsImage& img, cv::Mat*& out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = "Mat is null";
        }
        return false;
    }
    if (img.isNull() || img.width <= 0 || img.height <= 0) {
        if (err) {
            *err = "empty image";
        }
        return false;
    }
    if (img.encoding == HsCommData::EncodeMono || !isInterleavedRgbFamily(img.encoding)) {
        return hsImageToMatGray(img, out, err);
    }
    cv::Mat bgr(img.height, img.width, CV_8UC3);
    for (int y = 0; y < img.height; ++y) {
        unsigned char* dst = bgr.ptr<unsigned char>(y);
        for (int x = 0; x < img.width; ++x) {
            int rr = 0;
            int gg = 0;
            int bb = 0;
            if (!img.sampleRgb(x, y, &rr, &gg, &bb)) {
                rr = gg = bb = 0;
            }
            if (rr < 0) rr = 0;
            if (rr > 255) rr = 255;
            if (gg < 0) gg = 0;
            if (gg > 255) gg = 255;
            if (bb < 0) bb = 0;
            if (bb > 255) bb = 255;
            dst[x * 3 + 0] = static_cast<unsigned char>(bb);
            dst[x * 3 + 1] = static_cast<unsigned char>(gg);
            dst[x * 3 + 2] = static_cast<unsigned char>(rr);
        }
    }
    *out = bgr;
    return true;
}

// Mat 转 HsImage。1 通道→mono8；3 通道 BGR→rgb8
bool matToHsImage(const cv::Mat& img, int64_t frameId, HsCommData::HsImage*& out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = "HsImage is null";
        }
        return false;
    }
    if (img.empty() || img.cols <= 0 || img.rows <= 0) {
        if (err) {
            *err = "empty Mat";
        }
        return false;
    }
    // 先统一到 8 位单通道或 8 位 BGR，四通道丢掉 alpha
    cv::Mat work;
    if (img.type() == CV_8UC1 || img.type() == CV_8UC3) {
        work = img;
    } else if (img.channels() == 1) {
        img.convertTo(work, CV_8UC1);
    } else {
        cv::Mat tmp;
        if (img.channels() == 3) {
            img.convertTo(tmp, CV_8UC3);
        } else if (img.channels() == 4) {
            cv::Mat bgr;
            cv::cvtColor(img, bgr, cv::COLOR_BGRA2BGR);
            bgr.convertTo(tmp, CV_8UC3);
        } else {
            if (err) {
                *err = "unsupported Mat channels";
            }
            return false;
        }
        work = tmp;
    }
    // BGR 逐像素换成 RGB 顺序；单通道按行整段拷贝
    if (work.type() == CV_8UC3) {
        HsCommData::HsImage fb = HsCommData::HsImage::make(work.cols, work.rows,
                                                            HsCommData::PixelByte, HsCommData::EncodeRgb, frameId);
        for (int y = 0; y < work.rows; ++y) {
            const unsigned char* src = work.ptr<unsigned char>(y);
            unsigned char* dst = fb.scanLine(y);
            for (int x = 0; x < work.cols; ++x) {
                dst[x * 3 + 0] = src[x * 3 + 2];
                dst[x * 3 + 1] = src[x * 3 + 1];
                dst[x * 3 + 2] = src[x * 3 + 0];
            }
        }
        *out = fb;
        return true;
    }
    HsCommData::HsImage fb = HsCommData::HsImage::make(work.cols, work.rows,
                                                        HsCommData::PixelByte, HsCommData::EncodeMono, frameId);
    for (int y = 0; y < work.rows; ++y) {
        memcpy(fb.scanLine(y), work.ptr(y), static_cast<size_t>(work.cols));
    }
    *out = fb;
    return true;
}

}
