/**
 * @file GraphicItem.cpp
 * @brief GraphicItem实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "GraphicItem.h"
#include "QtGeom.h"

#include <HsCtrl/canvas/OverlayRender.h>

#include <QColor>
#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

namespace HsCtrl {

// 十字尺寸小于 0：任一臂长随当前窗口
static bool isWindowCross(const HsCommData::Graphic& g)
{
    if (g.kind != HsCommData::KindCross) {
        return false;
    }
    for (size_t i = 0; i < g.cross.size(); ++i) {
        if (g.cross[i].size < 0.0) {
            return true;
        }
    }
    return false;
}

// KindObject 且元素全是区域时走行程缓存
static bool regionGraphic(const HsCommData::Graphic& g)
{
    return g.kind == HsCommData::KindObject
        && g.geom.isInitialized()
        && g.geom.kind() == HsCommData::ObjRegion;
}

// Stack 的 geom 里种类不一致
static bool stackMixed(const HsCommData::Graphic& g)
{
    return g.role == HsCommData::RoleStack
        && g.geom.isInitialized()
        && g.geom.kind() == HsCommData::ObjMixed;
}

// Stack 把区域行程拼成一块来画；混放时跳过不是区域的元素。Drawing 只用第 0 个
static HsCommData::HsRegion shownRegion(const HsCommData::Graphic& g)
{
    const int n = g.geomSpan();
    if (n <= 0) {
        return HsCommData::HsRegion();
    }
    if (!stackMixed(g)) {
        if (n == 1) {
            return *g.geom.regionAt(0);
        }
    }
    HsCommData::HsRegion out;
    int x0 = 0;
    int y0 = 0;
    int x1 = -1;
    int y1 = -1;
    for (int i = 0; i < n; ++i) {
        if (stackMixed(g) && g.geom.kindAt(i) != HsCommData::ObjRegion) {
            continue;
        }
        const HsCommData::HsRegion* r = g.geom.regionAt(i);
        out.appendRuns(*r);
        if (r->isEmpty() || HsCommData::isEmpty(r->bounds)) {
            continue;
        }
        const int left = HsCommData::left(r->bounds);
        const int top = HsCommData::top(r->bounds);
        const int right = HsCommData::right(r->bounds);
        const int bottom = HsCommData::bottom(r->bounds);
        if (x1 < x0) {
            x0 = left;
            y0 = top;
            x1 = right;
            y1 = bottom;
        } else {
            x0 = qMin(x0, left);
            y0 = qMin(y0, top);
            x1 = qMax(x1, right);
            y1 = qMax(y1, bottom);
        }
    }
    if (x1 >= x0 && y1 >= y0) {
        out.bounds = HsCommData::fromInclusive(x0, y0, x1, y1);
    }
    return out;
}

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

// 单位圆映射到椭圆的变换
static QTransform ellipseTransform(const QPointF& c, qreal ra, qreal rb, qreal phi)
{
    QTransform t;
    t.translate(c.x(), c.y());
    t.rotateRadians(phi);
    t.scale(qMax(ra, 1e-6), qMax(rb, 1e-6));
    return t;
}

// 单条 HsCommData::Graphic 的场景 Item
GraphicItem::GraphicItem(const HsCommData::Graphic& g, QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_g(g)
    , m_selected(false)
    , m_viewScale(1.0)
{
    setAcceptedMouseButtons(Qt::NoButton);
    setZValue(m_g.role == HsCommData::RoleDrawing ? 20 : 10);
    setVisible(m_g.visible);
    ensureRegionDisplay();
    rebuildBounds();
}

// 替换协议数据并重建 HsCommData::HsRegion 缓存
void GraphicItem::setGraphic(const HsCommData::Graphic& g)
{
    prepareGeometryChange();
    m_g = g;
    setVisible(m_g.visible);
    ensureRegionDisplay();
    rebuildBounds();
    update();
}

// 当前协议数据副本
HsCommData::Graphic GraphicItem::graphic() const
{
    return m_g;
}

// Drawing 选中（显示手柄）
void GraphicItem::setSelectedDrawing(bool on)
{
    if (m_selected == on) {
        return;
    }
    prepareGeometryChange();
    m_selected = on;
    rebuildBounds();
    update();
}

// 是否为当前选中 ROI
bool GraphicItem::isSelectedDrawing() const
{
    return m_selected;
}

// 视口缩放，用于手柄粗细与 HsCommData::HsRegion LOD
void GraphicItem::setViewScale(qreal s)
{
    s = qMax(s, 1e-6);
    if (qAbs(m_viewScale - s) < 1e-4) {
        return;
    }
    const bool handles = m_selected && m_g.role == HsCommData::RoleDrawing;
    if (handles) {
        prepareGeometryChange();
    }
    m_viewScale = s;
    if (handles) {
        rebuildBounds();
    }
    if (regionGraphic(m_g) && RegionDisplay::preferMask(shownRegion(m_g))) {
        const qreal lod = RegionDisplay::clampLodScale(m_viewScale);
        if (regionLodStale(lod)) {
            m_regionDisp.rle = shownRegion(m_g);
            m_regionDisp.rebuildMask(regionFillColor(), lod);
            update();
        }
    }
}

// 是否为旋转手柄
bool GraphicItem::isRotationHandle(int index) const
{
    if (m_g.kind == HsCommData::KindObject && m_g.geom.kind() == HsCommData::ObjRect2) {
        return index == 8;
    }
    if (m_g.kind == HsCommData::KindObject && m_g.geom.kind() == HsCommData::ObjEllipse) {
        return index == 4;
    }
    if (m_g.kind == HsCommData::KindCross) {
        return index == 5;
    }
    return false;
}

// 旋转手柄光标（缓存一张 pixmap）
static QCursor rotateHandleCursor()
{
    static QCursor cur;
    static bool inited = false;
    if (!inited) {
        inited = true;
        QPixmap pm(32, 32);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(Qt::white, 3.0));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(7, 7, 18, 18), 50 * 16, 250 * 16);
        p.setPen(QPen(QColor(0x18, 0x90, 0xFF), 1.8));
        p.drawArc(QRectF(7, 7, 18, 18), 50 * 16, 250 * 16);
        QPolygonF arrow;
        arrow << QPointF(23.5, 8) << QPointF(28, 13) << QPointF(20, 13);
        p.setBrush(QColor(0x18, 0x90, 0xFF));
        p.setPen(Qt::NoPen);
        p.drawPolygon(arrow);
        p.end();
        cur = QCursor(pm, 16, 16);
    }
    return cur;
}

// 按视口中手柄方向选拉伸光标
static Qt::CursorShape sizeShapeFromViewAngle(qreal rad)
{
    qreal deg = rad * 180.0 / HsCommData::kPi;
    while (deg < 0.0) {
        deg += 180.0;
    }
    while (deg >= 180.0) {
        deg -= 180.0;
    }
    if (deg < 22.5 || deg >= 157.5) {
        return Qt::SizeHorCursor;
    }
    if (deg < 67.5) {
        return Qt::SizeFDiagCursor;
    }
    if (deg < 112.5) {
        return Qt::SizeVerCursor;
    }
    return Qt::SizeBDiagCursor;
}

// 该手柄在当前视口下的光标
QCursor GraphicItem::cursorForHandle(int index, const QTransform& sceneToView) const
{
    if (isRotationHandle(index)) {
        return rotateHandleCursor();
    }

    QPointF center;
    if (m_g.kind == HsCommData::KindObject) {
        switch (m_g.geom.kind()) {
        case HsCommData::ObjCircle:
            center = toQ(m_g.geom.circleAt(0)->c);
            break;
        case HsCommData::ObjEllipse:
            center = toQ(m_g.geom.ellipseAt(0)->c);
            break;
        case HsCommData::ObjRect1:
            center = toQ(normalized(m_g.geom.rect1At(0)->r)).center();
            break;
        case HsCommData::ObjRect2:
            center = toQ(m_g.geom.rect2At(0)->c);
            break;
        default:
            return QCursor(Qt::SizeAllCursor);
        }
    } else if (m_g.kind == HsCommData::KindCross && !m_g.cross.empty()) {
        if (index <= 0) {
            return QCursor(Qt::SizeAllCursor);
        }
        center = toQ(m_g.cross[0].center);
    } else {
        return QCursor(Qt::SizeAllCursor);
    }

    const QVector<QPointF> hs = handleScenePositions();
    if (index < 0 || index >= hs.size()) {
        return QCursor(Qt::SizeAllCursor);
    }

    const QPointF a = sceneToView.map(center);
    const QPointF b = sceneToView.map(hs[index]);
    const qreal ang = qAtan2(b.y() - a.y(), b.x() - a.x());
    return QCursor(sizeShapeFromViewAngle(ang));
}

// 按几何与手柄重算包围盒
void GraphicItem::rebuildBounds()
{
    m_g.updateBounds();
    if (isWindowCross(m_g) && !m_g.cross.empty()) {
        const qreal s = 1.0e7;
        HsCommData::Point2d c = m_g.cross[0].center;
        for (size_t i = 0; i < m_g.cross.size(); ++i) {
            if (m_g.cross[i].size < 0.0) {
                c = m_g.cross[i].center;
                break;
            }
        }
        m_bounds = QRectF(c.x - s, c.y - s, s * 2.0, s * 2.0);
    } else {
        m_bounds = toQ(m_g.boundsImage).adjusted(-4, -4, 4, 4);
    }
    if (m_selected && m_g.role == HsCommData::RoleDrawing) {
        const QVector<QPointF> hs = handleScenePositions();
        const qreal pad = 12.0 / m_viewScale;
        for (int i = 0; i < hs.size(); ++i) {
            m_bounds = m_bounds.united(QRectF(hs[i].x() - pad, hs[i].y() - pad, pad * 2.0, pad * 2.0));
        }
    }
}

// 按 RLE 复杂度选择 path 或 mask
void GraphicItem::ensureRegionDisplay()
{
    if (!regionGraphic(m_g)) {
        m_regionDisp = RegionDisplay();
        return;
    }
    m_regionDisp.rle = shownRegion(m_g);
    m_regionDisp.maskLod = QPixmap();
    m_regionDisp.maskScale = 0;
    m_regionDisp.path = QPainterPath();
    m_regionDisp.outline = QPainterPath();
    // 填充用行程或 mask，描边才要边缘。两边不要一起做
    if (m_g.geomStyle.draw == HsCommData::DrawFill) {
        if (RegionDisplay::preferMask(m_regionDisp.rle)) {
            const qreal lod = RegionDisplay::clampLodScale(m_viewScale);
            m_regionDisp.rebuildMask(regionFillColor(), lod);
        } else {
            m_regionDisp.rebuildPath();
        }
    } else {
        m_regionDisp.rebuildOutline();
    }
}

// mask 是否因缩放过期
bool GraphicItem::regionLodStale(qreal lod) const
{
    if (m_regionDisp.maskLod.isNull() || m_regionDisp.maskScale <= 0) {
        return true;
    }
    if (m_viewScale >= 0.999 && m_regionDisp.maskScale < 0.999) {
        // 放大后必须换 1:1 mask，否则阈值涂色相对底图错格。
        return true;
    }
    const qreal built = m_regionDisp.maskScale;
    const qreal ratio = (lod > built) ? (lod / built) : (built / lod);
    return ratio >= 1.6;
}

// HsCommData::HsRegion 填充色
QColor GraphicItem::regionFillColor() const
{
    QColor c = toQ(m_g.geomStyle.fill);
    if (c.alpha() <= 0) {
        c = toQ(m_g.geomStyle.stroke);
    }
    c.setAlpha(255);
    return c;
}

// 把 mask 贴到 bounds 矩形
void GraphicItem::paintRegionMask(QPainter* painter, const QStyleOptionGraphicsItem* option) const
{
    const QRectF dest(toQ(m_regionDisp.rle.bounds));
    if (!dest.isValid() || dest.isEmpty() || m_regionDisp.maskLod.isNull()) {
        return;
    }
    QRectF vis = dest;
    if (option) {
        vis = option->exposedRect.intersected(dest);
    }
    if (vis.isEmpty()) {
        return;
    }
    const QPixmap& pm = m_regionDisp.maskLod;
    const qreal sx = static_cast<qreal>(pm.width()) / dest.width();
    const qreal sy = static_cast<qreal>(pm.height()) / dest.height();
    const QRectF src((vis.left() - dest.left()) * sx,
                     (vis.top() - dest.top()) * sy,
                     vis.width() * sx,
                     vis.height() * sy);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, m_viewScale < 0.999);
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPixmap(vis, pm, src);
}

// 含手柄垫的包围盒
QRectF GraphicItem::boundingRect() const
{
    return m_bounds;
}

// 轮廓路径（命中与描边共用）。Stack 把 geom 里每个几何都加进去
QPainterPath GraphicItem::outlinePath() const
{
    QPainterPath path;
    // 几何部分：Stack 取全部格，其它只取第 0 格。区域用外接矩形代替
    if (m_g.kind == HsCommData::KindObject || stackMixed(m_g)) {
        const int n = (m_g.role == HsCommData::RoleStack) ? m_g.geom.count() : 1;
        for (int gi = 0; gi < n; ++gi) {
            switch (m_g.geom.kindAt(gi)) {
            case HsCommData::ObjCircle:
                path.addEllipse(toQ(m_g.geom.circleAt(gi)->c), m_g.geom.circleAt(gi)->r, m_g.geom.circleAt(gi)->r);
                break;
            case HsCommData::ObjEllipse: {
                QPainterPath e;
                e.addEllipse(QPointF(0, 0), 1.0, 1.0);
                path.addPath(ellipseTransform(toQ(m_g.geom.ellipseAt(gi)->c), m_g.geom.ellipseAt(gi)->ra, m_g.geom.ellipseAt(gi)->rb, m_g.geom.ellipseAt(gi)->phi).map(e));
                break;
            }
            case HsCommData::ObjRect1:
                path.addRect(toQ(normalized(m_g.geom.rect1At(gi)->r)));
                break;
            case HsCommData::ObjRect2:
                path.addPolygon(rect2Polygon(*m_g.geom.rect2At(gi)));
                path.closeSubpath();
                break;
            case HsCommData::ObjLine:
                path.moveTo(toQ(m_g.geom.lineAt(gi)->a));
                path.lineTo(toQ(m_g.geom.lineAt(gi)->b));
                break;
            case HsCommData::ObjContour:
            case HsCommData::ObjPoly: {
                if (m_g.geom.kindAt(gi) == HsCommData::ObjPoly) {
                    const HsCommData::HsPoly* p = m_g.geom.polyAt(gi);
                    if (!p->segsEmpty()) {
                        path.moveTo(toQ(p->segAt(0).pt));
                        for (size_t i = 1; i < p->segCount(); ++i) {
                            path.lineTo(toQ(p->segAt(i).pt));
                        }
                        if (p->closed) {
                            path.closeSubpath();
                        }
                    }
                } else {
                    const HsCommData::HsContour* c = m_g.geom.contourAt(gi);
                    if (!c->ptsEmpty()) {
                        path.moveTo(toQ(c->ptAt(0)));
                        for (size_t i = 1; i < c->ptCount(); ++i) {
                            path.lineTo(toQ(c->ptAt(i)));
                        }
                        if (c->closed) {
                            path.closeSubpath();
                        }
                    }
                }
                break;
            }
            case HsCommData::ObjRegion:
                path.addRect(toQ(m_g.geom.regionAt(gi)->bounds));
                break;
            default:
                break;
            }
        }
        if (m_g.kind == HsCommData::KindObject
            || m_g.kind == HsCommData::KindKeypoints
            || m_g.kind == HsCommData::KindSkeleton) {
            return path;
        }
    }
    // 附加字段部分：穿窗十字用 1e7 半长近似，文字只取锚点处 8x8 小框
    const int n = m_g.geomSpan();
    for (int gi = 0; gi < n; ++gi) {
    switch (m_g.kind) {
    case HsCommData::KindArrow: {
        if (gi < 0 || gi >= static_cast<int>(m_g.arrow.size())) {
            break;
        }
        const HsCommData::HsArrow& arrow = m_g.arrow[static_cast<size_t>(gi)];
        path.moveTo(toQ(arrow.tail));
        path.lineTo(toQ(arrow.head));
        HsCommData::Point2d left;
        HsCommData::Point2d right;
        if (arrow.wings(&left, &right)) {
            path.moveTo(toQ(arrow.head));
            path.lineTo(toQ(left));
            path.moveTo(toQ(arrow.head));
            path.lineTo(toQ(right));
        }
        break;
    }
    case HsCommData::KindCross: {
        if (gi < 0 || gi >= static_cast<int>(m_g.cross.size())) {
            break;
        }
        const HsCommData::HsCross& c = m_g.cross[static_cast<size_t>(gi)];
        const QPointF p = toQ(c.center);
        const qreal s = (c.size < 0.0) ? 1.0e7 : c.size;
        const qreal cph = qCos(c.angle);
        const qreal sph = qSin(c.angle);
        const QPointF ax(cph * s, sph * s);
        const QPointF ay(-sph * s, cph * s);
        path.moveTo(p - ax);
        path.lineTo(p + ax);
        path.moveTo(p - ay);
        path.lineTo(p + ay);
        break;
    }
    case HsCommData::KindPoint:
        if (gi >= 0 && gi < static_cast<int>(m_g.point.size())) {
            path.addEllipse(toQ(m_g.point[static_cast<size_t>(gi)]), 3.0, 3.0);
        }
        break;
    case HsCommData::KindText:
        if (gi >= 0 && gi < static_cast<int>(m_g.text.size())) {
            path.addRect(QRectF(toQ(m_g.text[static_cast<size_t>(gi)].anchor), QSizeF(8, 8)));
        }
        break;
    case HsCommData::KindKeypoints: {
        const qreal r = qMax(1.0, (m_g.geomStyle.widthPx > 0.0 ? m_g.geomStyle.widthPx : 1.0) * 2.0);
        for (size_t i = 0; i < m_g.geom.contourAt(gi)->ptCount(); ++i) {
            path.addEllipse(toQ(m_g.geom.contourAt(gi)->ptAt(i)), r, r);
        }
        break;
    }
    case HsCommData::KindSkeleton: {
        const HsCommData::HsContour* ctPts = m_g.geom.contourAt(gi);
        for (size_t i = 0; i + 1 < ctPts->ptCount(); i += 2) {
            path.moveTo(toQ(ctPts->ptAt(i)));
            path.lineTo(toQ(ctPts->ptAt(i + 1)));
        }
        break;
    }
    case HsCommData::KindTextBadge:
        if (gi >= 0 && gi < static_cast<int>(m_g.text.size())) {
            path.addRect(QRectF(toQ(m_g.text[static_cast<size_t>(gi)].anchor), QSizeF(8, 8)));
        }
        break;
    case HsCommData::KindMask:
        if (!m_g.mask.isEmpty()) {
            path.addRect(QRectF(0, 0, m_g.mask.width, m_g.mask.height));
        }
        break;
    default:
        break;
    }
    }
    return path;
}

// 加描边宽度后的命中形状
QPainterPath GraphicItem::shape() const
{
    if (regionGraphic(m_g) && !stackMixed(m_g)) {
        QPainterPath p;
        p.addRect(m_bounds);
        return p;
    }
    QPainterPath path = outlinePath();
    QPainterPathStroker stroker;
    stroker.setWidth(8.0);
    return stroker.createStroke(path).united(path);
}

// 从 painter 世界变换取当前缩放
qreal GraphicItem::viewScale(const QPainter* painter) const
{
    const QTransform wt = painter->worldTransform();
    return QLineF(wt.map(QPointF(0, 0)), wt.map(QPointF(1, 0))).length();
}

// 按 kind 画几何，选中时画手柄
void GraphicItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    if (regionGraphic(m_g) && !stackMixed(m_g)) {
        const HsCommData::GeomStyle& st = m_g.geomStyle;
        QPen pen(m_selected ? toQ(HsCommData::GeomStyle::highlightStyle().stroke) : toQ(st.stroke));
        pen.setWidthF(st.widthPx);
        pen.setCosmetic(st.cosmetic);
        pen.setStyle(toQ(st.line));
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(st.line == HsCommData::LineSolid ? Qt::RoundCap : Qt::FlatCap);
        painter->setPen(pen);
        if (st.draw == HsCommData::DrawFill) {
            QColor body = toQ(st.fill);
            if (body.alpha() <= 0) {
                body = toQ(st.stroke);
            }
            body.setAlpha(255);
            painter->setBrush(body);
            painter->setOpacity(1.0);
            painter->setRenderHint(QPainter::Antialiasing, true);
            if (!m_regionDisp.maskLod.isNull()) {
                paintRegionMask(painter, option);
            } else {
                painter->drawPath(m_regionDisp.path);
            }
        } else {
            QColor edge = toQ(st.stroke);
            edge.setAlpha(255);
            // Region 的 boundary() 是一像素行程。widthPx 大于 1 时用 QPen
            // 扩边，才能像普通几何一样遵守 Graphic::GeomStyle.widthPx。
            if (st.widthPx > 1.0) {
                painter->setPen(pen);
                painter->setBrush(edge);
            } else {
                painter->setPen(Qt::NoPen);
                painter->setBrush(edge);
            }
            painter->setOpacity(1.0);
            painter->setRenderHint(QPainter::Antialiasing, false);
            painter->drawPath(m_regionDisp.outline);
        }
    } else {
        OverlayRender::paintScene(painter, m_g, m_selected);
    }

    if (m_g.role == HsCommData::RoleDrawing) {
        painter->save();
        const QTransform wt = painter->worldTransform();
        painter->resetTransform();
        QFont f = painter->font();
        f.setPixelSize(11);
        painter->setFont(f);
        painter->setPen(toQ(m_g.geomStyle.stroke));
        QString name;
        if (m_g.kind == HsCommData::KindObject) {
            switch (m_g.geom.kind()) {
            case HsCommData::ObjCircle:  name = QStringLiteral("Circle"); break;
            case HsCommData::ObjEllipse: name = QStringLiteral("Ellipse"); break;
            case HsCommData::ObjRect1:   name = QStringLiteral("Rect1"); break;
            case HsCommData::ObjRect2:   name = QStringLiteral("Rect2"); break;
            case HsCommData::ObjLine:    name = QStringLiteral("Line"); break;
            case HsCommData::ObjContour: name = QStringLiteral("XLD"); break;
            case HsCommData::ObjRegion:  name = QStringLiteral("Region"); break;
            default: break;
            }
        }
        switch (m_g.kind) {
        case HsCommData::KindKeypoints:  name = QStringLiteral("Keypoints"); break;
        case HsCommData::KindSkeleton:   name = QStringLiteral("Skeleton"); break;
        case HsCommData::KindTextBadge:  name = QStringLiteral("Badge"); break;
        case HsCommData::KindMask:       name = QStringLiteral("Mask"); break;
        default: break;
        }
        if (!name.isEmpty()) {
            const QPointF p = wt.map(toQ(topLeft(m_g.boundsImage))) + QPointF(4, -6);
            painter->drawText(p, name);
        }
        painter->restore();
    }

    if (m_selected && m_g.role == HsCommData::RoleDrawing) {
        paintHandles(painter);
    }
}

// 画选中手柄（屏幕像素大小）
void GraphicItem::paintHandles(QPainter* painter) const
{
    const QVector<QPointF> hs = handleScenePositions();
    if (hs.isEmpty()) {
        return;
    }
    const qreal s = qMax(viewScale(painter), 1e-6);
    const qreal r = 4.5 / s;
    painter->save();
    painter->setOpacity(1.0);
    QPen hp(QColor(0x18, 0x90, 0xFF));
    hp.setWidthF(1.0);
    hp.setCosmetic(true);
    painter->setPen(hp);

    if (m_g.kind == HsCommData::KindObject && m_g.geom.kind() == HsCommData::ObjRect2 && hs.size() >= 9) {
        painter->drawLine(hs[4], hs[8]);
    } else if (m_g.kind == HsCommData::KindObject && m_g.geom.kind() == HsCommData::ObjEllipse && hs.size() >= 5) {
        painter->drawLine(hs[0], hs[4]);
    }

    for (int i = 0; i < hs.size(); ++i) {
        if (isRotationHandle(i)) {
            painter->setBrush(QColor(0x18, 0x90, 0xFF));
            painter->drawEllipse(hs[i], r, r);
        } else {
            painter->setBrush(QColor(255, 255, 255));
            painter->drawRect(QRectF(hs[i].x() - r, hs[i].y() - r, r * 2.0, r * 2.0));
        }
    }
    painter->restore();
}

// 手柄在图像坐标系中的位置
QVector<QPointF> GraphicItem::handleScenePositions() const
{
    QVector<QPointF> hs;
    const qreal pad = 18.0 / qMax(m_viewScale, 1e-6);
    if (m_g.kind == HsCommData::KindObject && m_g.geom.isInitialized()) {
        switch (m_g.geom.kind()) {
        case HsCommData::ObjCircle:
            hs << (toQ(m_g.geom.circleAt(0)->c) + QPointF(m_g.geom.circleAt(0)->r, 0));
            hs << (toQ(m_g.geom.circleAt(0)->c) + QPointF(0, m_g.geom.circleAt(0)->r));
            hs << (toQ(m_g.geom.circleAt(0)->c) + QPointF(-m_g.geom.circleAt(0)->r, 0));
            hs << (toQ(m_g.geom.circleAt(0)->c) + QPointF(0, -m_g.geom.circleAt(0)->r));
            break;
        case HsCommData::ObjEllipse: {
            const qreal c = qCos(m_g.geom.ellipseAt(0)->phi);
            const qreal s = qSin(m_g.geom.ellipseAt(0)->phi);
            const QPointF ax(c, s);
            const QPointF ay(-s, c);
            const QPointF org = toQ(m_g.geom.ellipseAt(0)->c);
            hs << (org + ax * m_g.geom.ellipseAt(0)->ra);
            hs << (org - ax * m_g.geom.ellipseAt(0)->ra);
            hs << (org + ay * m_g.geom.ellipseAt(0)->rb);
            hs << (org - ay * m_g.geom.ellipseAt(0)->rb);
            hs << (org + ax * (m_g.geom.ellipseAt(0)->ra + pad));
            break;
        }
        case HsCommData::ObjRect1: {
            const QRectF r = toQ(normalized(m_g.geom.rect1At(0)->r));
            hs << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft();
            hs << QPointF(r.center().x(), r.top());
            hs << QPointF(r.right(), r.center().y());
            hs << QPointF(r.center().x(), r.bottom());
            hs << QPointF(r.left(), r.center().y());
            break;
        }
        case HsCommData::ObjRect2: {
            const qreal c = qCos(m_g.geom.rect2At(0)->phi);
            const qreal s = qSin(m_g.geom.rect2At(0)->phi);
            const QPointF ax(c, s);
            const QPointF ay(-s, c);
            const QPointF org = toQ(m_g.geom.rect2At(0)->c);
            hs << (org + ax * m_g.geom.rect2At(0)->l1 + ay * m_g.geom.rect2At(0)->l2);
            hs << (org - ax * m_g.geom.rect2At(0)->l1 + ay * m_g.geom.rect2At(0)->l2);
            hs << (org - ax * m_g.geom.rect2At(0)->l1 - ay * m_g.geom.rect2At(0)->l2);
            hs << (org + ax * m_g.geom.rect2At(0)->l1 - ay * m_g.geom.rect2At(0)->l2);
            hs << (org + ax * m_g.geom.rect2At(0)->l1);
            hs << (org - ax * m_g.geom.rect2At(0)->l1);
            hs << (org + ay * m_g.geom.rect2At(0)->l2);
            hs << (org - ay * m_g.geom.rect2At(0)->l2);
            hs << (org + ax * (m_g.geom.rect2At(0)->l1 + pad));
            break;
        }
        case HsCommData::ObjLine:
            hs << toQ(m_g.geom.lineAt(0)->a) << toQ(m_g.geom.lineAt(0)->b);
            break;
        case HsCommData::ObjContour:
            for (size_t i = 0; i < m_g.geom.contourAt(0)->ptCount(); ++i) {
                hs << toQ(m_g.geom.contourAt(0)->ptAt(i));
            }
            break;
        default:
            break;
        }
        return hs;
    }
    switch (m_g.kind) {
    case HsCommData::KindArrow:
        if (!m_g.arrow.empty()) {
            hs << toQ(m_g.arrow[0].tail) << toQ(m_g.arrow[0].head);
        }
        break;
    case HsCommData::KindCross: {
        if (m_g.cross.empty()) {
            break;
        }
        const QPointF p = toQ(m_g.cross[0].center);
        const qreal ang = m_g.cross[0].angle;
        const QPointF ax(qCos(ang), qSin(ang));
        const QPointF ay(-qSin(ang), qCos(ang));
        hs << p;
        qreal s = m_g.cross[0].size;
        if (s < 0.0) {
            // 穿窗十字：在约 80 屏像素处放虚手柄，拖动后写成有限半宽
            s = 80.0 / qMax(m_viewScale, 1e-6);
        }
        hs << (p + ax * s);
        hs << (p - ax * s);
        hs << (p + ay * s);
        hs << (p - ay * s);
        hs << (p + ax * (s + pad));
        break;
    }
    case HsCommData::KindPoint:
        if (!m_g.point.empty()) {
            hs << toQ(m_g.point[0]);
        }
        break;
    case HsCommData::KindText:
        if (!m_g.text.empty()) {
            hs << toQ(m_g.text[0].anchor);
        }
        break;
    default:
        break;
    }
    return hs;
}

// 视口坐标命中手柄
HandleHit GraphicItem::hitHandle(const QPointF& viewPos, const QTransform& sceneToView, qreal hotPx) const
{
    HandleHit hit;
    if (m_g.role != HsCommData::RoleDrawing || !m_selected) {
        return hit;
    }
    const QVector<QPointF> hs = handleScenePositions();
    qreal best = hotPx;
    for (int i = 0; i < hs.size(); ++i) {
        const QPointF vp = sceneToView.map(hs[i]);
        const qreal dist = QLineF(vp, viewPos).length();
        if (dist <= best) {
            best = dist;
            hit.index = i;
            hit.scenePos = hs[i];
        }
    }
    return hit;
}

// 图像坐标命中轮廓或填充
bool GraphicItem::hitShape(const QPointF& scenePos, qreal imageTol) const
{
    const bool fillBody = (m_g.role == HsCommData::RoleDrawing);
    // Stack 混放：区域按像素判，其余用轮廓路径；填充样式判内部，否则只判描边带
    if (stackMixed(m_g)) {
        const int row = qFloor(scenePos.y());
        const int col = qFloor(scenePos.x());
        const int n = m_g.geom.count();
        for (int gi = 0; gi < n; ++gi) {
            if (m_g.geom.kindAt(gi) != HsCommData::ObjRegion) {
                continue;
            }
            const QRectF b = QRectF(toQ(m_g.geom.regionAt(gi)->bounds)).adjusted(-imageTol, -imageTol, imageTol, imageTol);
            if (b.contains(scenePos) && m_g.geom.regionAt(gi)->contains(row, col)) {
                return true;
            }
        }
        const QPainterPath p = outlinePath();
        if (m_g.geomStyle.draw == HsCommData::DrawFill) {
            if (p.contains(scenePos)) {
                return true;
            }
        }
        QPainterPathStroker stroker;
        stroker.setWidth(qMax(imageTol * 2.0, 1.0));
        return stroker.createStroke(p).contains(scenePos);
    }
    // 几何对象：ROI 与填充样式命中内部，否则只命中边线附近 imageTol 内
    if (m_g.kind == HsCommData::KindObject && m_g.geom.isInitialized()) {
    switch (m_g.geom.kind()) {
    case HsCommData::ObjCircle: {
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            const qreal d = QLineF(scenePos, toQ(m_g.geom.circleAt(gi)->c)).length();
            const bool hit = (fillBody || m_g.geomStyle.draw == HsCommData::DrawFill)
                ? (d <= m_g.geom.circleAt(gi)->r + imageTol)
                : (qAbs(d - m_g.geom.circleAt(gi)->r) <= imageTol);
            if (hit) {
                return true;
            }
        }
        return false;
    }
    case HsCommData::ObjEllipse:
    case HsCommData::ObjRect1:
    case HsCommData::ObjRect2:
    case HsCommData::ObjContour: {
        const QPainterPath p = outlinePath();
        bool anyClosed = false;
        if (m_g.geom.kind() == HsCommData::ObjContour) {
            const int n = m_g.geomSpan();
            for (int gi = 0; gi < n && !anyClosed; ++gi) {
                anyClosed = m_g.geom.contourAt(gi)->closed;
            }
        }
        if (fillBody || m_g.geomStyle.draw == HsCommData::DrawFill || anyClosed) {
            if (p.contains(scenePos)) {
                return true;
            }
        }
        QPainterPathStroker stroker;
        stroker.setWidth(qMax(imageTol * 2.0, 1.0));
        return stroker.createStroke(p).contains(scenePos);
    }
    case HsCommData::ObjRegion: {
        const int row = qFloor(scenePos.y());
        const int col = qFloor(scenePos.x());
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            const QRectF b = QRectF(toQ(m_g.geom.regionAt(gi)->bounds)).adjusted(-imageTol, -imageTol, imageTol, imageTol);
            if (b.contains(scenePos) && m_g.geom.regionAt(gi)->contains(row, col)) {
                return true;
            }
        }
        return false;
    }
    case HsCommData::ObjLine: {
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            const QPointF a = toQ(m_g.geom.lineAt(gi)->a);
            const QPointF b = toQ(m_g.geom.lineAt(gi)->b);
            const QPointF ab = b - a;
            const qreal den = QPointF::dotProduct(ab, ab);
            if (den < 1e-6) {
                if (QLineF(scenePos, a).length() <= imageTol) {
                    return true;
                }
                continue;
            }
            const qreal t = qBound(0.0, QPointF::dotProduct(scenePos - a, ab) / den, 1.0);
            if (QLineF(scenePos, a + t * ab).length() <= imageTol) {
                return true;
            }
        }
        return false;
    }
    default:
        break;
    }
    }
    // 附加字段：箭头、骨架按线段距离，十字按两轴，文字按估算的文字框，掩膜按像素
    switch (m_g.kind) {
    case HsCommData::KindArrow: {
        if (m_g.arrow.empty()) {
            return false;
        }
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            const HsCommData::HsArrow& arrow = m_g.arrow[static_cast<size_t>(gi)];
            const QPointF a = toQ(arrow.tail);
            const QPointF b = toQ(arrow.head);
            const QPointF ab = b - a;
            const qreal den = QPointF::dotProduct(ab, ab);
            if (den < 1e-6) {
                if (QLineF(scenePos, a).length() <= imageTol) {
                    return true;
                }
                continue;
            }
            const qreal t = qBound(0.0, QPointF::dotProduct(scenePos - a, ab) / den, 1.0);
            if (QLineF(scenePos, a + t * ab).length() <= imageTol) {
                return true;
            }
        }
        return false;
    }
    case HsCommData::KindCross: {
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            const HsCommData::HsCross& c = m_g.cross[static_cast<size_t>(gi)];
            const QPointF p = toQ(c.center);
            const QPointF d = scenePos - p;
            const qreal cph = qCos(c.angle);
            const qreal sph = qSin(c.angle);
            const qreal along = d.x() * cph + d.y() * sph;
            const qreal across = -d.x() * sph + d.y() * cph;
            if (c.size < 0.0) {
                if (qMin(qAbs(along), qAbs(across)) <= imageTol) {
                    return true;
                }
            } else if (qAbs(across) <= imageTol && qAbs(along) <= c.size + imageTol) {
                return true;
            } else if (qAbs(along) <= imageTol && qAbs(across) <= c.size + imageTol) {
                return true;
            }
        }
        return false;
    }
    case HsCommData::KindPoint: {
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            if (QLineF(scenePos, toQ(m_g.point[static_cast<size_t>(gi)])).length() <= qMax(imageTol, m_g.markerSize)) {
                return true;
            }
        }
        return false;
    }
    case HsCommData::KindText: {
        const qreal s = qMax(m_viewScale, 1e-6);
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            const HsCommData::HsText& tx = m_g.text[static_cast<size_t>(gi)];
            const qreal fontImg = qMax(8.0, m_g.textStyle.fontPx) / s;
            const qreal w = qMax(fontImg, static_cast<qreal>(tx.content.size()) * fontImg * 0.65);
            const QRectF r(tx.anchor.x, tx.anchor.y - fontImg * 0.25, w + 8.0 / s, fontImg * 1.5);
            if (r.adjusted(-imageTol, -imageTol, imageTol, imageTol).contains(scenePos)) {
                return true;
            }
        }
        return false;
    }
    case HsCommData::KindKeypoints: {
        const qreal r = qMax(imageTol, qMax(1.0, (m_g.geomStyle.widthPx > 0.0 ? m_g.geomStyle.widthPx : 1.0) * 2.0));
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            for (size_t i = 0; i < m_g.geom.contourAt(gi)->ptCount(); ++i) {
                if (QLineF(scenePos, toQ(m_g.geom.contourAt(gi)->ptAt(i))).length() <= r) {
                    return true;
                }
            }
        }
        return false;
    }
    case HsCommData::KindSkeleton: {
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            const HsCommData::HsContour* ctPts = m_g.geom.contourAt(gi);
            for (size_t i = 0; i + 1 < ctPts->ptCount(); i += 2) {
                const QPointF a = toQ(ctPts->ptAt(i));
                const QPointF b = toQ(ctPts->ptAt(i + 1));
                const QPointF ab = b - a;
                const qreal den = QPointF::dotProduct(ab, ab);
                if (den < 1e-6) {
                    if (QLineF(scenePos, a).length() <= imageTol) {
                        return true;
                    }
                    continue;
                }
                const qreal t = qBound(0.0, QPointF::dotProduct(scenePos - a, ab) / den, 1.0);
                if (QLineF(scenePos, a + t * ab).length() <= imageTol) {
                    return true;
                }
            }
        }
        return false;
    }
    case HsCommData::KindTextBadge: {
        const qreal h = qMax(8.0, (m_g.geomStyle.widthPx > 0.0 ? m_g.geomStyle.widthPx : 1.0) * 8.0);
        const int n = m_g.geomSpan();
        for (int gi = 0; gi < n; ++gi) {
            const HsCommData::HsText& tx = m_g.text[static_cast<size_t>(gi)];
            const qreal w = qMax(h, static_cast<qreal>(tx.content.size()) * h * 0.55);
            const QRectF r(tx.anchor.x, tx.anchor.y, w, h);
            if (r.adjusted(-imageTol, -imageTol, imageTol, imageTol).contains(scenePos)) {
                return true;
            }
        }
        return false;
    }
    case HsCommData::KindMask: {
        if (m_g.mask.isEmpty()) {
            return false;
        }
        const int x = qFloor(scenePos.x());
        const int y = qFloor(scenePos.y());
        if (x < 0 || y < 0 || x >= m_g.mask.width || y >= m_g.mask.height) {
            return false;
        }
        return m_g.mask.pixels[static_cast<size_t>(y) * static_cast<size_t>(m_g.mask.width) + static_cast<size_t>(x)] != 0;
    }
    default:
        break;
    }
    return false;
}

// 是否有效
bool HandleHit::isValid() const
{
    return index >= 0;
}



}
