/**
 * @file JobCanvas.cpp
 * @brief JobCanvas实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "JobCanvas.h"
#include "ToolBase.h"
#include "QtConv.h"
#include "UiText.h"
#include <QBrush>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFont>
#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QLineF>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsTextItem>
#include <QHash>
#include <QList>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QResizeEvent>
#include <QScrollBar>
#include <QToolTip>
#include <QVector>
#include <QWheelEvent>

namespace Vision {

static const qreal kBoxW = 176;
static const qreal kHeadH = 22;
static const qreal kNameH = 18;
static const qreal kPortH = 16;
static const qreal kPortPad = 4;
static const qreal kBotPad = 6;
static const qreal kPinR = 5;
static const qreal kEmptyLeafH = kHeadH + kNameH + kPortPad + kPortH + kBotPad;
static const qreal kVGap = 48; ///< 竖向间距
static const qreal kHGap = 16;
static const qreal kPad = 10;
static const qreal kTitleH = 20;

// 连线低于节点，高于顺序线
static const qreal kZOrderLine = -0.5;
static const qreal kZGroupFrame = 0.5;
static const qreal kZToolConfig = 1.0;
static const qreal kZWire = 1.5;
static const qreal kZPin = 2.0;
static const qreal kZWireHandle = 2.2;
static const qreal kZTempWire = 8.0;
static const qreal kZDragGroup = 0.5;
static const qreal kZDragTool = 6.0;

enum {
    RoleId = 0,
    RoleKind = 1,
    RolePort = 2,
    RoleDir = 3,
    RoleData = 4
};

enum {
    KindTool = 0,
    KindGroup = 1,
    KindPin = 2,
    KindWire = 3,
    KindWireHandle = 4,
    KindWireSegHandle = 5
};

/**
 * @class CanvasPort
 * @brief 画布上的一个端口
 */
struct CanvasPort {
    QString key;   ///< 端口键
    QString label; ///< 显示名
    PortData data; ///< 数据类型
    CanvasPort() : data(PortData::Image) {}
    CanvasPort(const QString& k, const QString& l, PortData d)
        : key(k)
        , label(l)
        , data(d)
    {}
};

struct GroupSize {
    qreal w; ///< 宽
    qreal h; ///< 高
    GroupSize() : w(kBoxW), h(kEmptyLeafH) {}
    GroupSize(qreal ww, qreal hh) : w(ww), h(hh) {}
};

/**
 * @brief 针脚形状：圆用于ROI，菱形用于其余，方块用于位姿
 */
enum PinShapeKind {
    PinShapeCircle = 0,  ///< 圆，用于ROI
    PinShapeDiamond,     ///< 菱形
    PinShapeSquare       ///< 方块，用于位姿或数值
};

struct PinLayout {
    QHash<QString, QGraphicsItem*> item; ///< 画布键到图元
};

// 是否叶子工具组
static bool canvasLeaf(const ToolGroup& g)
{
    return !g.toolId.isNull() && g.children.empty();
}

// 组是否要拆开画
static bool canvasSplit(const ToolGroup& g)
{
    return g.kind == GroupParallel || g.kind == GroupIf;
}

// 工具类型对应的标题色
static QColor headerColor(ToolType ty)
{
    switch (ty) {
    case ToolType::Acquire:
        return QColor(0x52, 0xC4, 0x1A);
    case ToolType::Preprocess:
        return QColor(0x13, 0xC2, 0xC2);
    case ToolType::Locate:
        return QColor(0x18, 0x90, 0xFF);
    case ToolType::Detect:
        return QColor(0x72, 0x2E, 0xD1);
    case ToolType::RstProcess:
        return QColor(0xFA, 0x8C, 0x16);
    case ToolType::Logic:
        return QColor(0x8C, 0x8C, 0x8C);
    case ToolType::Comm:
        return QColor(0x2F, 0x54, 0xEB);
    case ToolType::Calib:
        return QColor(0x13, 0xC2, 0xC2);
    case ToolType::Acquire3D:
        return QColor(0x00, 0x96, 0x88);
    case ToolType::Prepare3D:
        return QColor(0x13, 0xC2, 0xC2);
    case ToolType::Measure3D:
        return QColor(0xFA, 0xAD, 0x14);
    case ToolType::Align3D:
        return QColor(0x18, 0x90, 0xFF);
    case ToolType::Infer:
        return QColor(0xEB, 0x2F, 0x96);
    default:
        return QColor(0x8C, 0x8C, 0x8C);
    }
}

// 端口数据类型对应的颜色
static QColor pinColor(PortData d)
{
    switch (d) {
    case PortData::Region:
        return QColor(0x52, 0xC4, 0x1A);
    case PortData::Contour:
        return QColor(0xFA, 0x8C, 0x16);
    case PortData::Roi:
        return QColor(0xFA, 0xDB, 0x14);
    case PortData::Logic:
        return QColor(0x13, 0xC2, 0xC2);
    case PortData::Pose:
        return QColor(0x72, 0x2E, 0xD1);
    case PortData::Cloud:
        return QColor(0x00, 0x96, 0x88);
    case PortData::Pose3:
        return QColor(0x59, 0x7E, 0xF7);
    case PortData::Shape3:
        return QColor(0xFA, 0xAD, 0x14);
    default:
        return QColor(0xFF, 0xFF, 0xFF);
    }
}

// 端口数据类型对应的针脚形状
static PinShapeKind pinShapeKind(PortData d)
{
    switch (d) {
    case PortData::Image:
        return PinShapeDiamond;
    case PortData::Pose:
    case PortData::Logic:
        return PinShapeSquare;
    default:
        return PinShapeCircle;
    }
}

// 针脚在场景中的中心
static QPointF pinSceneCenter(QGraphicsItem* pin)
{
    return pin ? pin->sceneBoundingRect().center() : QPointF();
}

// 创建一个针脚图元
static QGraphicsItem* makePinItem(QGraphicsItem* parent, qreal cx, qreal cy, PortData data)
{
    const PinShapeKind shape = pinShapeKind(data);
    const QColor fill = pinColor(data);
    QPen pen;
    QBrush brush;
    if (data == PortData::Image) {
        brush = QBrush(Qt::white);
        pen = QPen(QColor(0x8C, 0x8C, 0x8C), 1.6);
    } else {
        brush = QBrush(fill);
        pen = QPen(fill.darker(118));
    }
    if (shape == PinShapeDiamond) {
        QGraphicsPolygonItem* pin = new QGraphicsPolygonItem(parent);
        QPolygonF poly;
        poly << QPointF(cx, cy - kPinR) << QPointF(cx + kPinR, cy)
             << QPointF(cx, cy + kPinR) << QPointF(cx - kPinR, cy);
        pin->setPolygon(poly);
        pin->setBrush(brush);
        pin->setPen(pen);
        return pin;
    }
    if (shape == PinShapeSquare) {
        QGraphicsRectItem* pin = new QGraphicsRectItem(
            cx - kPinR, cy - kPinR, kPinR * 2.0, kPinR * 2.0, parent);
        pin->setBrush(brush);
        pin->setPen(pen);
        return pin;
    }
    QGraphicsEllipseItem* pin = new QGraphicsEllipseItem(
        cx - kPinR, cy - kPinR, kPinR * 2.0, kPinR * 2.0, parent);
    pin->setBrush(brush);
    pin->setPen(pen);
    return pin;
}

// 端口类型不匹配时的提示
static QString typeMismatchTip(PortData from, PortData to)
{
    return T("类型不匹配：") + QString::fromUtf8(portDataTitle(from).c_str())
        + QString::fromUtf8(" -> ") + QString::fromUtf8(portDataTitle(to).c_str());
}

// 拖线颜色，类型不合用另一色
static QColor dragWireColor(PortData from, bool mismatch)
{
    if (mismatch) {
        return QColor(0xF5, 0x22, 0x2D);
    }
    if (from == PortData::Image) {
        return QColor(0x8C, 0x8C, 0x8C);
    }
    return pinColor(from);
}

// 正交折线加圆角
static void addRoundCorner(QPainterPath* path, const QPointF& from, const QPointF& corner,
                           const QPointF& to, qreal radius)
{
    const qreal len1 = QLineF(from, corner).length();
    const qreal len2 = QLineF(corner, to).length();
    qreal r = radius;
    if (r > len1 * 0.42) {
        r = len1 * 0.42;
    }
    if (r > len2 * 0.42) {
        r = len2 * 0.42;
    }
    if (r < 1.5 || len1 < 1.0 || len2 < 1.0) {
        path->lineTo(corner);
        return;
    }
    const QPointF u1((corner.x() - from.x()) / len1, (corner.y() - from.y()) / len1);
    const QPointF u2((to.x() - corner.x()) / len2, (to.y() - corner.y()) / len2);
    path->lineTo(QPointF(corner.x() - u1.x() * r, corner.y() - u1.y() * r));
    path->quadTo(corner, QPointF(corner.x() + u2.x() * r, corner.y() + u2.y() * r));
}

static const qreal kWireStub = 26.0;
static const qreal kWireClear = 16.0;
static const qreal kWireMerge = 8.0;

// 线段是否碰到矩形
static bool segmentHitsRect(const QPointF& p1, const QPointF& p2, const QRectF& r)
{
    if (r.isEmpty()) {
        return false;
    }
    if (r.contains(p1) || r.contains(p2)) {
        return true;
    }
    const QLineF seg(p1, p2);
    QPointF ign;
    if (seg.intersect(QLineF(r.topLeft(), r.topRight()), &ign) == QLineF::BoundedIntersection) {
        return true;
    }
    if (seg.intersect(QLineF(r.bottomLeft(), r.bottomRight()), &ign) == QLineF::BoundedIntersection) {
        return true;
    }
    if (seg.intersect(QLineF(r.topLeft(), r.bottomLeft()), &ign) == QLineF::BoundedIntersection) {
        return true;
    }
    if (seg.intersect(QLineF(r.topRight(), r.bottomRight()), &ign) == QLineF::BoundedIntersection) {
        return true;
    }
    return false;
}

// 去掉正交折线上的多余点
static QVector<QPointF> simplifyOrthoPoints(const QVector<QPointF>& in)
{
    QVector<QPointF> pts;
    for (int i = 0; i < in.size(); ++i) {
        if (!pts.isEmpty() && QLineF(pts.last(), in[i]).length() < 0.5) {
            continue;
        }
        pts.push_back(in[i]);
    }
    QVector<QPointF> out;
    for (int i = 0; i < pts.size(); ++i) {
        if (out.size() >= 2) {
            const QPointF& a = out[out.size() - 2];
            const QPointF& b = out[out.size() - 1];
            const QPointF& c = pts[i];
            const bool colH = qAbs(a.y() - b.y()) < 0.5 && qAbs(b.y() - c.y()) < 0.5;
            const bool colV = qAbs(a.x() - b.x()) < 0.5 && qAbs(b.x() - c.x()) < 0.5;
            if (colH || colV) {
                out.last() = c;
                continue;
            }
        }
        out.push_back(pts[i]);
    }
    return out;
}

// 正交点列转路径
static QPainterPath makeOrthoPath(const QVector<QPointF>& raw)
{
    const QVector<QPointF> pts = simplifyOrthoPoints(raw);
    QPainterPath path;
    if (pts.isEmpty()) {
        return path;
    }
    path.moveTo(pts.first());
    if (pts.size() == 1) {
        return path;
    }
    if (pts.size() == 2) {
        path.lineTo(pts[1]);
        return path;
    }
    const qreal radius = 10.0;
    for (int i = 1; i + 1 < pts.size(); ++i) {
        addRoundCorner(&path, pts[i - 1], pts[i], pts[i + 1], radius);
    }
    path.lineTo(pts.last());
    return path;
}

// 折线与障碍相交的扣分
static int scoreOrthoAgainstObstacles(const QVector<QPointF>& raw, const QVector<QRectF>& obstacles,
                                      const QRectF& srcBox, const QRectF& dstBox)
{
    const QVector<QPointF> pts = simplifyOrthoPoints(raw);
    int hits = 0;
    const int nSeg = pts.size() - 1;
    for (int i = 0; i < nSeg; ++i) {
        const QPointF& p1 = pts[i];
        const QPointF& p2 = pts[i + 1];
        const qreal segLen = QLineF(p1, p2).length();
        // 起点短段是引出，不当碰撞
        if (i == 0 && segLen <= kWireStub + 4.0) {
            continue;
        }
        if (i == nSeg - 1 && segLen <= kWireStub + 4.0) {
            continue;
        }
        for (int j = 0; j < obstacles.size(); ++j) {
            const QRectF& r = obstacles[j];
            if (segmentHitsRect(p1, p2, r)) {
                ++hits;
            }
        }
    }
    // 终点要落在目标框外
    if (!dstBox.isEmpty() && pts.size() >= 2) {
        const QPointF& pre = pts[pts.size() - 2];
        if (pre.x() > dstBox.center().x()) {
            hits += 2;
        }
    }
    Q_UNUSED(srcBox);
    return hits;
}

// 正交折线长度
static qreal orthoPathLength(const QVector<QPointF>& raw)
{
    const QVector<QPointF> pts = simplifyOrthoPoints(raw);
    qreal len = 0;
    for (int i = 0; i + 1 < pts.size(); ++i) {
        len += QLineF(pts[i], pts[i + 1]).length();
    }
    return len;
}

// 收集连线要躲开的节点框
static QVector<QRectF> collectWireObstacles(const QHash<QString, QGraphicsItem*>& toolBoxes,
                                            const QHash<QString, QGraphicsItem*>& groupFrames,
                                            const QString& fromId, const QString& toId)
{
    QVector<QRectF> out;
    QHash<QString, QGraphicsItem*>::const_iterator it = toolBoxes.constBegin();
    for (; it != toolBoxes.constEnd(); ++it) {
        if (!it.value() || it.key() == fromId || it.key() == toId) {
            continue;
        }
        out.push_back(it.value()->sceneBoundingRect().adjusted(-4, -4, 4, 4));
    }
    it = groupFrames.constBegin();
    for (; it != groupFrames.constEnd(); ++it) {
        if (!it.value()) {
            continue;
        }
        // 节点框外扩一圈
        out.push_back(it.value()->sceneBoundingRect().adjusted(-6, -6, 6, 6));
    }
    return out;
}

