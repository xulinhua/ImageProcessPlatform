/**
 * @file FrameConvert.cpp
 * @brief FrameConvert实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FrameConvert.h"

#include <cstring>

namespace HsCtrl {

namespace {

// 按行拷贝像素，源步长与目标行字节数可以不同
void copyRows(QImage* dst, const unsigned char* src, int srcStride, int rowBytes)
{
    for (int y = 0; y < dst->height(); ++y) {
        memcpy(dst->scanLine(y), src + static_cast<size_t>(y) * static_cast<size_t>(srcStride),
               static_cast<size_t>(rowBytes));
    }
}

} // namespace

// QImage 转显示缓冲：灰度保持，其余转成 RGB888
HsCommData::HsImage frameFromQImage(const QImage& img, int64_t frameId)
{
    if (img.isNull() || img.width() <= 0 || img.height() <= 0) {
        return HsCommData::HsImage();
    }
    if (img.format() == QImage::Format_Grayscale8) {
        HsCommData::HsImage fb = HsCommData::HsImage::make(img.width(), img.height(), HsCommData::PixelByte, HsCommData::EncodeMono, frameId);
        const int rowBytes = img.width();
        for (int y = 0; y < img.height(); ++y) {
            memcpy(fb.scanLine(y), img.constScanLine(y), static_cast<size_t>(rowBytes));
        }
        return fb;
    }
    const QImage rgb = (img.format() == QImage::Format_RGB888)
        ? img
        : img.convertToFormat(QImage::Format_RGB888);
    if (rgb.isNull()) {
        return HsCommData::HsImage();
    }
    HsCommData::HsImage fb = HsCommData::HsImage::make(rgb.width(), rgb.height(), HsCommData::PixelByte, HsCommData::EncodeRgb, frameId);
    const int rowBytes = rgb.width() * 3;
    for (int y = 0; y < rgb.height(); ++y) {
        memcpy(fb.scanLine(y), rgb.constScanLine(y), static_cast<size_t>(rowBytes));
    }
    return fb;
}

// 显示缓冲转 QImage，未知 byte 编码走逐点取样
QImage qImageFromFrame(const HsCommData::HsImage& fb)
{
    if (fb.isNull()) {
        return QImage();
    }
    // 8 位灰、RGB、RGBA 与 QImage 格式一致，按行拷贝
    if (fb.kind == HsCommData::PixelByte && fb.encoding == HsCommData::EncodeMono) {
        QImage img(fb.width, fb.height, QImage::Format_Grayscale8);
        copyRows(&img, fb.pixels.data(), fb.stride, fb.width);
        return img;
    }
    if (fb.kind == HsCommData::PixelByte && fb.encoding == HsCommData::EncodeRgb) {
        QImage img(fb.width, fb.height, QImage::Format_RGB888);
        copyRows(&img, fb.pixels.data(), fb.stride, fb.width * 3);
        return img;
    }
    if (fb.kind == HsCommData::PixelByte && fb.encoding == HsCommData::EncodeRgba) {
        QImage img(fb.width, fb.height, QImage::Format_RGBA8888);
        copyRows(&img, fb.pixels.data(), fb.stride, fb.width * 4);
        return img;
    }
    // BGR / BGRA 逐像素换成 RGB888，BGRA 的透明度丢弃
    if (fb.kind == HsCommData::PixelByte && (fb.encoding == HsCommData::EncodeBgr || fb.encoding == HsCommData::EncodeBgra)) {
        QImage img(fb.width, fb.height, QImage::Format_RGB888);
        const int step = (fb.encoding == HsCommData::EncodeBgr) ? 3 : 4;
        for (int y = 0; y < fb.height; ++y) {
            const unsigned char* s = fb.scanLine(y);
            unsigned char* d = img.scanLine(y);
            for (int x = 0; x < fb.width; ++x) {
                d[x * 3] = s[x * step + 2];
                d[x * 3 + 1] = s[x * step + 1];
                d[x * 3 + 2] = s[x * step];
            }
        }
        return img;
    }
    // 其它 8 位编码（YUV、Bayer 等）逐点取 RGB，取不到填黑；非 8 位返回空图
    if (fb.kind == HsCommData::PixelByte) {
        QImage img(fb.width, fb.height, QImage::Format_RGB888);
        for (int y = 0; y < fb.height; ++y) {
            unsigned char* d = img.scanLine(y);
            for (int x = 0; x < fb.width; ++x) {
                int r = 0;
                int g = 0;
                int b = 0;
                if (!fb.sampleRgb(x, y, &r, &g, &b)) {
                    r = 0;
                    g = 0;
                    b = 0;
                }
                d[x * 3] = static_cast<unsigned char>(r);
                d[x * 3 + 1] = static_cast<unsigned char>(g);
                d[x * 3 + 2] = static_cast<unsigned char>(b);
            }
        }
        return img;
    }
    return QImage();
}

}
