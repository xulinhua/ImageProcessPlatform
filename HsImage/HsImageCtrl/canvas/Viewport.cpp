/**
 * @file Viewport.cpp
 * @brief Viewport实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCtrl/canvas/Viewport.h>

#include <QtMath>

namespace HsCtrl {

// 空视口
Viewport::Viewport()
{
}

// 底图尺寸与 part 均有效
bool Viewport::isValid() const
{
    return imageSize.width() > 0 && imageSize.height() > 0 && part.width() > 1e-6 && part.height() > 1e-6;
}

// 整幅图等比装入窗口，更新 part
void Viewport::fit(const QSize& viewportSize)
{
    // 整幅图等比装入窗口，更新 part。
    if (imageSize.isEmpty() || viewportSize.isEmpty()) {
        return;
    }
    const qreal imgW = imageSize.width();
    const qreal imgH = imageSize.height();
    const qreal vpW = viewportSize.width();
    const qreal vpH = viewportSize.height();
    const qreal s = qMin(vpW / imgW, vpH / imgH);
    if (s <= 1e-12) {
        return;
    }
    QRectF p(0, 0, vpW / s, vpH / s);
    p.moveCenter(QPointF(imgW * 0.5, imgH * 0.5));
    part = p;
}

// 保持中心，使 part 宽高比等于窗口
void Viewport::syncAspect(const QSize& viewportSize)
{
    if (viewportSize.isEmpty() || part.height() <= 1e-6) {
        return;
    }
    const qreal vpAspect = static_cast<qreal>(viewportSize.width()) / viewportSize.height();
    const QPointF c = part.center();
    part.setWidth(part.height() * vpAspect);
    part.moveCenter(c);
}

// 以图像点为锚缩放 part，不重采样位图
void Viewport::zoomAt(const QPointF& imagePt, const QPointF& relInPart, qreal factor, const QSize& viewportSize)
{
    // 以光标对应的图像点为锚缩放 part，不重采样位图。
    if (factor <= 0.0 || !isValid()) {
        return;
    }
    QRectF p = part;
    p.setWidth(p.width() / factor);
    p.setHeight(p.height() / factor);
    const qreal minSize = 4.0;
    const qreal maxW = imageSize.width() * 64.0;
    if (p.width() < minSize) {
        const qreal s = minSize / p.width();
        p.setWidth(p.width() * s);
        p.setHeight(p.height() * s);
    }
    if (p.width() > maxW) {
        const qreal s = maxW / p.width();
        p.setWidth(p.width() * s);
        p.setHeight(p.height() * s);
    }
    p.moveLeft(imagePt.x() - relInPart.x() * p.width());
    p.moveTop(imagePt.y() - relInPart.y() * p.height());
    part = p;
    syncAspect(viewportSize);
}

// 平移 part（图像坐标增量）
void Viewport::panImage(const QPointF& imageDelta)
{
    part.translate(imageDelta);
}

// 屏幕像素 / 图像像素
qreal Viewport::scale(const QSize& viewportSize) const
{
    if (!isValid() || viewportSize.isEmpty()) {
        return 1.0;
    }
    const qreal sx = static_cast<qreal>(viewportSize.width()) / part.width();
    const qreal sy = static_cast<qreal>(viewportSize.height()) / part.height();
    return qMin(sx, sy);
}

// 场景到视口的 QTransform
QTransform Viewport::viewTransform(const QSize& viewportSize) const
{
    QTransform t;
    if (!isValid() || viewportSize.isEmpty()) {
        return t;
    }
    const qreal s = scale(viewportSize);
    t.translate(viewportSize.width() * 0.5, viewportSize.height() * 0.5);
    t.scale(s, s);
    t.translate(-(part.x() + part.width() * 0.5),
                -(part.y() + part.height() * 0.5));
    return t;
}

}