// 正交绕开节点框
static QVector<QPointF> routeOrthoAvoidObstacles(const QPointF& a, const QPointF& b,
                                                 const QVector<QRectF>& obstacles,
                                                 const QRectF& srcBox, const QRectF& dstBox)
{
    const qreal stub = kWireStub;
    const qreal clear = kWireClear;
    const qreal leaveX = a.x() + stub;
    const qreal enterX = b.x() - stub;

    qreal bandTop = qMin(a.y(), b.y()) - clear;
    qreal bandBot = qMax(a.y(), b.y()) + clear;
    qreal rightX = qMax(leaveX, enterX + clear);
    qreal leftX = qMin(leaveX, enterX);
    qreal aboveY = bandTop;
    qreal belowY = bandBot;
    for (int i = 0; i < obstacles.size(); ++i) {
        const QRectF& r = obstacles[i];
        if (!srcBox.isEmpty() && r == srcBox) {
            continue;
        }
        if (!dstBox.isEmpty() && r == dstBox) {
            continue;
        }
        if (r.bottom() < bandTop || r.top() > bandBot) {
            continue;
        }
        rightX = qMax(rightX, r.right() + clear);
        leftX = qMin(leftX, r.left() - clear);
        aboveY = qMin(aboveY, r.top() - clear);
        belowY = qMax(belowY, r.bottom() + clear);
    }
    if (!dstBox.isEmpty()) {
        rightX = qMax(rightX, dstBox.right() + clear);
        leftX = qMin(leftX, qMin(enterX, dstBox.left() - clear));
    }
    if (!srcBox.isEmpty()) {
        rightX = qMax(rightX, srcBox.right() + clear);
        leftX = qMin(leftX, srcBox.left() - clear);
        // 上方通线不要贴住源框
        if (aboveY > srcBox.top() - clear && aboveY < srcBox.bottom() + clear) {
            aboveY = srcBox.top() - clear;
        }
    }

    QVector<QVector<QPointF> > cands;
    // 右侧绕上：先向右，再从上进入
    {
        QVector<QPointF> p;
        p << a << QPointF(rightX, a.y()) << QPointF(rightX, aboveY)
          << QPointF(enterX, aboveY) << QPointF(enterX, b.y()) << b;
        cands.push_back(p);
    }
    // 右侧绕下：先向右，再从下进入
    {
        QVector<QPointF> p;
        p << a << QPointF(rightX, a.y()) << QPointF(rightX, belowY)
          << QPointF(enterX, belowY) << QPointF(enterX, b.y()) << b;
        cands.push_back(p);
    }
    // 从离开点绕上
    {
        QVector<QPointF> p;
        p << a << QPointF(leaveX, a.y()) << QPointF(leaveX, aboveY)
          << QPointF(enterX, aboveY) << QPointF(enterX, b.y()) << b;
        cands.push_back(p);
    }
    // 从离开点绕下
    {
        QVector<QPointF> p;
        p << a << QPointF(leaveX, a.y()) << QPointF(leaveX, belowY)
          << QPointF(enterX, belowY) << QPointF(enterX, b.y()) << b;
        cands.push_back(p);
    }
    // 再向左绕上
    {
        QVector<QPointF> p;
        p << a << QPointF(leaveX, a.y()) << QPointF(leaveX, aboveY)
          << QPointF(leftX, aboveY) << QPointF(leftX, b.y())
          << QPointF(enterX, b.y()) << b;
        cands.push_back(p);
    }
    {
        QVector<QPointF> p;
        p << a << QPointF(leaveX, a.y()) << QPointF(leaveX, belowY)
          << QPointF(leftX, belowY) << QPointF(leftX, b.y())
          << QPointF(enterX, b.y()) << b;
        cands.push_back(p);
    }
    // 中间高度走一条 C 形
    {
        qreal midY = (a.y() + b.y()) * 0.5;
        if (qAbs(b.y() - a.y()) < 28.0) {
            midY = a.y() + ((b.y() >= a.y()) ? 36.0 : -36.0);
        }
        QVector<QPointF> p;
        p << a << QPointF(leaveX, a.y()) << QPointF(leaveX, midY)
          << QPointF(enterX, midY) << QPointF(enterX, b.y()) << b;
        cands.push_back(p);
    }
    // 再试两条更远的绕行

    for (int k = 1; k <= 2; ++k) {
        const qreal rx = rightX + 40.0 * k;
        QVector<QPointF> p1;
        p1 << a << QPointF(rx, a.y()) << QPointF(rx, aboveY)
           << QPointF(enterX, aboveY) << QPointF(enterX, b.y()) << b;
        cands.push_back(p1);
        QVector<QPointF> p2;
        p2 << a << QPointF(rx, a.y()) << QPointF(rx, belowY)
           << QPointF(enterX, belowY) << QPointF(enterX, b.y()) << b;
        cands.push_back(p2);
    }

    int bestHits = 100000;
    qreal bestLen = 1e18;
    QVector<QPointF> best = cands.first();
    for (int i = 0; i < cands.size(); ++i) {
        const int hits = scoreOrthoAgainstObstacles(cands[i], obstacles, srcBox, dstBox);
        const qreal len = orthoPathLength(cands[i]);
        if (hits < bestHits || (hits == bestHits && len < bestLen)) {
            bestHits = hits;
            bestLen = len;
            best = cands[i];
        }
    }
    return simplifyOrthoPoints(best);
}

// 网格吸附（连线编辑）
static qreal snapWireGrid(qreal v)
{
    const qreal g = 8.0;
    return qRound(v / g) * g;
}

// 场景点网格吸附
static QPointF snapWireGridPt(const QPointF& p)
{
    return QPointF(snapWireGrid(p.x()), snapWireGrid(p.y()));
}

// RoutePoint 列表转 QPointF
static QVector<QPointF> routeMidsToQ(const std::vector<RoutePoint>& mids)
{
    QVector<QPointF> out;
    out.reserve(static_cast<int>(mids.size()));
    for (size_t i = 0; i < mids.size(); ++i) {
        out.push_back(QPointF(mids[i].x, mids[i].y));
    }
    return out;
}

// QPointF 中间点转 RoutePoint
static std::vector<RoutePoint> routeMidsFromQ(const QVector<QPointF>& mids)
{
    std::vector<RoutePoint> out;
    out.reserve(static_cast<size_t>(mids.size()));
    for (int i = 0; i < mids.size(); ++i) {
        out.push_back(RoutePoint(mids[i].x(), mids[i].y()));
    }
    return out;
}

// 保证正交：不相邻正交时插入水平优先拐点
static QVector<QPointF> ensureOrthoChain(const QVector<QPointF>& in)
{
    if (in.size() < 2) {
        return in;
    }
    QVector<QPointF> out;
    out.push_back(in.first());
    for (int i = 1; i < in.size(); ++i) {
        const QPointF prev = out.last();
        const QPointF next = in[i];
        if (qAbs(prev.x() - next.x()) < 0.5 || qAbs(prev.y() - next.y()) < 0.5) {
            out.push_back(next);
        } else {
            out.push_back(QPointF(next.x(), prev.y()));
            out.push_back(next);
        }
    }
    return simplifyOrthoPoints(out);
}

// 由中间拐点拼出完整折线
static QVector<QPointF> fullRouteFromMids(const QPointF& a, const QPointF& b,
                                          const QVector<QPointF>& mids)
{
    QVector<QPointF> pts;
    pts.reserve(mids.size() + 2);
    pts.push_back(a);
    for (int i = 0; i < mids.size(); ++i) {
        pts.push_back(mids[i]);
    }
    pts.push_back(b);
    return ensureOrthoChain(pts);
}

// 从完整折线取出中间拐点
static QVector<QPointF> midsFromFullRoute(const QVector<QPointF>& full)
{
    QVector<QPointF> pts = simplifyOrthoPoints(full);
    QVector<QPointF> mids;
    if (pts.size() <= 2) {
        return mids;
    }
    for (int i = 1; i + 1 < pts.size(); ++i) {
        mids.push_back(pts[i]);
    }
    return mids;
}

// 无障碍时的默认 C 形中间点
static QVector<QPointF> defaultWireMids(const QPointF& a, const QPointF& b)
{
    const qreal stub = kWireStub;
    qreal midY = (a.y() + b.y()) * 0.5;
    const qreal dy = b.y() - a.y();
    if (qAbs(dy) < 28.0) {
        midY = a.y() + ((dy >= 0.0) ? 36.0 : -36.0);
    }
    QVector<QPointF> mids;
    mids.push_back(QPointF(a.x() + stub, midY));
    mids.push_back(QPointF(b.x() - stub, midY));
    return mids;
}

// 自动正交折线路径（拖线橡皮筋）
static QPainterPath makeWirePathAuto(const QPointF& a, const QPointF& b)
{
    return makeOrthoPath(fullRouteFromMids(a, b, defaultWireMids(a, b)));
}

// 端点加中间拐点，拖动过程中不插点、不合并
static QVector<QPointF> rawFullFromMids(const QPointF& a, const QPointF& b,
                                        const QVector<QPointF>& mids)
{
    QVector<QPointF> pts;
    pts.reserve(mids.size() + 2);
    pts.push_back(a);
    for (int i = 0; i < mids.size(); ++i) {
        pts.push_back(mids[i]);
    }
    pts.push_back(b);
    return pts;
}

// 把完整折线写回中间拐点
static void writeMidsFromRaw(QVector<QPointF>* mids, const QVector<QPointF>& full)
{
    if (!mids) {
        return;
    }
    mids->clear();
    if (full.size() <= 2) {
        return;
    }
    for (int i = 1; i + 1 < full.size(); ++i) {
        mids->push_back(full[i]);
    }
}

// 按下时在端口处拆段：source 插入会挪动后面的段下标
struct PinStubSplit {
    int segIndex;
    int bumpAt;
    int bumpCount;
};

// 平移一段正交直线
static void shiftStraightRun(QVector<QPointF>* full, int segIndex, bool horiz, qreal value,
                             int skipA, int skipB)
{
    if (!full || segIndex < 0 || segIndex + 1 >= full->size()) {
        return;
    }
    const int last = full->size() - 1;
    const qreal key = horiz ? (*full)[segIndex].y() : (*full)[segIndex].x();
    for (int i = segIndex; i > 0; --i) {
        const qreal v = horiz ? (*full)[i].y() : (*full)[i].x();
        if (qAbs(v - key) > 0.5) {
            break;
        }
        if (i == skipA || i == skipB) {
            continue;
        }
        if (horiz) {
            (*full)[i].setY(value);
        } else {
            (*full)[i].setX(value);
        }
    }
    for (int i = segIndex + 1; i < last; ++i) {
        const qreal v = horiz ? (*full)[i].y() : (*full)[i].x();
        if (qAbs(v - key) > 0.5) {
            break;
        }
        if (i == skipA || i == skipB) {
            continue;
        }
        if (horiz) {
            (*full)[i].setY(value);
        } else {
            (*full)[i].setX(value);
        }
    }
}

// 拖线时跳过两端引出的短段
static void pinStubSkips(const QVector<QPointF>& full, int* skipA, int* skipB)
{
    if (skipA) {
        *skipA = -1;
    }
    if (skipB) {
        *skipB = -1;
    }
    const int last = full.size() - 1;
    if (last >= 3 && qAbs(full[0].y() - full[1].y()) <= 0.5 && qAbs(full[1].x() - full[2].x()) <= 0.5) {
        if (skipA) {
            *skipA = 1;
        }
    }
    if (last >= 3 && qAbs(full[last].y() - full[last - 1].y()) <= 0.5
        && qAbs(full[last - 1].x() - full[last - 2].x()) <= 0.5) {
        if (skipB) {
            *skipB = last - 1;
        }
    }
}

// 吸附到相邻正交线
static qreal magnetNeighbor(const QVector<QPointF>& full, int segIndex, bool horiz, qreal value)
{
    const int last = full.size() - 1;
    if (segIndex < 0 || segIndex >= last) {
        return value;
    }
    qreal best = value;
    qreal bestD = kWireMerge + 0.01;
    const qreal key = horiz ? full[segIndex].y() : full[segIndex].x();
    int i0 = segIndex;
    while (i0 > 0) {
        const qreal v = horiz ? full[i0 - 1].y() : full[i0 - 1].x();
        if (qAbs(v - key) > 0.5) {
            break;
        }
        --i0;
    }
    if (i0 > 0) {
        const bool perp = horiz ? (qAbs(full[i0 - 1].x() - full[i0].x()) <= 0.5)
                                : (qAbs(full[i0 - 1].y() - full[i0].y()) <= 0.5);
        if (perp) {
            const qreal n = horiz ? full[i0 - 1].y() : full[i0 - 1].x();
            const qreal d = qAbs(value - n);
            if (d <= kWireMerge && d < bestD) {
                bestD = d;
                best = n;
            }
        }
    }
    int i1 = segIndex + 1;
    while (i1 < last) {
        const qreal v = horiz ? full[i1 + 1].y() : full[i1 + 1].x();
        if (qAbs(v - key) > 0.5) {
            break;
        }
        ++i1;
    }
    if (i1 < last) {
        const bool perp = horiz ? (qAbs(full[i1].x() - full[i1 + 1].x()) <= 0.5)
                                : (qAbs(full[i1].y() - full[i1 + 1].y()) <= 0.5);
        if (perp) {
            const qreal n = horiz ? full[i1 + 1].y() : full[i1 + 1].x();
            const qreal d = qAbs(value - n);
            if (d <= kWireMerge && d < bestD) {
                best = n;
            }
        }
    }
    return best;
}

// 两端相邻中间只剩很短的一段时，收成同一条
static void collapseShortJogs(QVector<QPointF>* full)
{
    if (!full || full->size() < 4) {
        return;
    }
    for (int i = 0; i + 3 < full->size(); ++i) {
        const QPointF a = (*full)[i];
        const QPointF b = (*full)[i + 1];
        const QPointF c = (*full)[i + 2];
        const QPointF d = (*full)[i + 3];
        const bool h1 = qAbs(a.y() - b.y()) <= 0.5 && qAbs(a.x() - b.x()) > 0.5;
        const bool vj = qAbs(b.x() - c.x()) <= 0.5 && qAbs(b.y() - c.y()) > 0.5;
        const bool h2 = qAbs(c.y() - d.y()) <= 0.5 && qAbs(c.x() - d.x()) > 0.5;
        if (h1 && vj && h2 && qAbs(b.y() - c.y()) <= kWireMerge) {
            const qreal y = (QLineF(a, b).length() >= QLineF(c, d).length()) ? b.y() : c.y();
            (*full)[i + 1].setY(y);
            (*full)[i + 2].setY(y);
        }
        const bool v1 = qAbs(a.x() - b.x()) <= 0.5 && qAbs(a.y() - b.y()) > 0.5;
        const bool hj = qAbs(b.y() - c.y()) <= 0.5 && qAbs(b.x() - c.x()) > 0.5;
        const bool v2 = qAbs(c.x() - d.x()) <= 0.5 && qAbs(c.y() - d.y()) > 0.5;
        if (v1 && hj && v2 && qAbs(b.x() - c.x()) <= kWireMerge) {
            const qreal x = (QLineF(a, b).length() >= QLineF(c, d).length()) ? b.x() : c.x();
            (*full)[i + 1].setX(x);
            (*full)[i + 2].setX(x);
        }
    }
}

