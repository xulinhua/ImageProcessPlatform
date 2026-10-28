/**
 * @file ImageItem.cpp
 * @brief ImageItem实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ImageItem.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

namespace HsCtrl {

// 设备面积太大时不用 DeviceCoordinateCache，避免缓存贴图过大
static const qreal kImageCacheMaxDevicePx = 2048.0 * 2048.0;

// 底图 Item，不接收鼠标，z=0
ImageItem::ImageItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_viewScale(1.0)
    , m_pixelInspect(false)
{
    setZValue(0);
    setAcceptedMouseButtons(Qt::NoButton);
    updateCacheMode();
}

// 换成新帧。像素转换留给绘制，不在换图时做成 QPixmap
void ImageItem::setFrame(const HsCommData::HsImage& frame)
{
    setImage(qImageFromFrame(frame));
}

void ImageItem::setImage(const QImage& image)
{
    prepareGeometryChange();
    m_image = image;
    m_imageSize = image.size();
    updateCacheMode();
    update();
}

// 清掉底图
void ImageItem::clear()
{
    prepareGeometryChange();
    m_image = QImage();
    m_imageSize = QSize();
    updateCacheMode();
    update();
}

// 记录视口缩放，缓存策略随它变
void ImageItem::setViewScale(qreal s)
{
    if (s <= 0.0) {
        s = 1.0;
    }
    if (qAbs(m_viewScale - s) < 1e-9) {
        updateCacheMode();
        return;
    }
    m_viewScale = s;
    updateCacheMode();
}

// 像素检验开关，用来关掉 DeviceCoordinateCache
void ImageItem::setPixelInspect(bool on)
{
    if (m_pixelInspect == on) {
        return;
    }
    m_pixelInspect = on;
    updateCacheMode();
}

// 空图、像素检验或接近 1:1 时不缓存，避免 HUD 取样错位
void ImageItem::updateCacheMode()
{
    if (m_imageSize.isEmpty() || m_pixelInspect || m_viewScale >= 0.999) {
        setCacheMode(QGraphicsItem::NoCache);
        return;
    }
    const qreal area = static_cast<qreal>(m_imageSize.width()) * m_viewScale
        * static_cast<qreal>(m_imageSize.height()) * m_viewScale;
    if (area > kImageCacheMaxDevicePx) {
        setCacheMode(QGraphicsItem::NoCache);
    } else {
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    }
}

// 图像宽高对应的场景矩形
QRectF ImageItem::boundingRect() const
{
    if (m_imageSize.isEmpty()) {
        return QRectF();
    }
    return QRectF(0, 0, m_imageSize.width(), m_imageSize.height());
}

// 缩小画 LOD，1:1 只贴可见像素格
void ImageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    if (m_image.isNull()) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

    // 1:1：像素 (i,j) 占半开区间 [i,i+1)×[j,j+1)，只画露出的源矩形
    QRectF vis = boundingRect();
    if (option) {
        vis = option->exposedRect.intersected(vis);
    }
    const int x0 = qMax(0, qFloor(vis.left()));
    const int y0 = qMax(0, qFloor(vis.top()));
    const int x1 = qMin(m_imageSize.width(), qCeil(vis.right()));
    const int y1 = qMin(m_imageSize.height(), qCeil(vis.bottom()));
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    const QRect src(x0, y0, x1 - x0, y1 - y0);
    painter->drawImage(QRectF(src), m_image, QRectF(src));
}

}
