/**
 * @file QtConv.h
 * @brief QtConv头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_QTCONV_H
#define VISION_QTCONV_H

#include "VisionUiExport.h"
#include "ObjectId.h"
#include "CalibMgr.h"
#include "ToolGeom.h"
#include <HsCtrl/HsImageWindow.h>
#include <HsCommData/HsImage.h>
#include <HsCommData/Graphic.h>
#include <QImage>
#include <QString>
#include <QVector>
#include <cstring>
#include <string>
#include <vector>

namespace Vision {

/** @brief UTF-8 std::string 转 QString */
inline QString qs(const std::string& s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

/** @brief UTF-8 std::string 转 QString */
inline std::string ss(const QString& s)
{
    const QByteArray b = s.toUtf8();
    return std::string(b.constData(), static_cast<size_t>(b.size()));
}

/** @brief ObjectId 转 QString */
inline QString qs(const ObjectId& id)
{
    return qs(id.toString());
}

/** @brief ObjectId 转 QString */
inline ObjectId oid(const QString& s)
{
    return ObjectId::fromString(ss(s));
}

/** @brief Graphic 向量转 QVector，给 HsImageWindow */
    inline QVector<HsCommData::Graphic> qg(const std::vector<HsCommData::Graphic>& v) {
    QVector<HsCommData::Graphic> o;
    o.reserve(static_cast<int>(v.size()));
    for (int i = 0; i < static_cast<int>(v.size()); ++i) {
        o.push_back(v[static_cast<size_t>(i)]);
    }
    return o;
}


/** @brief 结果几何转窗口叠加 */
inline QVector<HsCommData::Graphic> qg(const std::vector<HsCommData::HsObject>& v)
{
    return qg(objectsToGraphics(v));
}

/** @brief QVector 转 std::vector（给 HsImageWindow::setOverlays） */
    inline std::vector<HsCommData::Graphic> toStd(const QVector<HsCommData::Graphic>& v) {
    std::vector<HsCommData::Graphic> o;
    o.reserve(static_cast<size_t>(v.size()));
    for (int i = 0; i < v.size(); ++i) {
        o.push_back(v[i]);
    }
    return o;
}


/** @brief 像素排列转窗口底图编码。非 8bit 交错不在这里显示 */
    inline QString frameEncoding(const HsCommData::HsImage& fb) {
    if (fb.kind != HsCommData::PixelByte) {
        return QString();
    }
    switch (fb.encoding) {
    case HsCommData::EncodeMono: return QStringLiteral("mono8");
    case HsCommData::EncodeRgb:  return QStringLiteral("rgb8");
    case HsCommData::EncodeBgr:  return QStringLiteral("bgr8");
    case HsCommData::EncodeRgba: return QStringLiteral("rgba8");
    case HsCommData::EncodeBgra: return QStringLiteral("bgra8");
    default:                     return QString();
    }
}


/** @brief 把 HsImage 推到 HsImageWindow 底图，保留源图 frameId 供叠加对齐 */
    inline void applyFrame(HsCtrl::HsImageWindow* w, const HsCommData::HsImage& fb) {
    if (!w) {
        return;
    }
    if (fb.isNull() || fb.pixels.empty()) {
        w->clearImage();
        return;
    }
    const QString enc = frameEncoding(fb);
    if (enc.isEmpty()) {
        return;
    }
    w->setImage(fb.pixels.data(), fb.width, fb.height, fb.stride, enc, fb.frameId);
}


/** @brief HsImage 转 QImage（历史缩略图等） */
    VISION_UI_API QImage toQImage(const HsCommData::HsImage& frame);


/** @brief QImage 转 HsImage */
    VISION_UI_API HsCommData::HsImage fromQImage(const QImage& img, int64_t frameId);


/** @brief 相机有生效比例尺时写成毫米，否则像素1:1 */
    VISION_UI_API HsCommData::ImageCalib imageCalibForCamera(const ObjectId& cameraId);

}

#endif