// 拖动时拆开针脚引出段
static PinStubSplit splitPinStubForDrag(QVector<QPointF>* mids, int segIndex,
                                        const QPointF& a, const QPointF& b)
{
    PinStubSplit r;
    r.segIndex = segIndex;
    r.bumpAt = -1;
    r.bumpCount = 0;
    if (!mids) {
        return r;
    }
    QVector<QPointF> full = rawFullFromMids(a, b, *mids);
    int last = full.size() - 1;
    if (segIndex < 0 || segIndex >= last) {
        return r;
    }
    bool changed = false;
    if (segIndex + 1 == last && qAbs(full[segIndex].x() - full[last].x()) <= 0.5
        && qAbs(full[segIndex].y() - full[last].y()) > 0.5) {
        const qreal px = full[segIndex].x();
        const int at = last;
        full.insert(at, QPointF(b.x() - kWireStub, b.y()));
        full.insert(at, QPointF(px, b.y()));
        changed = true;
    } else if (segIndex == 0 && qAbs(full[0].y() - full[1].y()) <= 0.5) {
        const qreal span = qAbs(full[1].x() - full[0].x());
        if (span > kWireStub + 8.0) {
            const QPointF stub(a.x() + kWireStub, a.y());
            const QPointF far = full[1];
            full[1] = stub;
            full.insert(2, stub);
            full.insert(3, far);
            r.bumpAt = 1;
            r.bumpCount = 2;
            r.segIndex = 2;
            changed = true;
        } else if (full.size() >= 3 && qAbs(full[2].x() - full[1].x()) > 0.5) {
            full.insert(2, full[1]);
            r.bumpAt = 2;
            r.bumpCount = 1;
            r.segIndex = 1;
            changed = true;
        }
    }
    last = full.size() - 1;
    if (r.segIndex + 1 == last && qAbs(full[r.segIndex].y() - full[last].y()) <= 0.5
        && qAbs(full[r.segIndex].x() - full[last].x()) > 0.5) {
        const qreal span = qAbs(full[last].x() - full[r.segIndex].x());
        if (span > kWireStub + 8.0) {
            const QPointF stub(b.x() - kWireStub, b.y());
            const int at = last;
            full.insert(at, stub);
            full.insert(at, stub);
            changed = true;
        }
    }
    if (changed) {
        writeMidsFromRaw(mids, full);
    }
    return r;
}

// 拖动拐点后更新中间点
static void applyVertexDrag(QVector<QPointF>* mids, const QVector<QPointF>& base,
                            int index, const QPointF& press, const QPointF& now,
                            const QPointF& a, const QPointF& b)
{
    if (!mids || index < 0 || index >= base.size()) {
        return;
    }
    QVector<QPointF> full = rawFullFromMids(a, b, base);
    const int vi = index + 1;
    const int last = full.size() - 1;
    if (vi <= 0 || vi >= last) {
        return;
    }
    const QPointF cur = full[vi];
    const QPointF left = full[vi - 1];
    const QPointF right = full[vi + 1];
    const bool leftH = qAbs(left.y() - cur.y()) <= 0.5;
    const bool leftV = qAbs(left.x() - cur.x()) <= 0.5;
    const bool rightH = qAbs(right.y() - cur.y()) <= 0.5;
    const bool rightV = qAbs(right.x() - cur.x()) <= 0.5;
    const qreal dx = now.x() - press.x();
    const qreal dy = now.y() - press.y();
    int skipA = -1;
    int skipB = -1;
    pinStubSkips(full, &skipA, &skipB);
    if ((leftV || rightV) && !(vi == 1 && leftV) && !(vi == last - 1 && rightV)) {
        shiftStraightRun(&full, vi, false, cur.x() + dx, -1, -1);
    }
    if ((leftH || rightH) && !(vi == 1 && leftH) && !(vi == last - 1 && rightH)) {
        shiftStraightRun(&full, vi, true, cur.y() + dy, skipA, skipB);
    }
    writeMidsFromRaw(mids, full);
}

// 拖整段：竖段只改 X，横段只改 Y。其它段按当前段位移
static void applySegmentDrag(QVector<QPointF>* mids, const QVector<QPointF>& base,
                             int fullSegIndex, const QPointF& press, const QPointF& now,
                             const QPointF& a, const QPointF& b)
{
    if (!mids) {
        return;
    }
    QVector<QPointF> full = rawFullFromMids(a, b, base);
    const int last = full.size() - 1;
    if (fullSegIndex < 0 || fullSegIndex + 1 > last) {
        return;
    }
    const QPointF p0 = full[fullSegIndex];
    const QPointF p1 = full[fullSegIndex + 1];
    if (qAbs(p0.y() - p1.y()) <= 0.5) {
        int skipA = -1;
        int skipB = -1;
        pinStubSkips(full, &skipA, &skipB);
        const qreal y = magnetNeighbor(full, fullSegIndex, true, p0.y() + now.y() - press.y());
        shiftStraightRun(&full, fullSegIndex, true, y, skipA, skipB);
    } else if (qAbs(p0.x() - p1.x()) <= 0.5) {
        const qreal x = magnetNeighbor(full, fullSegIndex, false, p0.x() + now.x() - press.x());
        shiftStraightRun(&full, fullSegIndex, false, x, -1, -1);
    }
    Q_UNUSED(a);
    Q_UNUSED(b);
    writeMidsFromRaw(mids, full);
}
// 在整条路径某段中点插入拐点
static QVector<QPointF> insertMidOnSegment(const QVector<QPointF>& mids, int fullSegIndex,
                                           const QPointF& a, const QPointF& b, const QPointF& at)
{
    QVector<QPointF> full = rawFullFromMids(a, b, mids);
    if (fullSegIndex < 0 || fullSegIndex + 1 >= full.size()) {
        return mids;
    }
    const QPointF mid = snapWireGridPt(at);
    full.insert(fullSegIndex + 1, mid);
    return midsFromFullRoute(full);
}

// 删除一个中间拐点并共线合并
static QVector<QPointF> removeMidAt(const QVector<QPointF>& mids, int index,
                                    const QPointF& a, const QPointF& b)
{
    if (index < 0 || index >= mids.size()) {
        return mids;
    }
    QVector<QPointF> next = mids;
    next.remove(index);
    return midsFromFullRoute(rawFullFromMids(a, b, next));
}
// 连线颜色
static QColor wireColor(PortData d)
{
    if (d == PortData::Image) {
        return QColor(0x8C, 0x8C, 0x8C);
    }
    return pinColor(d);
}

// 端口类型的短标签
static QString portKeyLabel(PortData data)
{
    return QString::fromUtf8(portDataTitle(data).c_str());
}

// 针脚键：工具、端口、方向
static QString pinMapKey(const QString& toolId, const QString& port, int dir)
{
    return toolId + QChar('\t') + port + QChar('\t') + QString::number(dir);
}

// 收集工具的输入输出端口
static void collectCanvasPorts(const ToolCfg* t, QVector<CanvasPort>* ins, QVector<CanvasPort>* outs)
{
    ins->clear();
    outs->clear();
    const ToolType ty = t ? t->type : ToolType::Acquire;
    const std::vector<PortDesc> ports = defaultPortsForType(ty);
    for (int i = 0; i < static_cast<int>(ports.size()); ++i) {
        const PortDesc& p = ports[static_cast<size_t>(i)];
        const CanvasPort row(qs(p.key()), portKeyLabel(p.data()), p.data());
        if (p.dir() == PortDir::In) {
            ins->push_back(row);
        } else {
            outs->push_back(row);
        }
    }
}

// 量工具节点的宽高
static GroupSize measureToolBox(const ToolCfg* t)
{
    QVector<CanvasPort> ins;
    QVector<CanvasPort> outs;
    collectCanvasPorts(t, &ins, &outs);
    const bool disabled = (t && !t->enabled);
    int rows = ins.size();
    if (outs.size() > rows) {
        rows = outs.size();
    }
    if (rows < 1) {
        rows = 1;
    }
    const qreal h = kHeadH + kNameH + kPortPad + rows * kPortH + kBotPad;
    return GroupSize(kBoxW, h);
}

// 量组框的宽高
static GroupSize measureGroup(const JobCfg& job, const ToolGroup& g)
{
    if (canvasLeaf(g)) {
        return measureToolBox(job.findTool(g.toolId));
    }
    if (canvasSplit(g)) {
        if (g.children.empty()) {
            return GroupSize(kBoxW + kPad * 2, kEmptyLeafH + kTitleH + kPad * 2);
        }
        qreal w = kPad * 2;
        qreal h = 0;
        for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
            const GroupSize c = measureGroup(job, g.children[i]);
            if (i > 0) {
                w += kHGap;
            }
            w += c.w;
            if (c.h > h) {
                h = c.h;
            }
        }
        return GroupSize(w, h + kTitleH + kPad * 2);
    }
    if (g.children.empty()) {
        return GroupSize(kBoxW, kEmptyLeafH + kTitleH);
    }
    qreal w = kBoxW;
    qreal h = kTitleH;
    for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
        const GroupSize c = measureGroup(job, g.children[i]);
        if (c.w > w) {
            w = c.w;
        }
        if (i > 0) {
            h += kVGap;
        }
        h += c.h;
    }
    return GroupSize(w, h);
}

// 在节点上加一个针脚
static void addPin(QGraphicsRectItem* box, qreal cx, qreal cy, PortData data,
                   const QString& toolId, const QString& port, int dir, PinLayout* pins)
{
    QGraphicsItem* pin = makePinItem(box, cx, cy, data);
    pin->setZValue(kZPin);
    pin->setData(RoleId, toolId);
    pin->setData(RoleKind, KindPin);
    pin->setData(RolePort, port);
    pin->setData(RoleDir, dir);
    pin->setData(RoleData, static_cast<int>(data));
    pin->setCursor(dir == 1 ? Qt::CrossCursor : Qt::PointingHandCursor);
    if (pins) {
        pins->item.insert(pinMapKey(toolId, port, dir), pin);
    }
}

// 图元在场景中的左上角
static QPointF itemSceneTopLeft(QGraphicsItem* item)
{
    QGraphicsRectItem* box = qgraphicsitem_cast<QGraphicsRectItem*>(item);
    if (box) {
        return box->mapToScene(box->rect().topLeft());
    }
    return item ? item->scenePos() : QPointF();
}

// 设置图元在场景中的左上角
static void setItemSceneTopLeft(QGraphicsItem* item, const QPointF& sceneTL)
{
    if (!item) {
        return;
    }
    QGraphicsRectItem* box = qgraphicsitem_cast<QGraphicsRectItem*>(item);
    QGraphicsItem* par = item->parentItem();
    const QPointF parentTL = par ? par->mapFromScene(sceneTL) : sceneTL;
    if (box) {
        item->setPos(parentTL - box->rect().topLeft());
    } else {
        item->setPos(parentTL);
    }
}

// 组或工具的画布键
static QString canvasNodeKey(const ToolGroup& g)
{
    if (canvasLeaf(g)) {
        return qs(g.toolId);
    }
    return qs(g.id);
}

// 转成流程节点矩形
static QGraphicsRectItem* asFlowNode(QGraphicsItem* item)
{
    return qgraphicsitem_cast<QGraphicsRectItem*>(item);
}

// 节点底边中点
static QPointF flowNodeBottom(QGraphicsItem* item)
{
    QGraphicsRectItem* box = asFlowNode(item);
    if (!box) {
        return QPointF();
    }
    const QRectF r = box->rect();
    return box->mapToScene(QPointF(r.center().x(), r.bottom()));
}

// 节点顶边中点
static QPointF flowNodeTop(QGraphicsItem* item)
{
    QGraphicsRectItem* box = asFlowNode(item);
    if (!box) {
        return QPointF();
    }
    const QRectF r = box->rect();
    return box->mapToScene(QPointF(r.center().x(), r.top()));
}

// 执行顺序虚线箭头路径
static QPainterPath makeExecOrderPath(const QPointF& from, const QPointF& to)
{
    QPainterPath path;
    QLineF seg(from, to);
    const qreal len = seg.length();
    if (len < 0.5) {
        return path;
    }
    // 箭头画在线段末端
    QLineF shaft = seg;
    if (len > 10.0) {
        shaft.setLength(len - 7.0);
    }
    path.moveTo(shaft.p1());
    path.lineTo(shaft.p2());

    const qreal arrowLen = 9.0;
    const qreal arrowHalfW = 4.5;
    QLineF unit(seg);
    unit.setLength(1.0);
    const QPointF dir = unit.p2() - unit.p1();
    const QPointF n(-dir.y(), dir.x());
    const QPointF base = to - dir * arrowLen;
    path.moveTo(to);
    path.lineTo(base + n * arrowHalfW);
    path.lineTo(base - n * arrowHalfW);
    path.closeSubpath();
    return path;
}

// 画一条执行顺序线
static void addExecOrderLine(QGraphicsScene* scene, QGraphicsItem* from, QGraphicsItem* to,
                             QVector<CanvasOrderRef>* out)
{
    if (!scene || !from || !to) {
        return;
    }
    QPointF a = flowNodeBottom(from);
    QPointF b = flowNodeTop(to);
    if (a.isNull() || b.isNull()) {
        return;
    }
    a.setY(a.y() + 3.0);
    b.setY(b.y() - 3.0);
    QGraphicsPathItem* line = scene->addPath(makeExecOrderPath(a, b));
    const QColor orderGray(0xBF, 0xBF, 0xBF);
    QPen pen(orderGray, 1.2, Qt::DashLine);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);
    line->setPen(pen);
    line->setBrush(orderGray); // 顺序线用灰色填箭头
    line->setZValue(kZOrderLine);
    line->setAcceptedMouseButtons(Qt::NoButton);
    if (out) {
        CanvasOrderRef ref;
        ref.item = line;
        ref.from = from;
        ref.to = to;
        out->push_back(ref);
    }
}

// 按顺序把相邻节点连起来
static void linkSequentialNodes(QGraphicsScene* scene, const QHash<QString, QGraphicsItem*>& nodes,
                                const std::vector<ToolGroup>& siblings,
                                QVector<CanvasOrderRef>* orderLines)
{
    QGraphicsItem* prev = nullptr;
    for (int i = 0; i < static_cast<int>(siblings.size()); ++i) {
        const ToolGroup& g = siblings[static_cast<size_t>(i)];
        QGraphicsItem* cur = nodes.value(canvasNodeKey(g));
        if (prev && cur) {
            addExecOrderLine(scene, prev, cur, orderLines);
        }
        if (cur) {
            prev = cur;
        }
    }
}

