/**
 * @file QtConv.cpp
 * @brief QtConv实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "QtConv.h"

namespace Vision {

// HsImage 转 QImage（历史缩略图等）
QImage toQImage(const HsCommData::HsImage& frame)
{
    if (frame.isNull() || frame.kind != HsCommData::PixelByte) {
        return QImage();
    }
    QImage::Format fmt = QImage::Format_Invalid;
    if (frame.encoding == HsCommData::EncodeMono) {
        fmt = QImage::Format_Grayscale8;
    } else if (frame.encoding == HsCommData::EncodeRgb) {
        fmt = QImage::Format_RGB888;
    } else if (frame.encoding == HsCommData::EncodeRgba) {
        fmt = QImage::Format_RGBA8888;
    } else if (frame.encoding == HsCommData::EncodeBgr) {
        QImage img(frame.width, frame.height, QImage::Format_RGB888);
        for (int y = 0; y < frame.height; ++y) {
            const unsigned char* src = frame.scanLine(y);
            unsigned char* dst = img.scanLine(y);
            for (int x = 0; x < frame.width; ++x) {
                dst[x * 3] = src[x * 3 + 2];
                dst[x * 3 + 1] = src[x * 3 + 1];
                dst[x * 3 + 2] = src[x * 3];
            }
        }
        return img;
    } else if (frame.encoding == HsCommData::EncodeBgra) {
        QImage img(frame.width, frame.height, QImage::Format_RGBA8888);
        for (int y = 0; y < frame.height; ++y) {
            const unsigned char* src = frame.scanLine(y);
            unsigned char* dst = img.scanLine(y);
            for (int x = 0; x < frame.width; ++x) {
                dst[x * 4] = src[x * 4 + 2];
                dst[x * 4 + 1] = src[x * 4 + 1];
                dst[x * 4 + 2] = src[x * 4];
                dst[x * 4 + 3] = src[x * 4 + 3];
            }
        }
        return img;
    } else {
        return QImage();
    }
    QImage img(frame.width, frame.height, fmt);
    const int bpp = frame.bytesPerPixel();
    for (int y = 0; y < frame.height; ++y) {
        memcpy(img.scanLine(y), frame.scanLine(y),
               static_cast<size_t>(frame.width) * static_cast<size_t>(bpp));
    }
    return img;
}

// QImage 转 HsImage
HsCommData::HsImage fromQImage(const QImage& img, int64_t frameId)
{
    if (img.isNull()) {
        return HsCommData::HsImage();
    }
    QImage src = img;
    HsCommData::PixelEncoding enc = HsCommData::EncodeRgb;
    if (src.format() == QImage::Format_Grayscale8) {
        enc = HsCommData::EncodeMono;
    } else if (src.format() == QImage::Format_RGB888) {
        enc = HsCommData::EncodeRgb;
    } else if (src.format() == QImage::Format_RGBA8888 || src.format() == QImage::Format_ARGB32) {
        src = src.convertToFormat(QImage::Format_RGBA8888);
        enc = HsCommData::EncodeRgba;
    } else {
        src = src.convertToFormat(QImage::Format_RGB888);
        enc = HsCommData::EncodeRgb;
    }
    HsCommData::HsImage fb = HsCommData::HsImage::make(src.width(), src.height(),
                                                       HsCommData::PixelByte, enc, frameId);
    const int bpp = fb.bytesPerPixel();
    for (int y = 0; y < fb.height; ++y) {
        memcpy(fb.scanLine(y), src.constScanLine(y),
               static_cast<size_t>(fb.width) * static_cast<size_t>(bpp));
    }
    return fb;
}

// 相机有生效比例尺时写成毫米，否则像素1:1
HsCommData::ImageCalib imageCalibForCamera(const ObjectId& cameraId)
{
    HsCommData::ImageCalib calib;
    if (cameraId.isNull()) {
        return calib;
    }
    const CalibRst result = CalibMgr::instance().active(cameraId);
    if (!result.plane.present || result.plane.status != CalibValid || result.plane.mmPerPixel <= 0.0) {
        return calib;
    }
    HsCommData::UnitFactor factor;
    factor.unit = HsCommData::UnitMm;
    factor.xPerPixel = result.plane.mmPerPixel;
    factor.yPerPixel = result.plane.mmPerPixel;
    calib.factors.push_back(factor);
    calib.show.clear();
    calib.show.push_back(HsCommData::UnitMm);
    return calib;
}

}
