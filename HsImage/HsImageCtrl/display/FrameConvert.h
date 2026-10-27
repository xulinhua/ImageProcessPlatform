/**
 * @file FrameConvert.h
 * @brief FrameConvert头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCTRL_FRAMECONVERT_H
#define HSCTRL_FRAMECONVERT_H

#include <HsCommData/HsCommData.h>

#include <QImage>
#include <QSize>
#include <QString>

namespace HsCtrl {

/** @brief 像素尺寸。与 HsCommData::HsImage 宽高一致，供 Qt 使用 */
inline QSize bufferPixelSize(const HsCommData::HsImage& fb)
{
    return QSize(fb.width, fb.height);
}

/**
 * @brief QImage 转显示缓冲。灰度为 byte+Mono，彩色为 byte+RGB
 * @param frameId 帧号，由调用方传入
 */
HsCommData::HsImage frameFromQImage(const QImage& img, int64_t frameId);

/** @brief 显示缓冲转 QImage。byte 的 BGR/BGRA 等转成 Qt 可画格式 */
QImage qImageFromFrame(const HsCommData::HsImage& fb);

}

#endif