// 组框缩到包住子节点
static void fitFrameToChildren(QGraphicsRectItem* frame)
{
    if (!frame) {
        return;
    }
    const QList<QGraphicsItem*> kids = frame->childItems();
    if (kids.isEmpty()) {
        return;
    }
    QRectF bound;
    bool any = false;
    QGraphicsTextItem* title = nullptr;
    for (int i = 0; i < kids.size(); ++i) {
        QGraphicsItem* c = kids[i];
        if (!c) {
            continue;
        }
        if (c->data(RoleId).toString().isEmpty()) {
            QGraphicsTextItem* tx = qgraphicsitem_cast<QGraphicsTextItem*>(c);
            if (tx) {
                title = tx;
            }
            continue;
        }
        const int k = c->data(RoleKind).toInt();
        if (k != KindTool && k != KindGroup) {
            continue;
        }
        const QRectF r = c->mapRectToParent(c->boundingRect());
        if (!any) {
            bound = r;
            any = true;
        } else {
            bound = bound.united(r);
        }
    }
    if (!any) {
        return;
    }
    bound.adjust(-kPad, -(kTitleH + 4.0), kPad, kPad);
    if (bound.width() < kBoxW + kPad * 2.0) {
        bound.setWidth(kBoxW + kPad * 2.0);
    }
    if (bound.height() < kTitleH + kEmptyLeafH) {
        bound.setHeight(kTitleH + kEmptyLeafH);
    }
    frame->setRect(bound);
    if (title) {
        title->setPos(bound.left() + 8.0, bound.top() + 2.0);
    }
}

// 向上找所属组框
static QGraphicsRectItem* parentGroupFrame(QGraphicsItem* it)
{
    if (!it) {
        return 0;
    }
    QGraphicsItem* p = it->parentItem();
    while (p) {
        if (p->data(RoleKind).toInt() == KindGroup) {
            return qgraphicsitem_cast<QGraphicsRectItem*>(p);
        }
        p = p->parentItem();
    }
    return 0;
}

// 收集组里的工具框
static void collectGroupToolBoxes(QGraphicsItem* root, QVector<QGraphicsRectItem*>* boxes)
{
    if (!root || !boxes) {
        return;
    }
    if (root->data(RoleKind).toInt() == KindTool) {
        QGraphicsRectItem* box = qgraphicsitem_cast<QGraphicsRectItem*>(root);
        if (box) {
            boxes->push_back(box);
        }
    }
    const QList<QGraphicsItem*> kids = root->childItems();
    for (int i = 0; i < kids.size(); ++i) {
        collectGroupToolBoxes(kids[i], boxes);
    }
}

// 画一个工具节点
static QGraphicsRectItem* addToolConfig(QGraphicsScene* scene, qreal x, qreal y,
                        const ToolCfg* t, int index, const QString& id,
                        bool sel, PinLayout* pins, QGraphicsItem* parent)
{
    qreal nx = x;
    qreal ny = y;
    if (t && t->canvasPlaced) {
        nx = t->canvasX;
        ny = t->canvasY;
    }
    const GroupSize sz = measureToolBox(t);
    QVector<CanvasPort> ins;
    QVector<CanvasPort> outs;
    collectCanvasPorts(t, &ins, &outs);
    const bool disabled = (t && !t->enabled);

    QPen pen(sel ? QColor(0x18, 0x90, 0xFF) : QColor(0xD9, 0xD9, 0xD9));
    pen.setWidth(sel ? 2 : 1);
    QGraphicsRectItem* box = nullptr;
    if (parent) {
        box = new QGraphicsRectItem(nx, ny, sz.w, sz.h, parent);
        box->setPen(pen);
        box->setBrush(QBrush(disabled ? QColor(0xF0, 0xF0, 0xF0) : Qt::white));
    } else {
        box = scene->addRect(nx, ny, sz.w, sz.h, pen, QBrush(disabled ? QColor(0xF0, 0xF0, 0xF0) : Qt::white));
    }
    box->setData(RoleId, id);
    box->setData(RoleKind, KindTool);
    box->setFlag(QGraphicsItem::ItemIsSelectable, true);
    box->setCursor(Qt::OpenHandCursor);
    box->setZValue(kZToolConfig);
    if (disabled) {
        box->setOpacity(0.55);
    }

    const ToolType ty = t ? t->type : ToolType::Acquire;
    QGraphicsRectItem* head = new QGraphicsRectItem(nx, ny, sz.w, kHeadH, box);
    head->setBrush(QBrush(disabled ? QColor(0xB0, 0xB0, 0xB0) : headerColor(ty)));
    head->setPen(Qt::NoPen);
    head->setCursor(Qt::OpenHandCursor);

    QFont headFont;
    headFont.setPixelSize(11);
    headFont.setBold(true);
    QString headText = QString::number(index) + QLatin1Char(' ')
                       + QString::fromUtf8(toolTypeTitle(ty).c_str());
    QGraphicsSimpleTextItem* headLab = new QGraphicsSimpleTextItem(headText, box);
    headLab->setFont(headFont);
    headLab->setBrush(QBrush(Qt::white));
    headLab->setPos(nx + 8, ny + 4);
    headLab->setCursor(Qt::OpenHandCursor);

    const QString name = t ? qs(t->name) : T("未命名");
    QGraphicsTextItem* nameLab = scene->addText(name);
    nameLab->setDefaultTextColor(disabled ? QColor(0xA0, 0xA0, 0xA0) : QColor(0x33, 0x33, 0x33));
    nameLab->setTextWidth(sz.w - 16);
    nameLab->setPos(nx + 8, ny + kHeadH);
    nameLab->setParentItem(box);
    nameLab->setCursor(Qt::OpenHandCursor);

    QFont portFont;
    portFont.setPixelSize(10);
    const int rows = (ins.size() > outs.size()) ? ins.size() : outs.size();
    const qreal portTop = ny + kHeadH + kNameH + kPortPad;
    for (int r = 0; r < rows; ++r) {
        const qreal cy = portTop + r * kPortH + kPortH / 2.0;
        if (r < ins.size()) {
            addPin(box, nx, cy, ins[r].data, id, ins[r].key, 0, pins);
            QGraphicsSimpleTextItem* lab = new QGraphicsSimpleTextItem(ins[r].label, box);
            lab->setFont(portFont);
            lab->setBrush(QBrush(QColor(0x59, 0x59, 0x59)));
            lab->setPos(nx + 10, cy - 7);
            lab->setCursor(Qt::OpenHandCursor);
        }
        if (r < outs.size()) {
            addPin(box, nx + sz.w, cy, outs[r].data, id, outs[r].key, 1, pins);
            QGraphicsSimpleTextItem* lab = new QGraphicsSimpleTextItem(outs[r].label, box);
            lab->setFont(portFont);
            lab->setBrush(QBrush(QColor(0x59, 0x59, 0x59)));
            const QRectF br = lab->boundingRect();
            lab->setPos(nx + sz.w - 10 - br.width(), cy - 7);
            lab->setCursor(Qt::OpenHandCursor);
        }
    }
    return box;
}

// 画一个组及其子节点
static void paintGroup(QGraphicsScene* scene, const JobCfg* job, const ToolGroup& g,
                       qreal x, qreal y, const QString& selectedId, PinLayout* pins,
                       QGraphicsItem* parent, QHash<QString, QGraphicsItem*>* nodes,
                       QVector<CanvasOrderRef>* orderLines)
{
    if (canvasLeaf(g)) {
        const ToolCfg* t = job->findTool(g.toolId);
        const int index = job->indexOf(g.toolId) + 1;
        QGraphicsRectItem* box = addToolConfig(scene, x, y, t, index, qs(g.toolId),
                                             selectedId == qs(g.toolId), pins, parent);
        if (nodes && box) {
            nodes->insert(canvasNodeKey(g), box);
        }
        return;
    }
    const GroupSize sz = measureGroup(*job, g);
    const bool split = canvasSplit(g);
        const bool isIf = (g.kind == GroupIf);
        const bool sel = (selectedId == qs(g.id));
    qreal gx = x;
    qreal gy = y;
    if (split && g.canvasPlaced) {
        if (parent) {
            const QPointF local = parent->mapFromScene(QPointF(g.canvasX, g.canvasY));
            gx = local.x();
            gy = local.y();
        } else {
            gx = g.canvasX;
            gy = g.canvasY;
        }
    }
    QPen pen;
    QBrush brush;
    if (split) {
        pen = QPen(sel ? QColor(0x18, 0x90, 0xFF)
                     : (isIf ? QColor(0xFF, 0xC0, 0x69) : QColor(0x91, 0xD5, 0xFF)));
        pen.setStyle(Qt::DashLine);
        brush = QBrush(isIf ? QColor(0xFF, 0xF7, 0xE6) : QColor(0xE6, 0xF7, 0xFF));
    } else {
        pen = QPen(sel ? QColor(0x18, 0x90, 0xFF) : QColor(0xD9, 0xD9, 0xD9));
        pen.setStyle(Qt::DashLine);
        brush = QBrush(QColor(0xFA, 0xFA, 0xFA));
    }
    QGraphicsRectItem* frame = nullptr;
    if (parent) {
        frame = new QGraphicsRectItem(gx, gy, sz.w, sz.h, parent);
        frame->setPen(pen);
        frame->setBrush(brush);
    } else {
        frame = scene->addRect(gx, gy, sz.w, sz.h, pen, brush);
    }
    frame->setData(RoleId, qs(g.id));
    frame->setData(RoleKind, KindGroup);
    frame->setCursor(Qt::OpenHandCursor);
    frame->setZValue(kZGroupFrame);
    if (nodes) {
        nodes->insert(canvasNodeKey(g), frame);
    }
        QString titleText;
    QColor titleColor(0x88, 0x88, 0x88);
    if (split) {
        if (isIf) {
            titleText = T("条件 ");
            if (g.condSourceId.isNull()) {
                titleText += T("未指定");
            } else {
                const ToolCfg* src = job->findTool(g.condSourceId);
                titleText += src ? qs(src->name) : T("未命名");
            }
            titleText += (g.condWant == CondThenOnNg) ? T("=NG") : T("=OK");
            titleColor = QColor(0xD4, 0x6B, 0x08);
        } else {
            titleText = T("并行 ") + qs(g.name);
            titleColor = QColor(0x18, 0x90, 0xFF);
        }
    } else {
        if (g.kind == GroupLoop) {
            titleText = T("循环 ") + qs(g.name);
            const int n = g.loopCount < 1 ? 1 : g.loopCount;
            titleText += QString::fromLatin1(" x%1").arg(n);
            titleColor = QColor(0x52, 0xC4, 0x1A);
        } else {
            titleText = qs(g.name);
        }
        }
        QGraphicsTextItem* title = scene->addText(titleText);
    title->setDefaultTextColor(titleColor);
    title->setPos(gx + 8, gy + 2);
        title->setParentItem(frame);
    if (split) {
        qreal cx = gx + kPad;
        const qreal cy = gy + kTitleH + kPad;
        for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
            const GroupSize c = measureGroup(*job, g.children[i]);
            paintGroup(scene, job, g.children[i], cx, cy, selectedId, pins, frame, nodes, orderLines);
            cx += c.w + kHGap;
        }
    } else {
        qreal cy = gy + kTitleH;
        for (int i = 0; i < static_cast<int>(g.children.size()); ++i) {
            const GroupSize c = measureGroup(*job, g.children[i]);
            paintGroup(scene, job, g.children[i], gx + (sz.w - c.w) / 2.0, cy, selectedId, pins,
                       frame, nodes, orderLines);
            cy += c.h + kVGap;
        }
        if (orderLines && nodes) {
            linkSequentialNodes(scene, *nodes, g.children, orderLines);
        }
    }
    fitFrameToChildren(frame);
}

// 画数据连线
static void paintWires(QGraphicsScene* scene, const JobCfg& job, const PinLayout& pins,
                       int selectedLinkIndex, QVector<CanvasWireRef>* outWires,
                       const QHash<QString, QGraphicsItem*>& toolBoxes,
                       const QHash<QString, QGraphicsItem*>& groupFrames)
{
    for (int i = 0; i < static_cast<int>(job.links.size()); ++i) {
        const ToolLink& L = job.links[static_cast<size_t>(i)];
        const QString fromKey = pinMapKey(qs(L.fromId), qs(portKey(L.fromPort)), 1);
        const QString toKey = pinMapKey(qs(L.toId), qs(portKey(L.toPort)), 0);
        if (!pins.item.contains(fromKey) || !pins.item.contains(toKey)) {
            continue;
        }
        QGraphicsItem* fromPin = pins.item.value(fromKey);
        QGraphicsItem* toPin = pins.item.value(toKey);
        if (!fromPin || !toPin) {
            continue;
        }
        const QPointF a = pinSceneCenter(fromPin);
        const QPointF b = pinSceneCenter(toPin);
        QPainterPath path;
        if (L.routeCustom && !L.routePoints.empty()) {
            path = makeOrthoPath(rawFullFromMids(a, b, routeMidsToQ(L.routePoints)));
        } else {
            const QString fromId = qs(L.fromId);
            const QString toId = qs(L.toId);
            QRectF srcBox;
            QRectF dstBox;
            if (toolBoxes.contains(fromId) && toolBoxes.value(fromId)) {
                srcBox = toolBoxes.value(fromId)->sceneBoundingRect();
            }
            if (toolBoxes.contains(toId) && toolBoxes.value(toId)) {
                dstBox = toolBoxes.value(toId)->sceneBoundingRect();
            }
            const QVector<QRectF> obs = collectWireObstacles(toolBoxes, groupFrames, fromId, toId);
            path = makeOrthoPath(routeOrthoAvoidObstacles(a, b, obs, srcBox, dstBox));
        }
        QGraphicsPathItem* item = scene->addPath(path);
        const bool sel = (i == selectedLinkIndex);
        QPen pen(wireColor(L.fromPort.data), sel ? 4.8 : 2.2);
        if (sel) {
            pen.setColor(QColor(0x18, 0x90, 0xFF));
        }
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        item->setPen(pen);
        item->setBrush(Qt::NoBrush);
        item->setZValue(sel ? kZWire + 0.2 : kZWire);
        item->setData(RoleId, QString::number(i));
        item->setData(RoleKind, KindWire);
        item->setCursor(Qt::PointingHandCursor);
        if (outWires) {
            CanvasWireRef wr;
            wr.item = item;
            wr.fromKey = fromKey;
            wr.toKey = toKey;
            wr.linkIndex = i;
            outWires->push_back(wr);
        }
    }
}

