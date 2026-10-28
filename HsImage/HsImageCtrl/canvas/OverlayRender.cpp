/**
 * @file OverlayRender.cpp
 * @brief OverlayRender实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCtrl/canvas/OverlayRender.h>

#include "QtGeom.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QLineF>
#include <QPaintDevice>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRgb>
#include <QTransform>
#include <QtMath>

namespace HsCtrl {

// 旋转矩形四角（图像坐标）
static QPolygonF rect2Polygon(const HsCommData::Rect2P& r)
{
    const qreal c = qCos(r.phi);
    const qreal s = qSin(r.phi);
    const QPointF x(c * r.l1, s * r.l1);
    const QPointF y(-s * r.l2, c * r.l2);
    const QPointF org = toQ(r.c);
    QPolygonF poly;
    poly << (org + x + y) << (org - x + y) << (org - x - y) << (org + x - y);
    return poly;
}

// 轮廓折线转成 path
static QPainterPath contourPath(const HsCommData::HsContour& c)
{
    QPainterPath path;
    if (c.ptsEmpty()) {
        return path;
    }
    path.moveTo(toQ(c.ptAt(0)));
    for (size_t i = 1; i < c.ptCount(); ++i) {
        path.lineTo(toQ(c.ptAt(i)));
    }
    if (c.closed) {
        path.closeSubpath();
    }
    return path;
}

// 按 HsCommData::GeomStyle 设 pen/brush。selected 时换高亮笔
static void applyStyle(QPainter* painter, const HsCommData::Graphic& g, bool selected)
{
    const HsCommData::GeomStyle& st = g.geomStyle;
    QPen pen(selected ? toQ(HsCommData::GeomStyle::highlightStyle().stroke) : toQ(st.stroke));
    pen.setWidthF(st.widthPx > 0.0 ? st.widthPx : 1.0);
    pen.setCosmetic(st.cosmetic);
    pen.setStyle(toQ(st.line));
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(st.line == HsCommData::LineSolid ? Qt::RoundCap : Qt::FlatCap);
    painter->setPen(pen);

    QColor fill = toQ(st.fill);
    if (st.draw == HsCommData::DrawFill) {
        painter->setBrush(fill);
    } else {
        painter->setBrush(Qt::NoBrush);
    }
    painter->setOpacity(st.opacity);
    painter->setRenderHint(QPainter::Antialiasing, true);
}

// HsCommData::KindMask：非 0 像素涂 fill
static void paintMask(QPainter* painter, const HsCommData::Graphic& g)
{
    if (g.mask.isEmpty()) {
        return;
    }
    QColor fill = toQ(g.geomStyle.fill);
    if (fill.alpha() <= 0) {
        fill = toQ(g.geomStyle.stroke);
        fill.setAlpha(128);
    }
    const int w = g.mask.width;
    const int h = g.mask.height;
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);
    for (int y = 0; y < h; ++y) {
        QRgb* dst = reinterpret_cast<QRgb*>(img.scanLine(y));
        const unsigned char* src = &g.mask.pixels[static_cast<size_t>(y) * static_cast<size_t>(w)];
        for (int x = 0; x < w; ++x) {
            const int a = (fill.alpha() * static_cast<int>(src[x])) / 255;
            if (a <= 0) {
                dst[x] = 0;
            } else {
                dst[x] = qRgba(fill.red() * a / 255, fill.green() * a / 255,
                               fill.blue() * a / 255, a);
            }
        }
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawImage(QPointF(0, 0), img);
}

// HsCommData::KindTextBadge：色块加文字，字号保持屏幕像素
static void paintTextBadge(QPainter* painter, const HsCommData::Graphic& g, const HsCommData::HsText& text)
{
    const QString s = toQ(text.content);
    if (s.isEmpty()) {
        return;
    }
    const qreal fontPx = g.textStyle.fontPx > 1.0 ? g.textStyle.fontPx : 14.0;
    QFont f = painter->font();
    f.setPixelSize(qBound(8, qRound(fontPx), 72));
    f.setBold(true);
    painter->setFont(f);
    const QFontMetrics fm(f);
    const qreal pad = fm.height() * 0.3;
    const qreal maxW = fm.height() * 12.0;
    const QString shown = (fm.width(s) > maxW)
                              ? fm.elidedText(s, Qt::ElideMiddle, static_cast<int>(maxW))
                              : s;
    const QRectF br(toQ(text.anchor), QSizeF(fm.width(shown) + 2 * pad, fm.height() + pad));
    painter->setPen(Qt::NoPen);
    const bool boxed = !g.textStyle.back.isUnset();
    QColor bg = boxed ? toQ(g.textStyle.back) : toQ(g.textStyle.color);
    bg.setAlpha(200);
    painter->setBrush(bg);
    painter->drawRoundedRect(br, br.height() / 4.0, br.height() / 4.0);
    painter->setPen(boxed ? toQ(g.textStyle.color) : toQ(HsCommData::Color4::of(HsCommData::ColorWhite)));
    painter->drawText(br.adjusted(pad, 0, -pad, 0), Qt::AlignVCenter | Qt::AlignLeft, shown);
}

// HsCommData::KindText：锚点在图像坐标，字号保持屏幕像素
static void paintKindText(QPainter* painter, const HsCommData::Graphic& g, const HsCommData::HsText& text)
{
    painter->save();
    const QTransform wt = painter->worldTransform();
    painter->resetTransform();
    const int px = qBound(8, qRound(g.textStyle.fontPx > 1.0 ? g.textStyle.fontPx : 12.0), 72);
    QFont f = painter->font();
    f.setPixelSize(px);
    painter->setFont(f);
    const QFontMetrics fm(f);
    const QString s = toQ(text.content);
    const QPointF base = wt.map(toQ(text.anchor)) + QPointF(4, fm.ascent() - 4);
    const QColor fill = toQ(g.textStyle.color);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            painter->setPen(toQ(HsCommData::Color4::of(HsCommData::ColorBlack, 220)));
            painter->drawText(base + QPointF(dx, dy), s);
        }
    }
    painter->setPen(fill.isValid() ? fill : toQ(HsCommData::Color4::of(HsCommData::ColorWhite)));
    painter->drawText(base, s);
    painter->restore();
}

// 每条弦一个矩形，拼成填充 path
static void paintRegionRuns(QPainter* painter, const HsCommData::HsRegion& region)
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (size_t i = 0; i < region.runCount(); ++i) {
        const HsCommData::Chord& c = region.runAt(i);
        const int32_t len = c.length();
        if (len <= 0) {
            continue;
        }
        path.addRect(QRectF(c.cb, c.row, len, 1.0));
    }
    painter->drawPath(path);
}

// 画多边形或折线
static void paintPoly(QPainter* painter, const HsCommData::HsPoly& poly)
{
    if (poly.segsEmpty()) {
        return;
    }
    QPainterPath path;
    path.moveTo(toQ(poly.segAt(0).pt));
    for (size_t i = 1; i < poly.segCount(); ++i) {
        path.lineTo(toQ(poly.segAt(i).pt));
    }
    if (poly.closed) {
        path.closeSubpath();
    }
    painter->drawPath(path);
}

// 画一条箭头，含 HsArrow 的翼
static void paintOneArrow(QPainter* painter, const HsCommData::HsArrow& arrow)
{
    const QPointF a = toQ(arrow.tail);
    const QPointF b = toQ(arrow.head);
    painter->drawLine(a, b);
    HsCommData::Point2d left;
    HsCommData::Point2d right;
    if (!arrow.wings(&left, &right)) {
        return;
    }
    painter->drawLine(b, toQ(left));
    painter->drawLine(b, toQ(right));
}

// Stack 混放时按 geom 下标画一个元素，笔色仍跟 Graphic.kind
static void paintObjectAt(QPainter* painter, const HsCommData::Graphic& g, int index, const QPen& pen)
{
    switch (g.geom.kindAt(index)) {
    case HsCommData::ObjCircle: {
        const HsCommData::CircleP* c = g.geom.circleAt(index);
        painter->drawEllipse(toQ(c->c), c->r, c->r);
        break;
    }
    case HsCommData::ObjEllipse: {
        const HsCommData::EllipseP* e = g.geom.ellipseAt(index);
        painter->save();
        painter->translate(toQ(e->c));
        painter->rotate(e->phi * 180.0 / HsCommData::kPi);
        painter->drawEllipse(QPointF(0, 0), e->ra, e->rb);
        painter->restore();
        break;
    }
    case HsCommData::ObjRect1:
        painter->drawRect(toQ(normalized(g.geom.rect1At(index)->r)));
        break;
    case HsCommData::ObjRect2:
        painter->drawPolygon(rect2Polygon(*g.geom.rect2At(index)));
        break;
    case HsCommData::ObjLine: {
        const HsCommData::LineP* ln = g.geom.lineAt(index);
        painter->drawLine(toQ(ln->a), toQ(ln->b));
        break;
    }
    case HsCommData::ObjContour:
        painter->save();
        painter->setOpacity(1.0);
        {
            QColor solid = pen.color();
            solid.setAlpha(255);
            QPen solidPen = pen;
            solidPen.setColor(solid);
            painter->setPen(solidPen);
        }
        if (g.kind == HsCommData::KindKeypoints) {
            const qreal r = qMax(1.0, (g.geomStyle.widthPx > 0.0 ? g.geomStyle.widthPx : 1.0) * 2.0);
            const QColor c = pen.color();
            const HsCommData::HsContour* ct = g.geom.contourAt(index);
            for (size_t i = 0; i < ct->ptCount(); ++i) {
                const bool occluded = (index == 0) && (i < g.keypointVis.size()) && (g.keypointVis[i] == 1);
                painter->setBrush(occluded ? Qt::NoBrush : QBrush(c));
                painter->drawEllipse(toQ(ct->ptAt(i)), r, r);
            }
        } else if (g.kind == HsCommData::KindSkeleton) {
            const HsCommData::HsContour* ctPts = g.geom.contourAt(index);
            for (size_t i = 0; i + 1 < ctPts->ptCount(); i += 2) {
                const size_t ei = i / 2;
                const bool dashed = (index == 0) && (ei < g.edgeDashed.size()) && (g.edgeDashed[ei] != 0);
                if (dashed) {
                    QColor dc = pen.color();
                    dc.setAlpha(120);
                    QPen dp(dc, pen.widthF(), Qt::DashLine);
                    dp.setCosmetic(pen.isCosmetic());
                    painter->setPen(dp);
                }
                painter->drawLine(toQ(ctPts->ptAt(i)), toQ(ctPts->ptAt(i + 1)));
                if (dashed) {
                    painter->setPen(pen);
                }
            }
        } else {
            painter->drawPath(contourPath(*g.geom.contourAt(index)));
        }
        painter->restore();
        break;
    case HsCommData::ObjPoly:
        paintPoly(painter, *g.geom.polyAt(index));
        break;
    case HsCommData::ObjRegion: {
        painter->save();
        painter->setOpacity(1.0);
        QColor solid = pen.color();
        solid.setAlpha(255);
        painter->setPen(Qt::NoPen);
        painter->setBrush(solid);
        paintRegionRuns(painter, *g.geom.regionAt(index));
        painter->restore();
        break;
    }
    default:
        break;
    }
}

// 把过中心的直线裁到视口矩形，供穿窗十字
static bool clipLine(const QRectF& vr, const QPointF& p, qreal ang, QPointF* a, QPointF* b)
{
    if (!a || !b || vr.isEmpty()) {
        return false;
    }
    const qreal dx = qCos(ang);
    const qreal dy = qSin(ang);
    qreal t0 = -1.0e30;
    qreal t1 = 1.0e30;
    const qreal pdir[4] = { -dx, dx, -dy, dy };
    const qreal q[4] = {
        p.x() - vr.left(), vr.right() - p.x(),
        p.y() - vr.top(), vr.bottom() - p.y()
    };
    for (int i = 0; i < 4; ++i) {
        if (qAbs(pdir[i]) < 1.0e-12) {
            if (q[i] < 0.0) {
                return false;
            }
            continue;
        }
        const qreal t = q[i] / pdir[i];
        if (pdir[i] < 0.0) {
            if (t > t1) {
                return false;
            }
            if (t > t0) {
                t0 = t;
            }
        } else {
            if (t < t0) {
                return false;
            }
            if (t < t1) {
                t1 = t;
            }
        }
    }
    *a = p + QPointF(t0 * dx, t0 * dy);
    *b = p + QPointF(t1 * dx, t1 * dy);
    return true;
}

// 尺寸小于 0 时画穿窗十字，否则画有限半宽
static void paintOneCross(QPainter* painter, const HsCommData::HsCross& c)
{
    const QPointF p = toQ(c.center);
    if (c.size < 0.0) {
        QPaintDevice* dev = painter->device();
        if (!dev) {
            return;
        }
        const QTransform inv = painter->worldTransform().inverted();
        const QRectF vr = inv.mapRect(QRectF(0, 0, dev->width(), dev->height()));
        QPointF a;
        QPointF b;
        if (clipLine(vr, p, c.angle, &a, &b)) {
            painter->drawLine(a, b);
        }
        if (clipLine(vr, p, c.angle + HsCommData::kPi * 0.5, &a, &b)) {
            painter->drawLine(a, b);
        }
        return;
    }
    const qreal cph = qCos(c.angle);
    const qreal sph = qSin(c.angle);
    const QPointF ax(cph * c.size, sph * c.size);
    const QPointF ay(-sph * c.size, cph * c.size);
    painter->drawLine(p - ax, p + ax);
    painter->drawLine(p - ay, p + ay);
}

// 在当前变换下画一条 HsCommData::Graphic
void OverlayRender::paintScene(QPainter* painter, const HsCommData::Graphic& g, bool selected)
{
    if (!painter || !g.visible) {
        return;
    }
    const bool mixed = g.role == HsCommData::RoleStack
        && g.geom.isInitialized()
        && g.geom.kind() == HsCommData::ObjMixed;
    applyStyle(painter, g, selected);
    const QPen pen = painter->pen();
    if (mixed) {
        const int n = g.geom.count();
        for (int i = 0; i < n; ++i) {
            paintObjectAt(painter, g, i, pen);
        }
        switch (g.kind) {
        case HsCommData::KindCross:
            for (size_t i = 0; i < g.cross.size(); ++i) {
                paintOneCross(painter, g.cross[i]);
            }
            break;
        case HsCommData::KindArrow:
            for (size_t i = 0; i < g.arrow.size(); ++i) {
                paintOneArrow(painter, g.arrow[i]);
            }
            break;
        case HsCommData::KindPoint:
            painter->setBrush(pen.color());
            for (size_t i = 0; i < g.point.size(); ++i) {
                painter->drawEllipse(toQ(g.point[i]), 3.0, 3.0);
            }
            break;
        case HsCommData::KindText:
            for (size_t i = 0; i < g.text.size(); ++i) {
                paintKindText(painter, g, g.text[i]);
            }
            break;
        case HsCommData::KindTextBadge:
            for (size_t i = 0; i < g.text.size(); ++i) {
                paintTextBadge(painter, g, g.text[i]);
            }
            break;
        case HsCommData::KindMask:
            paintMask(painter, g);
            break;
        default:
            break;
        }
        return;
    }
    const int n = g.geomSpan();
    if (n <= 0) {
        return;
    }

    for (int gi = 0; gi < n; ++gi) {
    switch (g.kind) {
    case HsCommData::KindObject:
        paintObjectAt(painter, g, gi, pen);
        break;
    case HsCommData::KindArrow:
        if (gi >= 0 && gi < static_cast<int>(g.arrow.size())) {
            paintOneArrow(painter, g.arrow[static_cast<size_t>(gi)]);
        }
        break;
    case HsCommData::KindCross:
        if (gi >= 0 && gi < static_cast<int>(g.cross.size())) {
            paintOneCross(painter, g.cross[static_cast<size_t>(gi)]);
        }
        break;
    case HsCommData::KindPoint:
        if (gi >= 0 && gi < static_cast<int>(g.point.size())) {
            painter->setBrush(pen.color());
            painter->drawEllipse(toQ(g.point[static_cast<size_t>(gi)]), 3.0, 3.0);
        }
        break;
    case HsCommData::KindText:
        if (gi >= 0 && gi < static_cast<int>(g.text.size())) {
            paintKindText(painter, g, g.text[static_cast<size_t>(gi)]);
        }
        break;
    case HsCommData::KindKeypoints: {
        const qreal r = qMax(1.0, (g.geomStyle.widthPx > 0.0 ? g.geomStyle.widthPx : 1.0) * 2.0);
        const QColor c = pen.color();
        for (size_t i = 0; i < g.geom.contourAt(gi)->ptCount(); ++i) {
            const bool occluded = (gi == 0) && (i < g.keypointVis.size()) && (g.keypointVis[i] == 1);
            if (occluded) {
                painter->setBrush(Qt::NoBrush);
            } else {
                painter->setBrush(c);
            }
            painter->drawEllipse(toQ(g.geom.contourAt(gi)->ptAt(i)), r, r);
        }
        break;
    }
    case HsCommData::KindSkeleton: {
        const HsCommData::HsContour* ctPts = g.geom.contourAt(gi);
        for (size_t i = 0; i + 1 < ctPts->ptCount(); i += 2) {
            const size_t ei = i / 2;
            const bool dashed = (gi == 0) && (ei < g.edgeDashed.size()) && (g.edgeDashed[ei] != 0);
            if (dashed) {
                QColor dc = pen.color();
                dc.setAlpha(120);
                QPen dp(dc, pen.widthF(), Qt::DashLine);
                dp.setCosmetic(pen.isCosmetic());
                painter->setPen(dp);
            }
            painter->drawLine(toQ(ctPts->ptAt(i)), toQ(ctPts->ptAt(i + 1)));
            if (dashed) {
                painter->setPen(pen);
            }
        }
        break;
    }
    case HsCommData::KindTextBadge:
        if (gi >= 0 && gi < static_cast<int>(g.text.size())) {
            paintTextBadge(painter, g, g.text[static_cast<size_t>(gi)]);
        }
        break;
    case HsCommData::KindMask:
        paintMask(painter, g);
        break;
    default:
        break;
    }
    }
}

// 套上 image→device 后画一批 HsCommData::Graphic
void OverlayRender::paintList(QPainter* painter, const std::vector<HsCommData::Graphic>& items,
                              const QTransform& imageToDevice)
{
    if (!painter) {
        return;
    }
    painter->save();
    painter->setTransform(imageToDevice, false);
    for (size_t i = 0; i < items.size(); ++i) {
        paintScene(painter, items[i], false);
    }
    painter->restore();
}

}
