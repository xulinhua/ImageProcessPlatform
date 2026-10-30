/**
 * @file Viewport.h
 * @brief Viewport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_CANVAS_VIEWPORT_H
#define HSIMAGE_CANVAS_VIEWPORT_H

#include <HsCtrl/HsImgCtrlExport.h>

#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QTransform>
#include <QtMath>

namespace HsCtrl {

/**
 * @class Viewport
 * @brief 视口。part 是图像坐标系里的可见矩形，缩放和平移只改它。
 */
class HSCTRL_EXPORT Viewport {
public:
    QRectF part;      ///< 可见图像矩形
    QSize  imageSize; ///< 底图像素尺寸

    Viewport(); ///< 空视口

    /** @brief 底图尺寸与 part 均有效 */
    bool isValid() const;
    /** @brief 整幅图等比装入窗口，更新 part */
    void fit(const QSize& viewportSize);
    /** @brief 保持中心，使 part 宽高比等于窗口 */
    void syncAspect(const QSize& viewportSize);
    /**
     * @brief 以图像点为锚缩放 part，不重采样位图
     * @param imagePt 锚点的图像坐标
     * @param relInPart 锚点在 part 内的相对位置，0–1
     * @param factor >1 放大
     */
    void zoomAt(const QPointF& imagePt, const QPointF& relInPart, qreal factor, const QSize& viewportSize);
    /** @brief 平移 part，增量为图像坐标 */
    void panImage(const QPointF& imageDelta);
    /** @brief 场景到视口的 QTransform */
    QTransform viewTransform(const QSize& viewportSize) const;
    /** @brief 屏幕像素 / 图像像素 */
    qreal scale(const QSize& viewportSize) const;
};

/** @brief 场景坐标落到像素格，ROI 取样用 floor */
inline QPoint sceneToPixel(const QPointF& scenePt)
{
    return QPoint(qFloor(scenePt.x()), qFloor(scenePt.y()));
}

/** @brief 设备边取整。减去一个很小的量再 ceil，避免浮点贴边多出一像素 */
inline int deviceEdge(qreal v)
{
    return qCeil(v - qreal(1e-6));
}

/** @brief 图像像素 (x,y) 在设备上的矩形，供 HUD 网格和像素检验 */
inline QRect pixelDeviceRect(const QTransform& xf, int x, int y)
{
    const int x0 = deviceEdge(xf.map(QPointF(x, y)).x());
    const int y0 = deviceEdge(xf.map(QPointF(x, y)).y());
    const int x1 = deviceEdge(xf.map(QPointF(x + 1, y)).x());
    const int y1 = deviceEdge(xf.map(QPointF(x, y + 1)).y());
    return QRect(x0, y0, qMax(1, x1 - x0), qMax(1, y1 - y0));
}

}

#endif