// 流程画布
JobCanvas::JobCanvas(QWidget* parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_zoom(1.0)
    , m_panning(false)
    , m_vScrollLeft(new QScrollBar(Qt::Vertical, this))
    , m_blockScrollSync(false)
    , m_wiring(false)
    , m_wireFromData(PortData::Image)
    , m_tempWire(nullptr)
    , m_movingNode(false)
    , m_movingGroup(false)
    , m_moveArmed(false)
    , m_moveItem(nullptr)
    , m_job(nullptr)
    , m_selectedLinkIndex(-1)
    , m_wirePickIndex(0)
    , m_draggingWireHandle(false)
    , m_dragLinkIndex(-1)
    , m_dragVertexIndex(-1)
    , m_dragSegIndex(-1)
    , m_dragIsSegment(false)
    , m_dragPressScene(0, 0)
    , m_previewRoute(false)
    , m_previewLinkIndex(-1)
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing, true);
    setBackgroundBrush(QColor(0xFA, 0xFB, 0xFC));
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setMinimumWidth(160);
    setMinimumHeight(80);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_vScrollLeft->setVisible(false);
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(onVertScroll(int)));
    connect(m_vScrollLeft, SIGNAL(valueChanged(int)), this, SLOT(onLeftScroll(int)));
    connect(verticalScrollBar(), SIGNAL(rangeChanged(int,int)), m_vScrollLeft, SLOT(setRange(int,int)));
    setFocusPolicy(Qt::ClickFocus);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::NoDrag);
}

// 缩放置回 100%
void JobCanvas::resetZoom()
{
    if (qAbs(m_zoom - 1.0) < 0.001) {
        return;
    }
    resetTransform();
    m_zoom = 1.0;
}

// 按流程重画画布
void JobCanvas::showJob(const JobCfg* job, const QString& selectedId, int selectedLinkIndex)
{
    m_tempWire = 0;
    m_wiring = false;
    m_movingNode = false;
    m_movingGroup = false;
    m_moveArmed = false;
    m_moveItem = 0;
    m_draggingWireHandle = false;
    m_dragLinkIndex = -1;
    m_dragVertexIndex = -1;
    m_dragSegIndex = -1;
    m_dragIsSegment = false;
    m_wireVertexHandles.clear();
    m_wireSegHandles.clear();
    m_previewRoute = false;
    m_previewLinkIndex = -1;
    m_previewMids.clear();
    m_nodeWireSnaps.clear();
    m_job = job;
    m_selectedLinkIndex = selectedLinkIndex;
    m_pinItems.clear();
    m_wires.clear();
    m_orderLines.clear();
    m_toolBoxes.clear();
    m_groupFrames.clear();
    m_selectedGroupId.clear();
    m_scene->clear();
    if (!job) {
        QGraphicsTextItem* empty = m_scene->addText(T("选中流程后\n在此显示节点"));
        empty->setDefaultTextColor(QColor(0x88, 0x88, 0x88));
        empty->setPos(24, 24);
        m_scene->setSceneRect(0, 0, 240, 200);
        return;
    }

    PinLayout pins;
    QHash<QString, QGraphicsItem*> nodes;
    QString jobTitle = qs(job->name);
    if (!job->enabled) {
        jobTitle += T("（未启用）");
    }
    QGraphicsTextItem* title = m_scene->addText(jobTitle);
    title->setDefaultTextColor(job->enabled ? QColor(0x88, 0x88, 0x88) : QColor(0xA0, 0xA0, 0xA0));
    title->setPos(12, 4);

    const ToolGroup& root = job->root;
    qreal y = 32;
    qreal maxW = 240;
    qreal x = 16;
    if (canvasLeaf(root) || (root.children.empty() && !canvasSplit(root))) {
        if (!job->tools.empty() && root.children.empty()) {
            QGraphicsItem* prevNode = nullptr;
            for (int i = 0; i < static_cast<int>(job->tools.size()); ++i) {
                const ToolCfg& rec = job->tools[static_cast<size_t>(i)];
                const GroupSize sz = measureToolBox(&rec);
                QGraphicsRectItem* box = addToolConfig(m_scene, x, y, &rec, i + 1, qs(rec.id),
                            selectedId == qs(rec.id), &pins, 0);
                if (box) {
                    nodes.insert(qs(rec.id), box);
                    if (prevNode) {
                        addExecOrderLine(m_scene, prevNode, box, &m_orderLines);
                    }
                    prevNode = box;
                }
                y += sz.h + kVGap;
                if (sz.w + 32 > maxW) {
                    maxW = sz.w + 32;
                }
            }
        }
    } else if (canvasSplit(root)) {
        const GroupSize sz = measureGroup(*job, root);
        paintGroup(m_scene, job, root, x, y, selectedId, &pins, 0, &nodes, &m_orderLines);
        maxW = sz.w + 32;
        y += sz.h;
    } else {
        qreal colW = kBoxW;
        for (int i = 0; i < static_cast<int>(root.children.size()); ++i) {
            const GroupSize c = measureGroup(*job, root.children[i]);
            if (c.w > colW) {
                colW = c.w;
            }
        }
        maxW = colW + 32;
        for (int i = 0; i < static_cast<int>(root.children.size()); ++i) {
            const GroupSize c = measureGroup(*job, root.children[i]);
            const qreal cx = x + (colW - c.w) / 2.0;
            paintGroup(m_scene, job, root.children[i], cx, y, selectedId, &pins, 0, &nodes, &m_orderLines);
            y += c.h + kVGap;
        }
        linkSequentialNodes(m_scene, nodes, root.children, &m_orderLines);
    }
    QHash<QString, QGraphicsItem*>::const_iterator git = nodes.constBegin();
    for (; git != nodes.constEnd(); ++git) {
        QGraphicsRectItem* frame = qgraphicsitem_cast<QGraphicsRectItem*>(git.value());
        if (!frame) {
            continue;
        }
        const int rk = frame->data(RoleKind).toInt();
        if (rk == KindTool) {
            m_toolBoxes.insert(git.key(), frame);
            continue;
        }
        if (rk != KindGroup) {
            continue;
        }
        ToolGroup* tg = const_cast<JobCfg*>(job)->findGroup(oid(git.key()));
        if (!tg || !canvasSplit(*tg)) {
            continue;
        }
        m_groupFrames.insert(git.key(), frame);
    }
    if (!selectedId.isEmpty()) {
        ToolGroup* sg = const_cast<JobCfg*>(job)->findGroup(oid(selectedId));
        if (sg && canvasSplit(*sg)) {
            m_selectedGroupId = selectedId;
        }
    }
    paintWires(m_scene, *job, pins, m_selectedLinkIndex, &m_wires, m_toolBoxes, m_groupFrames);
    m_pinItems = pins.item;
    updateWireHandles();
    QGraphicsTextItem* hint = m_scene->addText(
        T("滚轮缩放，拖动画布平移。拖拐点或线段可改走线，双击线段加点，双击拐点删除。"));
    hint->setDefaultTextColor(QColor(0xBF, 0xBF, 0xBF));
    hint->setPos(12, y + 4);
    const QRectF br = m_scene->itemsBoundingRect().adjusted(-120, -48, 120, 48);
    m_scene->setSceneRect(br.isEmpty() ? QRectF(0, 0, qMax(maxW, 280.0), qMax(y + 36.0, 220.0)) : br);
    layoutLeftScrollBar();
    onVertScroll(verticalScrollBar()->value());
}

// 命中测试：空白、工具、组或连线
bool JobCanvas::hitAt(const QPoint& viewPos, QString* id, int* kind, QString* port, int* dir) const
{
    if (id) {
        id->clear();
    }
    if (kind) {
        *kind = CanvasHitBlank;
    }
    if (port) {
        port->clear();
    }
    if (dir) {
        *dir = 0;
    }
    QGraphicsItem* it = itemAt(viewPos);
    while (it) {
        const int k = it->data(RoleKind).toInt();
        if (k == KindPin) {
            if (id) {
                *id = it->data(RoleId).toString();
            }
            if (kind) {
                *kind = CanvasHitTool;
            }
            if (port) {
                *port = it->data(RolePort).toString();
            }
            if (dir) {
                *dir = it->data(RoleDir).toInt();
            }
            return true;
        }
        if (k == KindWire) {
            if (id) {
                *id = it->data(RoleId).toString();
            }
            if (kind) {
                *kind = CanvasHitWire;
            }
            return true;
        }
        it = it->parentItem();
    }
    it = itemAt(viewPos);
    while (it && it->data(RoleId).toString().isEmpty()) {
        it = it->parentItem();
    }
    if (!it) {
        return false;
    }
    if (id) {
        *id = it->data(RoleId).toString();
    }
    if (it->data(RoleKind).toInt() == KindGroup) {
        if (kind) {
            *kind = CanvasHitGroup;
        }
    } else {
        if (kind) {
            *kind = CanvasHitTool;
        }
    }
    return true;
}

// 命中针脚
bool JobCanvas::hitPinAt(const QPoint& viewPos, QString* id, QString* port, int* dir) const
{
    if (id) {
        id->clear();
    }
    if (port) {
        port->clear();
    }
    if (dir) {
        *dir = 0;
    }
    const QPointF sp = mapToScene(viewPos);
    const qreal maxDist = kPinR + 8.0;
    QGraphicsItem* best = nullptr;
    qreal bestDist = maxDist;
    QHash<QString, QGraphicsItem*>::const_iterator it = m_pinItems.constBegin();
    for (; it != m_pinItems.constEnd(); ++it) {
        QGraphicsItem* pin = it.value();
        if (!pin || !pin->isVisible()) {
            continue;
        }
        const qreal d = QLineF(pinSceneCenter(pin), sp).length();
        if (d <= bestDist) {
            bestDist = d;
            best = pin;
        }
    }
    if (!best) {
        return false;
    }
    if (id) {
        *id = best->data(RoleId).toString();
    }
    if (port) {
        *port = best->data(RolePort).toString();
    }
    if (dir) {
        *dir = best->data(RoleDir).toInt();
    }
    return true;
}

// 命中工具框
QGraphicsItem* JobCanvas::toolBoxAt(const QPoint& viewPos) const
{
    const QPointF sp = mapToScene(viewPos);
    QGraphicsItem* best = nullptr;
    qreal bestArea = 0;
    QHash<QString, QGraphicsItem*>::const_iterator it = m_toolBoxes.constBegin();
    for (; it != m_toolBoxes.constEnd(); ++it) {
        QGraphicsRectItem* box = qgraphicsitem_cast<QGraphicsRectItem*>(it.value());
        if (!box || !box->isVisible()) {
            continue;
        }
        const QRectF br = box->sceneBoundingRect();
        if (!br.contains(sp)) {
            continue;
        }
        const qreal area = br.width() * br.height();
        if (!best || area < bestArea) {
            best = box;
            bestArea = area;
        }
    }
    return best;
}

// 场景点是否落在组内工具上
bool JobCanvas::scenePointOnGroupTool(QGraphicsRectItem* frame, const QPointF& sp) const
{
    if (!frame) {
        return false;
    }
    const QList<QGraphicsItem*> kids = frame->childItems();
    for (int i = 0; i < kids.size(); ++i) {
        QGraphicsItem* c = kids[i];
        if (!c) {
            continue;
        }
        if (c->data(RoleKind).toInt() == KindTool) {
            if (c->sceneBoundingRect().contains(sp)) {
                return true;
            }
            continue;
        }
        QGraphicsRectItem* sub = qgraphicsitem_cast<QGraphicsRectItem*>(c);
        if (sub && sub->data(RoleKind).toInt() == KindGroup) {
            if (scenePointOnGroupTool(sub, sp)) {
                return true;
            }
        }
    }
    return false;
}

// 命中组框
QGraphicsItem* JobCanvas::groupFrameAt(const QPoint& viewPos) const
{
    if (toolBoxAt(viewPos)) {
        return 0;
    }
    const QPointF sp = mapToScene(viewPos);
    if (!m_selectedGroupId.isEmpty()) {
        QGraphicsRectItem* sel = qgraphicsitem_cast<QGraphicsRectItem*>(
            m_groupFrames.value(m_selectedGroupId));
        if (sel && sel->sceneBoundingRect().contains(sp) && !scenePointOnGroupTool(sel, sp)) {
            return sel;
        }
    }
    QGraphicsRectItem* best = nullptr;
    qreal bestArea = 0;
    QHash<QString, QGraphicsItem*>::const_iterator it = m_groupFrames.constBegin();
    for (; it != m_groupFrames.constEnd(); ++it) {
        QGraphicsRectItem* frame = qgraphicsitem_cast<QGraphicsRectItem*>(it.value());
        if (!frame || !frame->sceneBoundingRect().contains(sp)) {
            continue;
        }
        if (scenePointOnGroupTool(frame, sp)) {
            continue;
        }
        const QRectF br = frame->sceneBoundingRect();
        const qreal area = br.width() * br.height();
        if (!best || area < bestArea) {
            best = frame;
            bestArea = area;
        }
    }
    return best;
}

// 沿父组把框收紧
void JobCanvas::refitGroupChain(QGraphicsItem* item)
{
    QGraphicsRectItem* g = parentGroupFrame(item);
    while (g) {
        fitFrameToChildren(g);
        g = parentGroupFrame(g);
    }
}

// 取连线当前中间拐点（预览沿用存盘；否则从自动路径提取）
QVector<QPointF> JobCanvas::midPointsForLink(int linkIndex, const QPointF& a, const QPointF& b) const
{
    if (m_previewRoute && linkIndex == m_previewLinkIndex) {
        return m_previewMids;
    }
    if (m_job && linkIndex >= 0 && linkIndex < static_cast<int>(m_job->links.size())) {
        const ToolLink& L = m_job->links[static_cast<size_t>(linkIndex)];
        if (L.routeCustom && !L.routePoints.empty()) {
            return routeMidsToQ(L.routePoints);
        }
    }
    QString fromId;
    QString toId;
    if (m_job && linkIndex >= 0 && linkIndex < static_cast<int>(m_job->links.size())) {
        fromId = qs(m_job->links[static_cast<size_t>(linkIndex)].fromId);
        toId = qs(m_job->links[static_cast<size_t>(linkIndex)].toId);
    }
    QRectF srcBox;
    QRectF dstBox;
    if (m_toolBoxes.contains(fromId) && m_toolBoxes.value(fromId)) {
        srcBox = m_toolBoxes.value(fromId)->sceneBoundingRect();
    }
    if (m_toolBoxes.contains(toId) && m_toolBoxes.value(toId)) {
        dstBox = m_toolBoxes.value(toId)->sceneBoundingRect();
    }
    const QVector<QRectF> obs = collectWireObstacles(m_toolBoxes, m_groupFrames, fromId, toId);
    return midsFromFullRoute(routeOrthoAvoidObstacles(a, b, obs, srcBox, dstBox));
}
// 把斜着接到端口的那一段拉回横平，不改它的横向位置
static void repairPinAttachments(QVector<QPointF>* mids, const QPointF& a, const QPointF& b)
{
    if (!mids || mids->isEmpty()) {
        return;
    }
    auto diagonal = [](const QPointF& p, const QPointF& pin) {
        return qAbs(p.x() - pin.x()) > 0.5 && qAbs(p.y() - pin.y()) > 0.5;
    };
    auto runLen = [](const QVector<QPointF>& pts, int start, int dir, qreal keyY) {
        int n = 0;
        for (int i = start; i >= 0 && i < pts.size(); i += dir) {
            if (n > 0 && qAbs(pts[i].y() - keyY) > 0.5) {
                break;
            }
            ++n;
        }
        return n;
    };
    if (diagonal((*mids)[0], a)) {
        const int n = mids->size();
        const qreal oldY = (*mids)[0].y();
        const int lead = runLen(*mids, 0, 1, oldY);
        const int trail = runLen(*mids, n - 1, -1, (*mids)[n - 1].y());
        const int use = (lead + trail > n) ? 1 : lead;
        for (int i = 0; i < use; ++i) {
            (*mids)[i].setY(a.y());
        }
        if (use == 1 && lead + trail > n) {
            mids->insert(1, QPointF((*mids)[0].x(), oldY));
        }
    }
    if (!mids->isEmpty() && diagonal(mids->last(), b)) {
        const int n = mids->size();
        const qreal oldY = mids->last().y();
        const int trail = runLen(*mids, n - 1, -1, oldY);
        const int lead = runLen(*mids, 0, 1, (*mids)[0].y());
        const int use = (lead + trail > n) ? 1 : trail;
        for (int i = 0; i < use; ++i) {
            (*mids)[n - 1 - i].setY(b.y());
        }
        if (use == 1 && lead + trail > n) {
            mids->insert(n - 1, QPointF((*mids)[n - 1].x(), oldY));
        }
    }
}

// 沿短桩方向平移这一段，拐点按短桩长度跟着平移。整段共线时只动最近的点
static void followPinRun(QVector<QPointF>* mids, const QVector<QPointF>& base,
                         bool fromStart, bool horiz, qreal crossDelta, qreal alongDelta)
{
    if (!mids || base.isEmpty() || mids->size() != base.size()) {
        return;
    }
    if (qAbs(crossDelta) <= 0.5 && qAbs(alongDelta) <= 0.5) {
        return;
    }
    const int n = base.size();
    int count = 0;
    const int stepStart = fromStart ? 0 : (n - 1);
    const int dir = fromStart ? 1 : -1;
    const qreal key = horiz ? base[stepStart].y() : base[stepStart].x();
    for (int i = stepStart; i >= 0 && i < n; i += dir) {
        const qreal v = horiz ? base[i].y() : base[i].x();
        if (count > 0 && qAbs(v - key) > 0.5) {
            break;
        }
        ++count;
    }
    int other = 0;
    const int otherStart = fromStart ? (n - 1) : 0;
    const int otherDir = fromStart ? -1 : 1;
    const qreal otherKey = horiz ? base[otherStart].y() : base[otherStart].x();
    for (int i = otherStart; i >= 0 && i < n; i += otherDir) {
        const qreal v = horiz ? base[i].y() : base[i].x();
        if (other > 0 && qAbs(v - otherKey) > 0.5) {
            break;
        }
        ++other;
    }
    const bool overlap = count + other > n;
    const int use = overlap ? 1 : count;
    const int begin = fromStart ? 0 : (n - use);
    if (qAbs(crossDelta) > 0.5) {
        for (int k = 0; k < use; ++k) {
            const int i = begin + k;
            if (horiz) {
                (*mids)[i].setY(base[i].y() + crossDelta);
            } else {
                (*mids)[i].setX(base[i].x() + crossDelta);
            }
        }
    }
    if (overlap && count > 1 && qAbs(crossDelta) > 0.5) {
        const QPointF corner = base[fromStart ? 0 : (n - 1)];
        const int at = fromStart ? 1 : (mids->size() - 1);
        mids->insert(at, corner);
        return;
    }
    if (overlap || qAbs(alongDelta) <= 0.5) {
        return;
    }
    const int far = fromStart ? (use - 1) : begin;
    const int stop = fromStart ? (n - other) : other;
    const qreal alongKey = horiz ? base[far].x() : base[far].y();
    if (horiz) {
        (*mids)[far].setX(base[far].x() + alongDelta);
    } else {
        (*mids)[far].setY(base[far].y() + alongDelta);
    }
    for (int i = far + dir; i >= 0 && i < n; i += dir) {
        if (fromStart ? (i >= stop) : (i < stop)) {
            break;
        }
        const qreal v = horiz ? base[i].x() : base[i].y();
        if (qAbs(v - alongKey) > 0.5) {
            break;
        }
        if (horiz) {
            (*mids)[i].setX(base[i].x() + alongDelta);
        } else {
            (*mids)[i].setY(base[i].y() + alongDelta);
        }
    }
}

// 按下时记下当前显示的拐点，再开预览，避免读到空的自动走线
void JobCanvas::beginCustomPreviewFromAuto(int linkIndex, const QPointF& a, const QPointF& b)
{
    QVector<QPointF> mids = midPointsForLink(linkIndex, a, b);
    if (mids.isEmpty()) {
        mids = defaultWireMids(a, b);
    }
    repairPinAttachments(&mids, a, b);
    m_previewRoute = true;
    m_previewLinkIndex = linkIndex;
    m_previewMids = mids;
}
// 拖节点开始前记下自定义折线，并把已经斜掉的端口接回横平
void JobCanvas::captureNodeWireSnaps()
{
    m_nodeWireSnaps.clear();
    if (!m_job || m_movingGroup) {
        return;
    }
    JobCfg* job = const_cast<JobCfg*>(m_job);
    for (int i = 0; i < m_wires.size(); ++i) {
        const CanvasWireRef& w = m_wires[i];
        if (w.linkIndex < 0 || w.linkIndex >= (int)job->links.size()) {
            continue;
        }
        ToolLink& L = job->links[w.linkIndex];
        if (!L.routeCustom || L.routePoints.empty()) {
            continue;
        }
        QGraphicsItem* fp = m_pinItems.value(w.fromKey, 0);
        QGraphicsItem* tp = m_pinItems.value(w.toKey, 0);
        if (!fp || !tp) {
            continue;
        }
        NodeWireSnap s;
        s.linkIndex = w.linkIndex;
        s.mids = routeMidsToQ(L.routePoints);
        s.pinA = pinSceneCenter(fp);
        s.pinB = pinSceneCenter(tp);
        repairPinAttachments(&s.mids, s.pinA, s.pinB);
        s.srcHoriz = s.mids.isEmpty()
            || qAbs(s.mids[0].y() - s.pinA.y()) <= qAbs(s.mids[0].x() - s.pinA.x());
        s.dstHoriz = s.mids.isEmpty()
            || qAbs(s.mids.last().y() - s.pinB.y()) <= qAbs(s.mids.last().x() - s.pinB.x());
        L.routePoints.clear();
        for (int k = 0; k < s.mids.size(); ++k) {
            L.routePoints.push_back(RoutePoint(s.mids[k].x(), s.mids[k].y()));
        }
        m_nodeWireSnaps.push_back(s);
    }
}

// 节点移动后，只让端口旁边那一段跟着走，其余拐点保持按下时的位置
void JobCanvas::applyNodeWireSnaps()
{
    if (!m_job || m_movingGroup || m_nodeWireSnaps.isEmpty()) {
        return;
    }
    JobCfg* job = const_cast<JobCfg*>(m_job);
    for (int s = 0; s < m_nodeWireSnaps.size(); ++s) {
        const NodeWireSnap& snap = m_nodeWireSnaps[s];
        if (snap.linkIndex < 0 || snap.linkIndex >= (int)job->links.size() || snap.mids.isEmpty()) {
            continue;
        }
        QString fromKey;
        QString toKey;
        for (int i = 0; i < m_wires.size(); ++i) {
            if (m_wires[i].linkIndex == snap.linkIndex) {
                fromKey = m_wires[i].fromKey;
                toKey = m_wires[i].toKey;
                break;
            }
        }
        QGraphicsItem* fp = m_pinItems.value(fromKey, 0);
        QGraphicsItem* tp = m_pinItems.value(toKey, 0);
        if (!fp || !tp) {
            continue;
        }
        const QPointF curA = pinSceneCenter(fp);
        const QPointF curB = pinSceneCenter(tp);
        const QPointF dA = curA - snap.pinA;
        const QPointF dB = curB - snap.pinB;
        QVector<QPointF> mids = snap.mids;
        const bool both = qAbs(dA.x() - dB.x()) <= 0.5 && qAbs(dA.y() - dB.y()) <= 0.5
            && (qAbs(dA.x()) > 0.5 || qAbs(dA.y()) > 0.5);
        if (both) {
            for (int k = 0; k < mids.size(); ++k) {
                mids[k] += dA;
            }
        } else {
            if (qAbs(dA.x()) > 0.5 || qAbs(dA.y()) > 0.5) {
                followPinRun(&mids, snap.mids, true, snap.srcHoriz,
                             snap.srcHoriz ? dA.y() : dA.x(), snap.srcHoriz ? dA.x() : dA.y());
            }
            if (qAbs(dB.x()) > 0.5 || qAbs(dB.y()) > 0.5) {
                followPinRun(&mids, snap.mids, false, snap.dstHoriz,
                             snap.dstHoriz ? dB.y() : dB.x(), snap.dstHoriz ? dB.x() : dB.y());
            }
        }
        ToolLink& L = job->links[snap.linkIndex];
        L.routePoints.clear();
        L.routeCustom = true;
        for (int k = 0; k < mids.size(); ++k) {
            L.routePoints.push_back(RoutePoint(mids[k].x(), mids[k].y()));
        }
    }
}

// 计算一条连线路径；可选返回中间拐点
QPainterPath JobCanvas::wirePathForLink(int linkIndex, const QPointF& a, const QPointF& b,
                                        QVector<QPointF>* outMids) const
{
    const QVector<QPointF> mids = midPointsForLink(linkIndex, a, b);
    if (outMids) {
        *outMids = mids;
    }
    const bool preview = (m_previewRoute && linkIndex == m_previewLinkIndex);
    const bool custom = (!preview && m_job && linkIndex >= 0
        && linkIndex < static_cast<int>(m_job->links.size())
        && m_job->links[static_cast<size_t>(linkIndex)].routeCustom
        && !m_job->links[static_cast<size_t>(linkIndex)].routePoints.empty());
    if (preview) {
        return makeOrthoPath(rawFullFromMids(a, b, mids));
    }
    if (custom) {
        return makeOrthoPath(rawFullFromMids(a, b, mids));
    }
    QString fromId;
    QString toId;
    if (m_job && linkIndex >= 0 && linkIndex < static_cast<int>(m_job->links.size())) {
        fromId = qs(m_job->links[static_cast<size_t>(linkIndex)].fromId);
        toId = qs(m_job->links[static_cast<size_t>(linkIndex)].toId);
    }
    QRectF srcBox;
    QRectF dstBox;
    if (m_toolBoxes.contains(fromId) && m_toolBoxes.value(fromId)) {
        srcBox = m_toolBoxes.value(fromId)->sceneBoundingRect();
    }
    if (m_toolBoxes.contains(toId) && m_toolBoxes.value(toId)) {
        dstBox = m_toolBoxes.value(toId)->sceneBoundingRect();
    }
    const QVector<QRectF> obs = collectWireObstacles(m_toolBoxes, m_groupFrames, fromId, toId);
    return makeOrthoPath(routeOrthoAvoidObstacles(a, b, obs, srcBox, dstBox));
}
// 收集鼠标下的连线
void JobCanvas::collectWireHitsAt(const QPoint& viewPos, QVector<int>* outIndices) const
{
    if (!outIndices) {
        return;
    }
    outIndices->clear();
    const QPointF sp = mapToScene(viewPos);
    const qreal tol = qMax(8.0, 10.0 / qMax(m_zoom, 0.25));
    QPainterPathStroker stroker;
    stroker.setWidth(tol * 2.0);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    struct HitDist {
        int index;
        qreal dist;
    };
    QVector<HitDist> hits;
    for (int i = 0; i < m_wires.size(); ++i) {
        const CanvasWireRef& w = m_wires[i];
        if (!w.item) {
            continue;
        }
        const QPainterPath stroke = stroker.createStroke(w.item->path());
        if (!stroke.contains(sp)) {
            continue;
        }
        const QPainterPath path = w.item->path();
        qreal best = 1e9;
        const int steps = 24;
        for (int s = 0; s <= steps; ++s) {
            const qreal t = static_cast<qreal>(s) / steps;
            const QPointF p = path.pointAtPercent(t);
            const qreal d = QLineF(p, sp).length();
            if (d < best) {
                best = d;
            }
        }
        HitDist hd;
        hd.index = w.linkIndex;
        hd.dist = best;
        hits.push_back(hd);
    }
    for (int i = 0; i < hits.size(); ++i) {
        for (int j = i + 1; j < hits.size(); ++j) {
            if (hits[j].dist < hits[i].dist) {
                const HitDist tmp = hits[i];
                hits[i] = hits[j];
                hits[j] = tmp;
            }
        }
    }
    for (int i = 0; i < hits.size(); ++i) {
        outIndices->push_back(hits[i].index);
    }
}

// 刷新选中连线的外观
void JobCanvas::refreshWireSelectionVisual()
{
    for (int i = 0; i < m_wires.size(); ++i) {
        const CanvasWireRef& w = m_wires[i];
        if (!w.item || !m_job || w.linkIndex < 0
            || w.linkIndex >= static_cast<int>(m_job->links.size())) {
            continue;
        }
        const bool sel = (w.linkIndex == m_selectedLinkIndex);
        const ToolLink& L = m_job->links[static_cast<size_t>(w.linkIndex)];
        QPen pen(sel ? QColor(0x18, 0x90, 0xFF) : wireColor(L.fromPort.data), sel ? 4.8 : 2.2);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        w.item->setPen(pen);
        w.item->setZValue(sel ? kZWire + 0.2 : kZWire);
    }
    updateWireHandles();
}

// 更新选中连线的拐点手柄与段手柄
void JobCanvas::updateWireHandles()
{
    // 拖动中不重建手柄，避免删掉正在抓的图元后连线乱跑
    if (m_draggingWireHandle) {
        return;
    }
    for (int i = 0; i < m_wireVertexHandles.size(); ++i) {
        if (m_wireVertexHandles[i]) {
            m_scene->removeItem(m_wireVertexHandles[i]);
            delete m_wireVertexHandles[i];
        }
    }
    m_wireVertexHandles.clear();
    for (int i = 0; i < m_wireSegHandles.size(); ++i) {
        if (m_wireSegHandles[i]) {
            m_scene->removeItem(m_wireSegHandles[i]);
            delete m_wireSegHandles[i];
        }
    }
    m_wireSegHandles.clear();
    if (!m_job || m_selectedLinkIndex < 0) {
        return;
    }
    const CanvasWireRef* selWire = nullptr;
    for (int i = 0; i < m_wires.size(); ++i) {
        if (m_wires[i].linkIndex == m_selectedLinkIndex) {
            selWire = &m_wires[i];
            break;
        }
    }
    if (!selWire || !selWire->item) {
        return;
    }
    const CanvasWireRef& w = *selWire;
    QGraphicsItem* fromPin = m_pinItems.value(w.fromKey);
    QGraphicsItem* toPin = m_pinItems.value(w.toKey);
    if (!fromPin || !toPin) {
        return;
    }
    const QPointF a = pinSceneCenter(fromPin);
    const QPointF b = pinSceneCenter(toPin);
    QVector<QPointF> mids;
    wirePathForLink(m_selectedLinkIndex, a, b, &mids);
    const QVector<QPointF> full = rawFullFromMids(a, b, mids);
    const qreal hr = 5.5;
    for (int i = 1; i + 1 < full.size(); ++i) {
        const QPointF p = full[i];
        QGraphicsEllipseItem* h = m_scene->addEllipse(p.x() - hr, p.y() - hr, hr * 2, hr * 2,
            QPen(QColor(0x18, 0x90, 0xFF), 2), QBrush(Qt::white));
        h->setZValue(kZWireHandle);
        h->setData(RoleKind, KindWireHandle);
        h->setData(RoleId, QString::number(m_selectedLinkIndex));
        h->setData(RoleDir, i - 1);
        h->setCursor(Qt::SizeAllCursor);
        m_wireVertexHandles.push_back(h);
    }
    // 选中线加粗，跳过两端引出段
    for (int si = 0; si + 1 < full.size(); ++si) {
        const QPointF p0 = full[si];
        const QPointF p1 = full[si + 1];
        const qreal len = QLineF(p0, p1).length();
        if (len < 12.0) {
            continue;
        }
        const QPointF mid((p0.x() + p1.x()) * 0.5, (p0.y() + p1.y()) * 0.5);
        const bool horiz = qAbs(p0.y() - p1.y()) < 0.5;
        const qreal sw = horiz ? 10.0 : 6.0;
        const qreal sh = horiz ? 6.0 : 10.0;
        QGraphicsRectItem* h = m_scene->addRect(mid.x() - sw * 0.5, mid.y() - sh * 0.5, sw, sh,
            QPen(QColor(0x18, 0x90, 0xFF), 1.5), QBrush(QColor(0x18, 0x90, 0xFF, 80)));
        h->setZValue(kZWireHandle - 0.05);
        h->setData(RoleKind, KindWireSegHandle);
        h->setData(RoleId, QString::number(m_selectedLinkIndex));
        h->setData(RoleDir, si);
        h->setCursor(horiz ? Qt::SizeVerCursor : Qt::SizeHorCursor);
        m_wireSegHandles.push_back(h);
    }
}
// 按节点位置更新连线
void JobCanvas::updateWires()
{
    if (!m_job) {
        return;
    }
    for (int i = 0; i < m_wires.size(); ++i) {
        const CanvasWireRef& w = m_wires[i];
        if (!w.item) {
            continue;
        }
        QGraphicsItem* fromPin = m_pinItems.value(w.fromKey);
        QGraphicsItem* toPin = m_pinItems.value(w.toKey);
        if (!fromPin || !toPin) {
            continue;
        }
        const QPointF a = pinSceneCenter(fromPin);
        const QPointF b = pinSceneCenter(toPin);
        w.item->setPath(wirePathForLink(w.linkIndex, a, b, 0));
    }
    updateWireHandles();
}

// 更新执行顺序线
void JobCanvas::updateOrderLines()
{
    for (int i = 0; i < m_orderLines.size(); ++i) {
        CanvasOrderRef& r = m_orderLines[i];
        if (!r.item || !r.from || !r.to) {
            continue;
        }
        QPointF a = flowNodeBottom(r.from);
        QPointF b = flowNodeTop(r.to);
        if (a.isNull() || b.isNull()) {
            continue;
        }
        a.setY(a.y() + 3.0);
        b.setY(b.y() - 3.0);
        r.item->setPath(makeExecOrderPath(a, b));
    }
}

// 节点拖完后写回坐标
void JobCanvas::finishNodeMove()
{
    if (m_movingNode && m_moveItem && !m_moveId.isEmpty()) {
        if (m_movingGroup) {
            QGraphicsRectItem* frame = qgraphicsitem_cast<QGraphicsRectItem*>(m_moveItem);
            if (frame) {
                const QPointF tl = frame->mapToScene(frame->rect().topLeft());
                emit groupMoved(frame->data(RoleId).toString(), tl.x(), tl.y());
            }
            m_moveItem->setZValue(kZGroupFrame);
            refitGroupChain(m_moveItem);
        } else {
            QPointF tl = m_moveItem->scenePos();
            QGraphicsRectItem* box = qgraphicsitem_cast<QGraphicsRectItem*>(m_moveItem);
            if (box) {
                tl = box->mapToScene(box->rect().topLeft());
            }
            m_moveItem->setZValue(kZToolConfig);
            refitGroupChain(m_moveItem);
            applyNodeWireSnaps();
            emit nodeMoved(m_moveId, tl.x(), tl.y());
        }
    }
    m_movingNode = false;
    m_movingGroup = false;
    m_moveArmed = false;
    m_moveItem = 0;
    m_nodeWireSnaps.clear();
    unsetCursor();
}

// 鼠标按下
void JobCanvas::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panLast = e->pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    if (e->button() != Qt::LeftButton) {
        QGraphicsView::mousePressEvent(e);
        return;
    }
    QGraphicsItem* hitItem = itemAt(e->pos());
    while (hitItem) {
        const int hk = hitItem->data(RoleKind).toInt();
        if (hk == KindWireHandle || hk == KindWireSegHandle) {
            m_draggingWireHandle = true;
            m_dragLinkIndex = hitItem->data(RoleId).toInt();
            m_dragIsSegment = (hk == KindWireSegHandle);
            m_dragVertexIndex = m_dragIsSegment ? -1 : hitItem->data(RoleDir).toInt();
            m_dragSegIndex = m_dragIsSegment ? hitItem->data(RoleDir).toInt() : -1;
            m_selectedLinkIndex = m_dragLinkIndex;
            if (m_job && m_dragLinkIndex >= 0
                && m_dragLinkIndex < static_cast<int>(m_job->links.size())) {
                for (int wi = 0; wi < m_wires.size(); ++wi) {
                    if (m_wires[wi].linkIndex != m_dragLinkIndex) {
                        continue;
                    }
                    QGraphicsItem* fromPin = m_pinItems.value(m_wires[wi].fromKey);
                    QGraphicsItem* toPin = m_pinItems.value(m_wires[wi].toKey);
                    if (fromPin && toPin) {
                        const QPointF a = pinSceneCenter(fromPin);
                        const QPointF b = pinSceneCenter(toPin);
                        beginCustomPreviewFromAuto(m_dragLinkIndex, a, b);
                        m_dragPressScene = mapToScene(e->pos());
                        m_dragBaseMids = m_previewMids;
                        if (m_dragIsSegment) {
                            const PinStubSplit sp = splitPinStubForDrag(&m_previewMids, m_dragSegIndex, a, b);
                            m_dragBaseMids = m_previewMids;
                            if (sp.bumpCount > 0) {
                                for (int hi = 0; hi < m_wireSegHandles.size(); ++hi) {
                                    QGraphicsRectItem* h = m_wireSegHandles[hi];
                                    if (!h) {
                                        continue;
                                    }
                                    int si = h->data(RoleDir).toInt();
                                    if (si >= sp.bumpAt) {
                                        si += sp.bumpCount;
                                    }
                                    h->setData(RoleDir, si);
                                }
                                for (int hi = 0; hi < m_wireVertexHandles.size(); ++hi) {
                                    QGraphicsEllipseItem* h = m_wireVertexHandles[hi];
                                    if (!h) {
                                        continue;
                                    }
                                    int vi = h->data(RoleDir).toInt();
                                    if (vi + 1 >= sp.bumpAt) {
                                        vi += sp.bumpCount;
                                    }
                                    h->setData(RoleDir, vi);
                                }
                            }
                            hitItem->setData(RoleDir, sp.segIndex);
                            m_dragSegIndex = sp.segIndex;
                        }
                    }
                    break;
                }
            }
            setCursor(m_dragIsSegment ? hitItem->cursor() : Qt::SizeAllCursor);
            e->accept();
            return;
        }
        hitItem = hitItem->parentItem();
    }
    QString id;
    int kind = CanvasHitBlank;
    QString port;
    int dir = 0;
    hitAt(e->pos(), &id, &kind, &port, &dir);
    // 按下的是针脚则开始拖线
    QString pinId;
    QString pinPort;
    int pinDir = 0;
    if (hitPinAt(e->pos(), &pinId, &pinPort, &pinDir) && pinDir == 1) {
        m_wiring = true;
        m_wireFromId = pinId;
        m_wireFromPort = pinPort;
        m_wireFromData = portDataFromKey(ss(pinPort));
        m_wireFromPos = mapToScene(e->pos());
        QPen pen(dragWireColor(m_wireFromData, false), 1.6, Qt::DashLine);
        pen.setCapStyle(Qt::RoundCap);
        m_tempWire = m_scene->addPath(makeWirePathAuto(m_wireFromPos, m_wireFromPos), pen);
        m_tempWire->setBrush(Qt::NoBrush);
        m_tempWire->setZValue(kZTempWire);
        m_tempWire->setAcceptedMouseButtons(Qt::NoButton);
        e->accept();
        return;
    }
    if (kind == CanvasHitTool && !port.isEmpty() && dir == 1) {
        m_wiring = true;
        m_wireFromId = id;
        m_wireFromPort = port;
        m_wireFromData = portDataFromKey(ss(port));
        m_wireFromPos = mapToScene(e->pos());
        QPen pen(dragWireColor(m_wireFromData, false), 1.6, Qt::DashLine);
        pen.setCapStyle(Qt::RoundCap);
        m_tempWire = m_scene->addPath(makeWirePathAuto(m_wireFromPos, m_wireFromPos), pen);
        m_tempWire->setBrush(Qt::NoBrush);
        m_tempWire->setZValue(kZTempWire);
        m_tempWire->setAcceptedMouseButtons(Qt::NoButton);
        e->accept();
        return;
    }
    QGraphicsItem* box = toolBoxAt(e->pos());
    if (box) {
        m_moveArmed = true;
        m_movingNode = false;
        m_movingGroup = false;
        m_moveItem = box;
        m_moveId = box->data(RoleId).toString();
        m_moveGrab = mapToScene(e->pos()) - itemSceneTopLeft(box);
        m_movePressView = e->pos();
        m_selectedGroupId.clear();
        e->accept();
        return;
    }
    QGraphicsItem* frame = groupFrameAt(e->pos());
    if (frame) {
        m_moveArmed = true;
        m_movingNode = false;
        m_movingGroup = true;
        m_moveItem = frame;
        m_moveId = frame->data(RoleId).toString();
        m_moveGrab = mapToScene(e->pos()) - itemSceneTopLeft(frame);
        m_movePressView = e->pos();
        m_selectedGroupId = m_moveId;
        m_selectedLinkIndex = -1;
        refreshWireSelectionVisual();
        emit groupClicked(m_moveId);
        e->accept();
        return;
    }
    QVector<int> wireHits;
    collectWireHitsAt(e->pos(), &wireHits);
    if (!wireHits.isEmpty()) {
        const bool sameSpot = (QLineF(QPointF(m_wirePickPos), QPointF(e->pos())).length() < 8.0
                               && wireHits == m_wirePickList);
        if (sameSpot && m_wirePickList.size() > 1) {
            m_wirePickIndex = (m_wirePickIndex + 1) % m_wirePickList.size();
        } else {
            m_wirePickList = wireHits;
            m_wirePickIndex = 0;
            m_wirePickPos = e->pos();
        }
        m_selectedLinkIndex = m_wirePickList[m_wirePickIndex];
        refreshWireSelectionVisual();
        emit wireClicked(m_selectedLinkIndex);
        e->accept();
        return;
    }
    m_wirePickList.clear();
    m_wirePickIndex = 0;
    if (kind == CanvasHitGroup) {
        m_selectedLinkIndex = -1;
        refreshWireSelectionVisual();
        ToolGroup* g = m_job ? const_cast<JobCfg*>(m_job)->findGroup(oid(id)) : nullptr;
        if (g && canvasSplit(*g)) {
            m_selectedGroupId = id;
        } else {
            m_selectedGroupId.clear();
        }
            emit groupClicked(id);
    } else if (kind == CanvasHitTool && !id.isEmpty()) {
        m_selectedLinkIndex = -1;
        m_selectedGroupId.clear();
        refreshWireSelectionVisual();
        emit toolClicked(id);
    } else if (kind == CanvasHitBlank) {
        m_selectedLinkIndex = -1;
        refreshWireSelectionVisual();
        m_panning = true;
        m_panLast = e->pos();
        setCursor(Qt::OpenHandCursor);
        e->accept();
        return;
    }
    QGraphicsView::mousePressEvent(e);
}

// 双击工具打开界面；双击拐点删点；双击连线段加点
void JobCanvas::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        QGraphicsView::mouseDoubleClickEvent(e);
        return;
    }
    QGraphicsItem* hitItem = itemAt(e->pos());
    while (hitItem) {
        const int hk = hitItem->data(RoleKind).toInt();
        if (hk == KindWireHandle) {
            const int linkIndex = hitItem->data(RoleId).toInt();
            const int vi = hitItem->data(RoleDir).toInt();
            for (int wi = 0; wi < m_wires.size(); ++wi) {
                if (m_wires[wi].linkIndex != linkIndex) {
                    continue;
                }
                QGraphicsItem* fromPin = m_pinItems.value(m_wires[wi].fromKey);
                QGraphicsItem* toPin = m_pinItems.value(m_wires[wi].toKey);
                if (!fromPin || !toPin) {
                    break;
                }
                const QPointF a = pinSceneCenter(fromPin);
                const QPointF b = pinSceneCenter(toPin);
                QVector<QPointF> mids = midPointsForLink(linkIndex, a, b);
                mids = removeMidAt(mids, vi, a, b);
                m_pendingRoutePoints = mids;
                m_selectedLinkIndex = linkIndex;
                emit linkRouteChanged(linkIndex);
                e->accept();
                return;
            }
            break;
        }
        hitItem = hitItem->parentItem();
    }
    QGraphicsItem* box = toolBoxAt(e->pos());
    if (box) {
        emit toolDoubleClicked(box->data(RoleId).toString());
        e->accept();
        return;
    }
    QVector<int> wireHits;
    collectWireHitsAt(e->pos(), &wireHits);
    if (!wireHits.isEmpty()) {
        const int linkIndex = wireHits.first();
        for (int wi = 0; wi < m_wires.size(); ++wi) {
            if (m_wires[wi].linkIndex != linkIndex || !m_wires[wi].item) {
                continue;
            }
            QGraphicsItem* fromPin = m_pinItems.value(m_wires[wi].fromKey);
            QGraphicsItem* toPin = m_pinItems.value(m_wires[wi].toKey);
            if (!fromPin || !toPin) {
                break;
            }
            const QPointF a = pinSceneCenter(fromPin);
            const QPointF b = pinSceneCenter(toPin);
            QVector<QPointF> mids = midPointsForLink(linkIndex, a, b);
            const QVector<QPointF> full = rawFullFromMids(a, b, mids);
            const QPointF sp = mapToScene(e->pos());
            int bestSeg = 1;
            qreal bestDist = 1e18;
            for (int si = 0; si + 1 < full.size(); ++si) {
                const QLineF seg(full[si], full[si + 1]);
                // 点到线段的距离
                const qreal len2 = seg.dx() * seg.dx() + seg.dy() * seg.dy();
                qreal t = 0;
                if (len2 > 1e-6) {
                    t = ((sp.x() - seg.x1()) * seg.dx() + (sp.y() - seg.y1()) * seg.dy()) / len2;
                }
                if (t < 0) {
                    t = 0;
                }
                if (t > 1) {
                    t = 1;
                }
                const QPointF proj(seg.x1() + t * seg.dx(), seg.y1() + t * seg.dy());
                const qreal d = QLineF(sp, proj).length();
                if (d < bestDist) {
                    bestDist = d;
                    bestSeg = si;
                }
            }
            mids = insertMidOnSegment(mids, bestSeg, a, b, sp);
            m_pendingRoutePoints = mids;
            m_selectedLinkIndex = linkIndex;
            emit linkRouteChanged(linkIndex);
            e->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(e);
}

// 鼠标移动
void JobCanvas::mouseMoveEvent(QMouseEvent* e)
{
    if (m_panning) {
        setCursor(Qt::ClosedHandCursor);
        const QPoint d = e->pos() - m_panLast;
        m_panLast = e->pos();
        if (horizontalScrollBar()) {
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        }
        if (verticalScrollBar()) {
            verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        }
        e->accept();
        return;
    }
    if (m_draggingWireHandle && m_dragLinkIndex >= 0) {
        const QPointF sp = mapToScene(e->pos());
        QPointF a;
        QPointF b;
        for (int wi = 0; wi < m_wires.size(); ++wi) {
            if (m_wires[wi].linkIndex != m_dragLinkIndex) {
                continue;
            }
            QGraphicsItem* fromPin = m_pinItems.value(m_wires[wi].fromKey);
            QGraphicsItem* toPin = m_pinItems.value(m_wires[wi].toKey);
            if (fromPin && toPin) {
                a = pinSceneCenter(fromPin);
                b = pinSceneCenter(toPin);
            }
            break;
        }
        if (m_dragIsSegment) {
            applySegmentDrag(&m_previewMids, m_dragBaseMids, m_dragSegIndex, m_dragPressScene, sp, a, b);
        } else {
            applyVertexDrag(&m_previewMids, m_dragBaseMids, m_dragVertexIndex, m_dragPressScene, sp, a, b);
        }
        updateWires();
        // 拖动已有手柄
        const qreal hr = 5.5;
        for (int hi = 0; hi < m_wireVertexHandles.size(); ++hi) {
            QGraphicsEllipseItem* h = m_wireVertexHandles[hi];
            if (!h) {
                continue;
            }
            const int idx = h->data(RoleDir).toInt();
            if (idx < 0 || idx >= m_previewMids.size()) {
                continue;
            }
            const QPointF p = m_previewMids[idx];
            h->setRect(p.x() - hr, p.y() - hr, hr * 2.0, hr * 2.0);
        }
        const QVector<QPointF> full = rawFullFromMids(a, b, m_previewMids);
        for (int hi = 0; hi < m_wireSegHandles.size(); ++hi) {
            QGraphicsRectItem* h = m_wireSegHandles[hi];
            if (!h) {
                continue;
            }
            const int si = h->data(RoleDir).toInt();
            if (si < 0 || si + 1 >= full.size()) {
                continue;
            }
            const QPointF p0 = full[si];
            const QPointF p1 = full[si + 1];
            const QPointF mid((p0.x() + p1.x()) * 0.5, (p0.y() + p1.y()) * 0.5);
            const bool horiz = qAbs(p0.y() - p1.y()) <= qAbs(p0.x() - p1.x());
            const qreal sw = horiz ? 10.0 : 6.0;
            const qreal sh = horiz ? 6.0 : 10.0;
            h->setRect(mid.x() - sw * 0.5, mid.y() - sh * 0.5, sw, sh);
        }
        e->accept();
        return;
    }
    if (m_moveArmed && m_moveItem) {
        if (!m_movingNode) {
            if (QLineF(QPointF(m_movePressView), QPointF(e->pos())).length() < 6.0) {
                e->accept();
                return;
            }
            captureNodeWireSnaps();
            m_movingNode = true;
            m_moveItem->setZValue(m_movingGroup ? kZDragGroup : kZDragTool);
            setCursor(Qt::ClosedHandCursor);
        }
        setItemSceneTopLeft(m_moveItem, mapToScene(e->pos()) - m_moveGrab);
        refitGroupChain(m_moveItem);
        applyNodeWireSnaps();
        updateWires();
        updateOrderLines();
        if (m_scene) {
            QRectF r = m_scene->sceneRect();
            r = r.united(m_moveItem->sceneBoundingRect().adjusted(-48, -48, 48, 48));
            m_scene->setSceneRect(r);
        }
        e->accept();
        return;
    }
    if (m_wiring && m_tempWire) {
        QString toId;
        QString toPort;
        int dir = 0;
        hitPinAt(e->pos(), &toId, &toPort, &dir);
        bool mismatch = false;
        // 松开时若落在端口上则连线
        if (dir == 0 && !toPort.isEmpty()) {
            const PortData td = portDataFromKey(ss(toPort));
            mismatch = (td != m_wireFromData);
            if (mismatch) {
                QToolTip::showText(e->globalPos(), typeMismatchTip(m_wireFromData, td), this);
            } else {
                QToolTip::hideText();
            }
        } else {
            QToolTip::hideText();
        }
        QPen pen(dragWireColor(m_wireFromData, mismatch), 1.6, Qt::DashLine);
        pen.setCapStyle(Qt::RoundCap);
        m_tempWire->setPen(pen);
        m_tempWire->setPath(makeWirePathAuto(m_wireFromPos, mapToScene(e->pos())));
        e->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(e);
}

// 鼠标松开
void JobCanvas::mouseReleaseEvent(QMouseEvent* e)
{
    if (m_panning && (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton)) {
        m_panning = false;
        unsetCursor();
        e->accept();
        return;
    }
    if (e->button() == Qt::LeftButton && m_draggingWireHandle) {
        m_draggingWireHandle = false;
        if (m_dragLinkIndex >= 0) {
            QPointF ra;
            QPointF rb;
            bool haveEnds = false;
            for (int wi = 0; wi < m_wires.size(); ++wi) {
                if (m_wires[wi].linkIndex != m_dragLinkIndex) {
                    continue;
                }
                QGraphicsItem* fromPin = m_pinItems.value(m_wires[wi].fromKey);
                QGraphicsItem* toPin = m_pinItems.value(m_wires[wi].toKey);
                if (fromPin && toPin) {
                    ra = pinSceneCenter(fromPin);
                    rb = pinSceneCenter(toPin);
                    haveEnds = true;
                }
                break;
            }
            if (haveEnds) {
                QVector<QPointF> done = rawFullFromMids(ra, rb, m_previewMids);
                collapseShortJogs(&done);
                m_previewMids = midsFromFullRoute(done);
            }
            m_pendingRoutePoints = m_previewMids;
            emit linkRouteChanged(m_dragLinkIndex);
        }
        m_previewRoute = false;
        m_previewLinkIndex = -1;
        m_previewMids.clear();
        unsetCursor();
        e->accept();
        return;
    }
    if (e->button() == Qt::LeftButton && m_wiring) {
        QString toId;
        QString toPort;
        int dir = 0;
        hitPinAt(e->pos(), &toId, &toPort, &dir);
        if (m_tempWire) {
            m_scene->removeItem(m_tempWire);
            delete m_tempWire;
            m_tempWire = nullptr;
        }
        m_wiring = false;
        if (dir == 0 && !toId.isEmpty() && !toPort.isEmpty()) {
            const PortData td = portDataFromKey(ss(toPort));
            if (td != m_wireFromData) {
                QToolTip::showText(e->globalPos(), typeMismatchTip(m_wireFromData, td), this);
            } else {
                emit linkPorts(m_wireFromId, m_wireFromPort, toId, toPort);
            }
        }
        e->accept();
        return;
    }
    if (e->button() == Qt::LeftButton && (m_moveArmed || m_movingNode)) {
        const QString id = m_moveId;
        const bool wasGroup = m_movingGroup;
        finishNodeMove();
        if (!wasGroup && !id.isEmpty()) {
            emit toolClicked(id);
        }
        e->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(e);
}

// 滚轮缩放
void JobCanvas::wheelEvent(QWheelEvent* e)
{
    const qreal f = (e->delta() > 0) ? 1.12 : (1.0 / 1.12);
    const qreal next = m_zoom * f;
    if (next < 0.3 || next > 3.0) {
        e->accept();
        return;
    }
    m_zoom = next;
    scale(f, f);
    e->accept();
}

// 右键：针脚当工具、连线发 wire、空白也可弹菜单
void JobCanvas::contextMenuEvent(QContextMenuEvent* e)
{
    QString id;
    int kind = CanvasHitBlank;
    QString port;
    int dir = 0;
    hitAt(e->pos(), &id, &kind, &port, &dir);
    QVector<int> wireHits;
    collectWireHitsAt(e->pos(), &wireHits);
    if (!wireHits.isEmpty()) {
        id = QString::number(wireHits.first());
        kind = CanvasHitWire;
        m_selectedLinkIndex = wireHits.first();
        refreshWireSelectionVisual();
        emit wireClicked(m_selectedLinkIndex);
        emit canvasMenuRequested(id, kind, e->globalPos());
        e->accept();
        return;
    }
    QGraphicsItem* box = toolBoxAt(e->pos());
    if (box) {
        id = box->data(RoleId).toString();
        kind = CanvasHitTool;
    }
    if (kind == CanvasHitTool && !id.isEmpty()) {
        emit toolClicked(id);
    } else if (kind == CanvasHitGroup && !id.isEmpty()) {
        emit groupClicked(id);
    }
    emit canvasMenuRequested(id, kind, e->globalPos());
    e->accept();
}

// 0 复位缩放；Delete 删除选中连线最后一个拐点；无拐点则恢复自动路由
void JobCanvas::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_0 && !(e->modifiers() & Qt::ControlModifier)) {
        resetZoom();
        e->accept();
        return;
    }
    if (e->key() == Qt::Key_0 && (e->modifiers() & Qt::ControlModifier)) {
        resetZoom();
        e->accept();
        return;
    }
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)
        && m_selectedLinkIndex >= 0 && m_job
        && m_selectedLinkIndex < static_cast<int>(m_job->links.size())) {
        const ToolLink& L = m_job->links[static_cast<size_t>(m_selectedLinkIndex)];
        if (L.routeCustom && !L.routePoints.empty()) {
            for (int wi = 0; wi < m_wires.size(); ++wi) {
                if (m_wires[wi].linkIndex != m_selectedLinkIndex) {
                    continue;
                }
                QGraphicsItem* fromPin = m_pinItems.value(m_wires[wi].fromKey);
                QGraphicsItem* toPin = m_pinItems.value(m_wires[wi].toKey);
                if (!fromPin || !toPin) {
                    break;
                }
                const QPointF a = pinSceneCenter(fromPin);
                const QPointF b = pinSceneCenter(toPin);
                QVector<QPointF> mids = routeMidsToQ(L.routePoints);
                mids = removeMidAt(mids, mids.size() - 1, a, b);
                if (mids.isEmpty()) {
                    emit linkRouteReset(m_selectedLinkIndex);
                } else {
                    m_pendingRoutePoints = mids;
                    emit linkRouteChanged(m_selectedLinkIndex);
                }
                e->accept();
                return;
            }
        } else if (L.routeCustom) {
            emit linkRouteReset(m_selectedLinkIndex);
            e->accept();
            return;
        }
    }
    QGraphicsView::keyPressEvent(e);
}

// 尺寸变化后重排滚动条
void JobCanvas::resizeEvent(QResizeEvent* e)
{
    QGraphicsView::resizeEvent(e);
    layoutLeftScrollBar();
}

// 垂直滚动
void JobCanvas::onVertScroll(int value)
{
    if (m_blockScrollSync) {
        return;
    }
    m_blockScrollSync = true;
    QScrollBar* v = verticalScrollBar();
    m_vScrollLeft->setRange(v->minimum(), v->maximum());
    m_vScrollLeft->setPageStep(v->pageStep());
    m_vScrollLeft->setSingleStep(v->singleStep());
    if (m_vScrollLeft->value() != value) {
        m_vScrollLeft->setValue(value);
    }
    layoutLeftScrollBar();
    m_blockScrollSync = false;
}

// 左侧滚动
void JobCanvas::onLeftScroll(int value)
{
    if (m_blockScrollSync) {
        return;
    }
    m_blockScrollSync = true;
    verticalScrollBar()->setValue(value);
    m_blockScrollSync = false;
}

// 布置左侧滚动条
void JobCanvas::layoutLeftScrollBar()
{
    if (!m_vScrollLeft) {
        return;
    }
    QScrollBar* v = verticalScrollBar();
    const int sbw = m_vScrollLeft->sizeHint().width();
    const bool vert = v && v->isVisible();
    m_vScrollLeft->setVisible(vert);
    const bool horiz = horizontalScrollBar() && horizontalScrollBar()->isVisible();
    const int horizH = horiz ? horizontalScrollBar()->height() : 0;
    if (vert) {
        setViewportMargins(sbw, 0, 0, 0);
        m_vScrollLeft->setGeometry(0, 0, sbw, height() - horizH);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }
}

// 缩放比例
qreal JobCanvas::zoomScale() const
{
    return m_zoom;
}



// 待定路由点
QVector<QPointF> JobCanvas::pendingRoutePoints() const
{
    return m_pendingRoutePoints;
}



}

#include "moc_JobCanvas.cpp"
