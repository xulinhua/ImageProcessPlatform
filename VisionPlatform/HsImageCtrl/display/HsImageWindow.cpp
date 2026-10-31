/**
 * @file HsImageWindow.cpp
 * @brief 图像显示窗口实现：分层场景、视口、ROI 交互、测量、绘制会话与前景 HUD
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCtrl/HsImageWindow.h>
#include <HsCommData/HsCommData.h>
#include "FrameConvert.h"
#include "GraphicItem.h"
#include "ImageItem.h"
#include "ImageLayer.h"
#include "StackLayer.h"
#include "DrawingLayer.h"
#include "MeasureLayer.h"
#include "ForegroundLayer.h"
#include "HsImageWindowMenu.h"
#include "LayerItem.h"
#include "QtGeom.h"
#include <HsCtrl/canvas/Viewport.h>

#ifdef HSIMAGE_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QColor>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QStandardPaths>
#include <QButtonGroup>
#include <QToolButton>
#include <QTransform>
#include <QVector>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace HsCtrl {

// 像素检验：格子边长够大才填可见格；格数封顶，避免拖动卡顿
static const int kHandleBody = -2;          ///< 拖图元本体（非手柄）
static const qreal kInspectMinCellPx = 28.0; ///< 高倍画格子时单格最小边长（屏幕像素）
static const int kInspectMaxCells = 1600;    ///< 可见格数上限
static const int kInspectNeighborHalf = 3;   ///< 邻域表半宽（格）
static const int kInspectNeighborCell = 22;  ///< 灰度邻域格边长（像素）
static const int kInspectNeighborCellRgbW = 36; ///< 彩色邻域格宽（像素）
static const int kInspectNeighborCellRgbH = 42; ///< 彩色邻域格高（像素）
static const qreal kZoomInFactor = 1.25;  ///< 单步放大倍率，与滚轮一致
static const qreal kZoomOutFactor = 0.8;  ///< 单步缩小倍率，与滚轮一致

// 浮动工具条按钮的图标种类，存在按钮属性 overlayIcon 里
enum OverlayIconKind {
    IconSnap = 1,   ///< 保存快照
    IconZoomIn,     ///< 放大
    IconZoomOut,    ///< 缩小
    IconFit,        ///< 适应窗口
    IconOne,        ///< 实际像素 1:1
    IconDist,       ///< 测距离
    IconAngle,      ///< 测夹角
    IconRadius,     ///< 测半径
    IconRect,       ///< 测矩形
    IconEllipse     ///< 测椭圆
};

// 用指定颜色画 18x18 透明底按钮图标，换皮肤时重画
static QIcon paintOverlayIcon(int kind, const QColor& color)
{
    QPixmap pm(18, 18);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    // 默认只描线；相机镜头、端点等实心部位临时换成同色画刷
    switch (kind) {
    case IconSnap:
        p.drawRoundedRect(QRectF(1.5, 4.0, 15, 10), 1.2, 1.2);
        p.setBrush(color);
        p.drawEllipse(QRectF(7.0, 6.2, 4.0, 4.0));
        break;
    case IconZoomIn:
    case IconZoomOut:
        p.drawEllipse(QRectF(1.5, 1.5, 9.0, 9.0));
        p.drawLine(QPointF(9.2, 9.2), QPointF(15.2, 15.2));
        p.drawLine(QPointF(3.2, 6.0), QPointF(8.8, 6.0));
        if (kind == IconZoomIn) {
            p.drawLine(QPointF(6.0, 3.2), QPointF(6.0, 8.8));
        }
        break;
    case IconFit:
        p.drawLine(QPointF(1.5, 1.5), QPointF(6.0, 1.5));
        p.drawLine(QPointF(1.5, 1.5), QPointF(1.5, 6.0));
        p.drawLine(QPointF(16.5, 1.5), QPointF(12.0, 1.5));
        p.drawLine(QPointF(16.5, 1.5), QPointF(16.5, 6.0));
        p.drawLine(QPointF(1.5, 16.5), QPointF(6.0, 16.5));
        p.drawLine(QPointF(1.5, 16.5), QPointF(1.5, 12.0));
        p.drawLine(QPointF(16.5, 16.5), QPointF(12.0, 16.5));
        p.drawLine(QPointF(16.5, 16.5), QPointF(16.5, 12.0));
        p.drawRect(QRectF(5.5, 5.5, 7.0, 7.0));
        break;
    case IconOne:
        p.drawRect(QRectF(2.0, 2.0, 14.0, 14.0));
        p.drawLine(QPointF(9.0, 2.0), QPointF(9.0, 16.0));
        p.drawLine(QPointF(2.0, 9.0), QPointF(16.0, 9.0));
        break;
    case IconDist:
        p.drawLine(QPointF(3.0, 13.0), QPointF(15.0, 5.0));
        p.setBrush(color);
        p.drawEllipse(QPointF(3.0, 13.0), 1.6, 1.6);
        p.drawEllipse(QPointF(15.0, 5.0), 1.6, 1.6);
        break;
    case IconAngle:
        p.drawLine(QPointF(2.0, 14.5), QPointF(16.0, 14.5));
        p.drawLine(QPointF(2.0, 14.5), QPointF(14.5, 3.0));
        p.drawArc(QRectF(2.0, 8.5, 8.0, 8.0), 0 * 16, 70 * 16);
        break;
    case IconRadius:
        p.drawEllipse(QRectF(2.0, 2.0, 14.0, 14.0));
        p.drawLine(QPointF(9.0, 9.0), QPointF(15.0, 4.5));
        p.setBrush(color);
        p.drawEllipse(QPointF(9.0, 9.0), 1.3, 1.3);
        break;
    case IconRect:
        p.drawRect(QRectF(3.0, 4.0, 12.0, 8.0));
        p.drawLine(QPointF(3.0, 15.2), QPointF(15.0, 15.2));
        p.drawLine(QPointF(3.0, 14.0), QPointF(3.0, 16.4));
        p.drawLine(QPointF(15.0, 14.0), QPointF(15.0, 16.4));
        break;
    case IconEllipse:
        p.drawEllipse(QRectF(2.0, 5.0, 14.0, 8.0));
        p.drawLine(QPointF(2.0, 9.0), QPointF(16.0, 9.0));
        p.drawLine(QPointF(9.0, 5.0), QPointF(9.0, 13.0));
        break;
    default:
        break;
    }
    p.end();
    return QIcon(pm);
}

// 把 HsCommData::Graphic 注册为 Qt 元类型，供跨线程信号排队；只注册一次
void registerGraphicMetaTypes()
{
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    qRegisterMetaType<HsCommData::Graphic>("HsCommData::Graphic");
}

// 鼠标交互状态机
enum InteractionState {
    StateBrowse = 0, ///< 空闲：可平移或点选
    StatePan,        ///< 拖动平移视口
    StateDraw,       ///< 橡皮筋拖出新 ROI
    StateEditHandle  ///< 拖动已有 ROI 的手柄或本体
};

// 按图元种类生成新 ROI 的 id 前缀
static QString kindPrefix(const HsCommData::Graphic& g)
{
    if (g.kind == HsCommData::KindObject && g.geom.isInitialized()) {
        switch (g.geom.kind()) {
        case HsCommData::ObjCircle:  return QStringLiteral("roi.circle");
        case HsCommData::ObjEllipse: return QStringLiteral("roi.ellipse");
        case HsCommData::ObjRect1:   return QStringLiteral("roi.rect1");
        case HsCommData::ObjRect2:   return QStringLiteral("roi.rect2");
        case HsCommData::ObjLine:    return QStringLiteral("roi.line");
        case HsCommData::ObjContour: return QStringLiteral("roi.contour");
        case HsCommData::ObjRegion:  return QStringLiteral("roi.region");
        default: break;
        }
    }
    switch (g.kind) {
    case HsCommData::KindCross: return QStringLiteral("roi.cross");
    case HsCommData::KindText:  return QStringLiteral("roi.text");
    default:                    return QStringLiteral("roi.item");
    }
}

// 绘制工具对应的 GraphicKind；十字、文本单列，其余都是几何对象
static HsCommData::GraphicKind toolKind(DrawTool t)
{
    switch (t) {
    case ToolCross: return HsCommData::KindCross;
    case ToolText:  return HsCommData::KindText;
    default:        return HsCommData::KindObject;
    }
}

// 该工具能否开绘制会话；浏览、文本等返回 false
static bool sessionToolOk(DrawTool t)
{
    switch (t) {
    case ToolCircle:
    case ToolRect1:
    case ToolRect2:
    case ToolEllipse:
    case ToolLine:
    case ToolCross:
    case ToolContour:
    case ToolPolygon:
    case ToolBrush:
    case ToolMeasureDist:
    case ToolMeasureAngle:
    case ToolMeasureRadius:
    case ToolMeasureRect:
    case ToolMeasureEllipse:
        return true;
    default:
        return false;
    }
}

// 是否为测量工具
static bool measureTool(DrawTool t)
{
    return t == ToolMeasureDist || t == ToolMeasureAngle || t == ToolMeasureRadius
        || t == ToolMeasureRect || t == ToolMeasureEllipse;
}

// id 前缀是否属于测量图元（m.dist / m.ang / m.rad / m.rect / m.ell）
static bool measureItemId(const QString& id)
{
    return id.startsWith(QLatin1String("m.dist"))
        || id.startsWith(QLatin1String("m.ang"))
        || id.startsWith(QLatin1String("m.rad"))
        || id.startsWith(QLatin1String("m.rect"))
        || id.startsWith(QLatin1String("m.ell"));
}


// 测量图元的读数文本，按标定换算单位；不是测量图元返回空串
static QString measureLabel(DrawTool hint, bool preview, const HsCommData::Graphic& g,
                            const HsCommData::ImageCalib& cal)
{
    // 先按 id 前缀定种类；预览中的橡皮筋还没有测量 id，用当前工具 hint
    DrawTool kind = ToolBrowse;
    if (g.id.compare(0, 6, "m.dist") == 0) {
        kind = ToolMeasureDist;
    } else if (g.id.compare(0, 5, "m.ang") == 0) {
        kind = ToolMeasureAngle;
    } else if (g.id.compare(0, 5, "m.rad") == 0) {
        kind = ToolMeasureRadius;
    } else if (g.id.compare(0, 6, "m.rect") == 0) {
        kind = ToolMeasureRect;
    } else if (g.id.compare(0, 5, "m.ell") == 0) {
        kind = ToolMeasureEllipse;
    } else if (preview && measureTool(hint)) {
        kind = hint;
    } else {
        return QString();
    }
    // 种类与几何须对得上，否则不出读数
    if (kind == ToolMeasureDist && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjLine) {
        const HsCommData::LineP* ln = g.geom.lineAt(0);
        if (!ln) {
            return QString();
        }
        const std::string s = HsCommData::formatLength(ln->b.x - ln->a.x, ln->b.y - ln->a.y, cal);
        return QString::fromUtf8(s.c_str());
    }
    if (kind == ToolMeasureRadius && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjCircle) {
        const HsCommData::CircleP* c = g.geom.circleAt(0);
        if (!c) {
            return QString();
        }
        const std::string s = HsCommData::formatRadius(c->r, cal);
        return QString::fromUtf8(s.c_str());
    }
    if (kind == ToolMeasureAngle && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjContour) {
        const HsCommData::HsContour* ct = g.geom.contourAt(0);
        if (!ct || ct->ptCount() < 3) {
            return QString();
        }
        const std::string s = HsCommData::formatAngle(ct->ptAt(0), ct->ptAt(1), ct->ptAt(2), cal);
        return QString::fromUtf8(s.c_str());
    }
    if (kind == ToolMeasureRect && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjRect1) {
        const HsCommData::Rect1P* rc = g.geom.rect1At(0);
        if (!rc) {
            return QString();
        }
        const HsCommData::Box2 box = HsCommData::normalized(rc->r);
        const std::string s = HsCommData::formatRect(HsCommData::width(box), HsCommData::height(box), cal);
        return QString::fromUtf8(s.c_str());
    }
    if (kind == ToolMeasureEllipse && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjEllipse) {
        const HsCommData::EllipseP* el = g.geom.ellipseAt(0);
        if (!el) {
            return QString();
        }
        const std::string s = HsCommData::formatEllipse(el->ra, el->rb, cal);
        return QString::fromUtf8(s.c_str());
    }
    return QString();
}

// 叠加文字：始终在设备坐标画，锚点可来自图像（先 map）或窗口；带 1px 黑色描边。
// 不随视口变换拉伸字形，避免高倍糊、低倍看不见；完全在视口外的行直接跳过。
static void paintDeviceText(QPainter* painter, const QPointF& devicePos, const QString& text,
                            int fontPx, const QColor& color, const QSize& viewSize)
{
    if (!painter || text.isEmpty()) {
        return;
    }
    const int px = qMax(1, fontPx);
    if (devicePos.x() < -px * 32 || devicePos.y() < -px * 4
        || devicePos.x() > viewSize.width() + px * 4
        || devicePos.y() > viewSize.height() + px * 4) {
        return;
    }

    QFont f = painter->font();
    f.setPixelSize(px);
    painter->setFont(f);
    const QFontMetrics fm(f);
    const int lineH = fm.lineSpacing();

    int lineStart = 0;
    qreal y = devicePos.y();
    while (lineStart <= text.size()) {
        const int nl = text.indexOf(QLatin1Char('\n'), lineStart);
        const int lineLen = (nl < 0) ? (text.size() - lineStart) : (nl - lineStart);
        const QString line = text.mid(lineStart, lineLen);
        const QPointF base(devicePos.x(), y + fm.ascent());
        if (y < viewSize.height() + lineH && y + lineH > 0) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    painter->setPen(QColor(0, 0, 0, 220));
                    painter->drawText(base + QPointF(dx, dy), line);
                }
            }
            painter->setPen(color);
            painter->drawText(base, line);
        }
        if (nl < 0) {
            break;
        }
        y += lineH;
        lineStart = nl + 1;
    }
}


// 非单通道图按彩色检验，显示 RGB
static bool inspectIsColor(const HsCommData::HsImage& frame)
{
    return frame.encoding != HsCommData::EncodeMono;
}

// 橡皮筋顶点转为协议 Point2d
static std::vector<HsCommData::Point2d> toPts(const QVector<QPointF>& v)
{
    std::vector<HsCommData::Point2d> o;
    o.reserve(static_cast<size_t>(v.size()));
    for (int i = 0; i < v.size(); ++i) {
        o.push_back(fromQ(v[i]));
    }
    return o;
}

// 格子文字色：亮底深字、暗底浅字（按 BT.601 亮度判断）
static QColor inspectTextColor(int r, int g, int b)
{
    const int y = (r * 299 + g * 587 + b * 114) / 1000;
    return (y > 160) ? QColor(20, 20, 20) : QColor(245, 245, 245);
}


// 平移整条 HsCommData::Graphic，含 HsCommData::HsRegion 的弦；最后重算包围框
static void translateGraphic(HsCommData::Graphic* g, const QPointF& delta)
{
    if (!g || (qFuzzyIsNull(delta.x()) && qFuzzyIsNull(delta.y()))) {
        return;
    }
    const HsCommData::Point2d d = fromQ(delta);
    // 几何对象按种类平移；区域只能按取整后的像素偏移平移弦与包围框
    switch (g->kind) {
    case HsCommData::KindObject:
        switch (g->geom.kind()) {
        case HsCommData::ObjCircle:
            g->geom.circleAt(0)->c += d;
            break;
        case HsCommData::ObjEllipse:
            g->geom.ellipseAt(0)->c += d;
            break;
        case HsCommData::ObjRect1:
            translate(g->geom.rect1At(0)->r, d.x, d.y);
            break;
        case HsCommData::ObjRect2:
            g->geom.rect2At(0)->c += d;
            break;
        case HsCommData::ObjLine:
            g->geom.lineAt(0)->a += d;
            g->geom.lineAt(0)->b += d;
            break;
        case HsCommData::ObjContour:
            for (size_t i = 0; i < g->geom.contourAt(0)->ptCount(); ++i) {
                g->geom.contourAt(0)->ptAt(i) += d;
            }
            break;
        case HsCommData::ObjRegion: {
            const int32_t dx = static_cast<int32_t>(qRound(delta.x()));
            const int32_t dy = static_cast<int32_t>(qRound(delta.y()));
            for (size_t i = 0; i < g->geom.regionAt(0)->runCount(); ++i) {
                g->geom.regionAt(0)->runAt(i).row += dy;
                g->geom.regionAt(0)->runAt(i).cb += dx;
                g->geom.regionAt(0)->runAt(i).ce += dx;
            }
            translate(g->geom.regionAt(0)->bounds, dx, dy);
            break;
        }
        default:
            break;
        }
        break;
    case HsCommData::KindArrow:
        for (size_t i = 0; i < g->arrow.size(); ++i) {
            g->arrow[i].tail += d;
            g->arrow[i].head += d;
        }
        break;
    case HsCommData::KindCross:
        for (size_t i = 0; i < g->cross.size(); ++i) {
            g->cross[i].center += d;
        }
        break;
    case HsCommData::KindPoint:
        for (size_t i = 0; i < g->point.size(); ++i) {
            g->point[i] += d;
        }
        break;
    case HsCommData::KindText:
    case HsCommData::KindTextBadge:
        for (size_t i = 0; i < g->text.size(); ++i) {
            g->text[i].anchor += d;
        }
        break;
    case HsCommData::KindKeypoints:
    case HsCommData::KindSkeleton:
        for (size_t i = 0; i < g->geom.contourAt(0)->ptCount(); ++i) {
            g->geom.contourAt(0)->ptAt(i) += d;
        }
        break;
    default:
        break;
    }
    g->updateBounds();
}

// 拖手柄改几何，pos 为图像坐标；最后重算包围框
static void applyHandleDrag(HsCommData::Graphic* g, int handle, const QPointF& pos)
{
    if (!g) {
        return;
    }
    // 几何对象：圆改半径；椭圆、旋转矩形按局部轴改半轴，末位手柄改转角；矩形改角点或边
    if (g->kind == HsCommData::KindObject && g->geom.isInitialized()) {
        switch (g->geom.kind()) {
        case HsCommData::ObjCircle:
            g->geom.circleAt(0)->r = qMax(1.0, QLineF(toQ(g->geom.circleAt(0)->c), pos).length());
            break;
        case HsCommData::ObjEllipse: {
            const qreal c = qCos(g->geom.ellipseAt(0)->phi);
            const qreal s = qSin(g->geom.ellipseAt(0)->phi);
            const QPointF ax(c, s);
            const QPointF ay(-s, c);
            const QPointF d = pos - toQ(g->geom.ellipseAt(0)->c);
            if (handle == 0 || handle == 1) {
                g->geom.ellipseAt(0)->ra = qMax(1.0, qAbs(QPointF::dotProduct(d, ax)));
            } else if (handle == 2 || handle == 3) {
                g->geom.ellipseAt(0)->rb = qMax(1.0, qAbs(QPointF::dotProduct(d, ay)));
            } else if (handle == 4) {
                g->geom.ellipseAt(0)->setAngRad(qAtan2(d.y(), d.x()));
            }
            break;
        }
        case HsCommData::ObjRect1: {
            QRectF r = toQ(g->geom.rect1At(0)->r).normalized();
            if (handle == 0) {
                r.setTopLeft(pos);
            } else if (handle == 1) {
                r.setTopRight(pos);
            } else if (handle == 2) {
                r.setBottomRight(pos);
            } else if (handle == 3) {
                r.setBottomLeft(pos);
            } else if (handle == 4) {
                r.setTop(pos.y());
            } else if (handle == 5) {
                r.setRight(pos.x());
            } else if (handle == 6) {
                r.setBottom(pos.y());
            } else if (handle == 7) {
                r.setLeft(pos.x());
            }
            g->geom.rect1At(0)->r = fromQ(r.normalized());
            break;
        }
        case HsCommData::ObjRect2: {
            const qreal c = qCos(g->geom.rect2At(0)->phi);
            const qreal s = qSin(g->geom.rect2At(0)->phi);
            const QPointF ax(c, s);
            const QPointF ay(-s, c);
            const QPointF d = pos - toQ(g->geom.rect2At(0)->c);
            if (handle >= 0 && handle <= 3) {
                g->geom.rect2At(0)->l1 = qMax(1.0, qAbs(QPointF::dotProduct(d, ax)));
                g->geom.rect2At(0)->l2 = qMax(1.0, qAbs(QPointF::dotProduct(d, ay)));
            } else if (handle == 4 || handle == 5) {
                g->geom.rect2At(0)->l1 = qMax(1.0, qAbs(QPointF::dotProduct(d, ax)));
            } else if (handle == 6 || handle == 7) {
                g->geom.rect2At(0)->l2 = qMax(1.0, qAbs(QPointF::dotProduct(d, ay)));
            } else if (handle == 8) {
                g->geom.rect2At(0)->setAngRad(qAtan2(d.y(), d.x()));
            }
            break;
        }
        case HsCommData::ObjLine:
            if (handle == 0) {
                g->geom.lineAt(0)->a = fromQ(pos);
            } else {
                g->geom.lineAt(0)->b = fromQ(pos);
            }
            break;
        case HsCommData::ObjContour:
            if (handle >= 0 && handle < static_cast<int>(g->geom.contourAt(0)->ptCount())) {
                g->geom.contourAt(0)->ptAt(static_cast<size_t>(handle)) = fromQ(pos);
            }
            break;
        default:
            break;
        }
    } else {
    // 非几何图元：箭头两端；十字 0 号拖中心、1..4 改半宽、5 起改转角；点和文本只移锚点
    switch (g->kind) {
    case HsCommData::KindArrow:
        if (g->arrow.empty()) {
            break;
        }
        if (handle == 0) {
            g->arrow[0].tail = fromQ(pos);
        } else {
            g->arrow[0].head = fromQ(pos);
        }
        break;
    case HsCommData::KindCross:
        if (g->cross.empty()) {
            break;
        }
        if (handle <= 0) {
            g->cross[0].center = fromQ(pos);
        } else if (handle >= 5) {
            const QPointF d = pos - toQ(g->cross[0].center);
            g->cross[0].setAngRad(qAtan2(d.y(), d.x()));
        } else {
            g->cross[0].size = qMax(1.0, QLineF(toQ(g->cross[0].center), pos).length());
        }
        break;
    case HsCommData::KindPoint:
        if (!g->point.empty()) {
            g->point[0] = fromQ(pos);
        }
        break;
    case HsCommData::KindText:
        if (!g->text.empty()) {
            g->text[0].anchor = fromQ(pos);
        }
        break;
    default:
        break;
    }
    }
    g->updateBounds();
}

class HsImageWindow::Private {
public:
    HsImageWindow* q; ///< 所属窗口
    QGraphicsScene scene; ///< 场景，坐标与图像像素 1:1
    ImageLayer imageLayer;   ///< 底图层
    StackLayer stackObj;     ///< 结果叠加层（Stack）
    DrawingLayer drawingObj; ///< 用户 ROI 层（Drawing）
    MeasureLayer measureObj; ///< 测量层
    ForegroundLayer foreObj; ///< 前景：HUD 开关、信息条与叠加文字
    ImageItem* imageItem; ///< 底图 Item，归 imageLayer 所有
    LayerItem* stackLayer; ///< Stack 层父节点
    LayerItem* drawingLayer; ///< Drawing 层父节点
    LayerItem* measureLayer; ///< 测量层父节点
    QHash<QString, GraphicItem*>& stackItems; ///< Stack 图元表，按 id 索引
    QHash<QString, GraphicItem*>& drawingItems; ///< Drawing 图元表，按 id 索引
    QHash<QString, GraphicItem*>& measureItems; ///< 测量图元表，按 id 索引
    HsCommData::HsImage& frame; ///< 当前底图，取样与 HUD 读它
    std::shared_ptr<const HsCommData::FrameOverlay>& overlay; ///< Stack 协议快照
    Viewport vp; ///< 视口：可见图像矩形与缩放
    WindowMode mode; ///< 设计态 / 运行态
    DrawTool tool; ///< 当前绘制工具
    InteractionState state; ///< 交互状态机

    QString activeId; ///< 选中或正在拖出的图元 id
    int activeHandle; ///< 拖动的手柄下标；kHandleBody 表示拖本体，-1 表示无
    QPoint lastViewPos; ///< 上一次鼠标视口坐标，平移用
    QPointF pressImage; ///< 按下时的图像坐标
    HsCommData::Graphic pressGraphic; ///< 开始拖动时的图元快照，拖动以它为基准
    bool rubberActive; ///< 正在拖出新 ROI（橡皮筋）
    QVector<QPointF> polyPts; ///< 折线 / 多边形已落下的顶点
    bool skipNextContextMenu; ///< 右键已用于结束形状，下一次不弹菜单

    bool& hudVisible; ///< HUD 开关
    bool pixelInspect; ///< 像素检验开关
    bool live; ///< 实时标记，HUD 底栏显示 Live
    bool& stackVisible; ///< Stack 层开关
    bool& drawingVisible; ///< Drawing 层开关
    HsCommData::GeomStyle& penStyle; ///< 新绘 ROI 的画笔
    HsCommData::GeomStyle& overlayStyle; ///< disp* 追加到 Stack 时的样式
    QString textDraft; ///< 文本工具要写下的字符串
    DrawInfoSpace textSpace; ///< 文本工具锚点坐标系
    int textFontPx; ///< 文本工具字号（屏幕像素）
    double crossSize; ///< 新十字半宽（图像像素），小于 0 表示穿窗
    QPointF cursorPt; ///< 光标图像坐标
    QPoint cursorViewPos; ///< 光标视口坐标
    double cursorGray; ///< 光标处灰度
    bool cursorValid; ///< 光标处取样是否有效
    QVector<DrawInfoEntry>& imageDrawInfos; ///< 图像坐标叠加文字
    QVector<DrawInfoEntry>& windowDrawInfos; ///< 窗口坐标叠加文字
    QString& imageInfoText; ///< 信息条文本，画在 HUD 下方
    QLabel* pixelPopup;    ///< 按住 Ctrl 时光标旁的像素信息小窗
    /** @brief Ctrl 小窗深度探针的数据类型 */
    enum DepthProbeKind {
        DepthProbeNone = 0, ///< 无深度探针，小窗显示灰度 / RGB
        DepthProbeU16,  ///< 16 位深度，单位毫米
        DepthProbeF32,  ///< 32 位浮点深度，单位米
        DepthProbeF64   ///< 64 位浮点深度，单位米
    };
    DepthProbeKind depthProbeKind; ///< Ctrl 小窗当前使用的深度探针类型
    int depthProbeW;               ///< 探针宽（像素）
    int depthProbeH;               ///< 探针高（像素）
    std::vector<uint16_t> depthProbeU16; ///< 16 位深度，按行紧排
    std::vector<float> depthProbeF32;    ///< 32 位浮点深度，按行紧排
    std::vector<double> depthProbeF64;   ///< 64 位浮点深度，按行紧排
    QImage depthProbeGray; ///< 归一化灰度，供小窗「灰度显示」一行；可空
    QWidget* overlayBar;           ///< 浮动视图工具条，截屏时不包含
    QBoxLayout* overlayBarLayout;  ///< 视图工具条布局
    Qt::Corner overlayBarCorner;   ///< 工具条停靠角
    Qt::Orientation overlayBarOrient; ///< 按钮排列方向
    OverlayToolbarSkin overlayBarSkin; ///< 工具条皮肤
    std::vector<DrawTool> measureShown; ///< 测量工具条上要显示的工具
    QButtonGroup* measureGroup;         ///< 测量按钮互斥组
    QWidget* measureBar;                ///< 测量工具条
    QBoxLayout* measureBarLayout;       ///< 测量工具条布局
    bool measureBarWant;                ///< 调用方要求显示测量工具条
    ViewportBackground vpBg;       ///< 视口背景风格
    QColor vpBgCustom;             ///< Custom / 棋盘格指定色；无效表示用风格默认
    ImageRefreshPolicy imageRefresh; ///< 换底图时是否清 Stack / ROI / 文字

    QElapsedTimer fpsClock; ///< Overlay fps 计时
    int fpsCount; ///< 本统计周期内的刷新次数
    qreal fps; ///< Overlay Hz
    HsCommData::ImageCalib imageCalib; ///< 像素比例；未设置的单位按 1:1
    int idSeq; ///< ROI id 自增序号

    bool sessionActive; ///< 绘制会话进行中
    bool sessionDraft; ///< 会话里已拖出草稿形状，还可调整，等右键或 Enter 提交
    bool crosshairOn; ///< 会话中光标进过窗口，前景画穿窗十字线
    double brushRadius; ///< 画笔半径（图像像素），一位小数
    int brushW; ///< 画笔掩码宽，等于底图宽
    int brushH; ///< 画笔掩码高，等于底图高
    qint64 brushCount; ///< 已涂像素数，0 表示没有涂
    bool brushStroke; ///< 正在涂一笔
    bool brushErase; ///< 当前这一笔是擦除
    bool brushRightArm; ///< 画笔会话中右键已按下：拖动超过阈值改为擦除，原地松开则提交
    QPoint brushRightView; ///< 右键按下时的视口坐标
    QPoint brushLastPx; ///< 上一次落笔的图像像素
    QPointF brushRingPt; ///< 上一次画笔圈的位置，用于局部重绘
    std::vector<unsigned char> brushBuf; ///< 画笔掩码，按行紧排，非 0 为已涂
    QImage brushOverlay; ///< 掩码的预乘 ARGB 预览图
    QVector<std::vector<unsigned char> > brushUndo; ///< 撤销栈，最多保留 8 步

    // 绑定各层引用、设缺省值，建场景与 Ctrl 像素小窗
    explicit Private(HsImageWindow* owner)
        : q(owner)
        , imageLayer()
        , stackObj()
        , drawingObj()
        , measureObj()
        , foreObj()
        , imageItem(imageLayer.item())
        , stackLayer(stackObj.node())
        , drawingLayer(drawingObj.node())
        , measureLayer(measureObj.node())
        , stackItems(stackObj.items())
        , drawingItems(drawingObj.items())
        , measureItems(measureObj.items())
        , frame(imageLayer.frame())
        , overlay(stackObj.overlay())
        , mode(WindowDesign)
        , tool(ToolBrowse)
        , state(StateBrowse)
        , activeHandle(-1)
        , rubberActive(false)
        , skipNextContextMenu(false)
        , hudVisible(foreObj.hudVisible())
        , pixelInspect(false)
        , live(false)
        , stackVisible(stackObj.wantVisible())
        , drawingVisible(drawingObj.wantVisible())
        , penStyle(drawingObj.pen())
        , overlayStyle(stackObj.style())
        , imageDrawInfos(foreObj.imageInfos())
        , windowDrawInfos(foreObj.windowInfos())
        , imageInfoText(foreObj.infoText())
        , textDraft(QStringLiteral("Text"))
        , textSpace(DrawInfoImage)
        , textFontPx(14)
        , crossSize(-1.0)
        , cursorGray(0)
        , cursorValid(false)
        , pixelPopup(nullptr)
        , depthProbeKind(DepthProbeNone)
        , depthProbeW(0)
        , depthProbeH(0)
        , overlayBar(nullptr)
        , overlayBarLayout(nullptr)
        , overlayBarCorner(Qt::TopRightCorner)
        , overlayBarOrient(Qt::Vertical)
        , overlayBarSkin(ToolbarSkinDark)
        , measureBar(nullptr)
        , measureBarLayout(nullptr)
        , measureBarWant(false)
        , measureGroup(nullptr)
        , vpBg(ViewportDark)
        , vpBgCustom()
        , imageRefresh()
        , fpsCount(0)
        , fps(0)
        , idSeq(0)
        , sessionActive(false)
        , sessionDraft(false)
        , crosshairOn(false)
        , brushRadius(8.0)
        , brushW(0)
        , brushH(0)
        , brushCount(0)
        , brushStroke(false)
        , brushErase(false)
        , brushRightArm(false)
    {
        imageItem->setZValue(windowLayerZ(LayerImage));
        stackLayer->setZValue(windowLayerZ(LayerStack));
        drawingLayer->setZValue(windowLayerZ(LayerDrawing));
        measureLayer->setZValue(windowLayerZ(LayerMeasure));
        // 超大 sceneRect，避免 QGraphicsView 按 item 包围盒自动居中，打乱 part 平移
        scene.setSceneRect(QRectF(-1.0e7, -1.0e7, 2.0e7, 2.0e7));
        scene.addItem(imageItem);
        scene.addItem(stackLayer);
        scene.addItem(drawingLayer);
        scene.addItem(measureLayer);
        fpsClock.start();
        pixelPopup = new QLabel(q);
        pixelPopup->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        pixelPopup->setAttribute(Qt::WA_ShowWithoutActivating);
        pixelPopup->setStyleSheet(QStringLiteral(
            "QLabel { background-color: #F5F5DC; color: #000000;"
            " border: 1px solid #282828; border-radius: 2px; padding: 4px 6px; }"));
        pixelPopup->hide();
    }

    // 按停靠角与排列方向摆放视图工具条和测量工具条；两条都显示时并排，间隔 gap
    void placeOverlayToolbar()
    {
        const int margin = 8;
        const int gap = 4;
        const QRect area = q->rect();
        const bool right = overlayBarCorner == Qt::TopRightCorner
            || overlayBarCorner == Qt::BottomRightCorner;
        const bool bottom = overlayBarCorner == Qt::BottomLeftCorner
            || overlayBarCorner == Qt::BottomRightCorner;
        const bool horizontal = overlayBarOrient == Qt::Horizontal;
        QSize viewSz;
        QSize measSz;
        if (overlayBar && overlayBar->isVisible() && overlayBarLayout) {
            overlayBarLayout->invalidate();
            overlayBarLayout->activate();
            viewSz = overlayBarLayout->sizeHint();
        }
        if (measureBar && measureBar->isVisible() && measureBarLayout) {
            measureBarLayout->invalidate();
            measureBarLayout->activate();
            measSz = measureBarLayout->sizeHint();
        }
        const bool viewOn = !viewSz.isEmpty();
        const bool measOn = !measSz.isEmpty();
        if (!viewOn && !measOn) {
            return;
        }
        // 横排时两条左右相接，竖排时上下相接；视图条贴着停靠角，测量条排在它里侧
        int vx = margin;
        int vy = margin;
        int mx = margin;
        int my = margin;
        if (horizontal) {
            const int totalW = (viewOn ? viewSz.width() : 0)
                + (measOn ? measSz.width() : 0)
                + ((viewOn && measOn) ? gap : 0);
            const int h = qMax(viewOn ? viewSz.height() : 0, measOn ? measSz.height() : 0);
            const int x0 = right ? area.width() - margin - totalW : margin;
            const int y0 = bottom ? area.height() - margin - h : margin;
            if (right) {
                if (viewOn && measOn) {
                    mx = x0;
                    my = y0;
                    vx = x0 + measSz.width() + gap;
                    vy = y0;
                } else if (viewOn) {
                    vx = x0;
                    vy = y0;
                } else {
                    mx = x0;
                    my = y0;
                }
            } else if (viewOn && measOn) {
                vx = x0;
                vy = y0;
                mx = x0 + viewSz.width() + gap;
                my = y0;
            } else if (viewOn) {
                vx = x0;
                vy = y0;
            } else {
                mx = x0;
                my = y0;
            }
        } else {
            const int totalH = (viewOn ? viewSz.height() : 0)
                + (measOn ? measSz.height() : 0)
                + ((viewOn && measOn) ? gap : 0);
            const int w = qMax(viewOn ? viewSz.width() : 0, measOn ? measSz.width() : 0);
            const int x0 = right ? area.width() - margin - w : margin;
            const int y0 = bottom ? area.height() - margin - totalH : margin;
            if (bottom) {
                if (viewOn && measOn) {
                    mx = x0;
                    my = y0;
                    vx = x0;
                    vy = y0 + measSz.height() + gap;
                } else if (viewOn) {
                    vx = x0;
                    vy = y0;
                } else {
                    mx = x0;
                    my = y0;
                }
            } else if (viewOn && measOn) {
                vx = x0;
                vy = y0;
                mx = x0;
                my = y0 + viewSz.height() + gap;
            } else if (viewOn) {
                vx = x0;
                vy = y0;
            } else {
                mx = x0;
                my = y0;
            }
        }
        if (viewOn) {
            overlayBar->setGeometry(vx, vy, viewSz.width(), viewSz.height());
            overlayBar->raise();
        }
        if (measOn) {
            measureBar->setGeometry(mx, my, measSz.width(), measSz.height());
            measureBar->raise();
        }
    }

    // 弹窗选路径保存视口快照（含叠加），默认放在图片目录、按时间命名
    void promptSaveSnapshot()
    {
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const QString dir = pictures.isEmpty() ? QDir::homePath() : pictures;
        const QString selected = QFileDialog::getSaveFileName(
            q,
            QString::fromUtf8("保存快照"),
            QDir(dir).filePath(QStringLiteral("snapshot_%1.png").arg(stamp)),
            QStringLiteral("PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp);;All files (*)"));
        if (selected.isEmpty()) {
            return;
        }
        QString err;
        if (!q->saveSnapshot(selected, &err)) {
            QMessageBox::warning(q, QString::fromUtf8("保存失败"), err);
        }
    }

    // 弹窗选路径导出原图像素（不含叠加）
    void promptSaveOriginal()
    {
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const QString dir = pictures.isEmpty() ? QDir::homePath() : pictures;
        const QString selected = QFileDialog::getSaveFileName(
            q,
            QString::fromUtf8("导出原图"),
            QDir(dir).filePath(QStringLiteral("image_%1.png").arg(stamp)),
            QStringLiteral("PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp);;All files (*)"));
        if (selected.isEmpty()) {
            return;
        }
        QString err;
        if (!q->saveCurrentFrame(selected, &err)) {
            QMessageBox::warning(q, QString::fromUtf8("保存失败"), err);
        }
    }

    // 图标颜色随皮肤：浅色皮肤用深灰，其余用近白
    QColor overlayIconColor() const
    {
        switch (overlayBarSkin) {
        case ToolbarSkinLight:
        case ToolbarSkinLightSolid:
            return QColor(0x33, 0x33, 0x33);
        default:
            return QColor(0xF5, 0xF5, 0xF5);
        }
    }

    // 换皮肤后按新颜色重画两条工具条上的所有按钮图标
    void refreshOverlayIcons()
    {
        const QColor color = overlayIconColor();
        QList<QToolButton*> buttons;
        if (overlayBar) {
            buttons += overlayBar->findChildren<QToolButton*>();
        }
        if (measureBar) {
            buttons += measureBar->findChildren<QToolButton*>();
        }
        for (int i = 0; i < buttons.size(); ++i) {
            const int kind = buttons[i]->property("overlayIcon").toInt();
            if (kind > 0) {
                buttons[i]->setIcon(paintOverlayIcon(kind, color));
            }
        }
    }

    // 创建一个图标按钮并加入指定工具条布局
    QToolButton* makeOverlayBtn(QWidget* bar, QBoxLayout* lay, int kind, const QString& tip)
    {
        QToolButton* b = new QToolButton(bar);
        b->setAutoRaise(true);
        b->setFixedSize(32, 24);
        b->setIconSize(QSize(18, 18));
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setProperty("overlayIcon", kind);
        b->setIcon(paintOverlayIcon(kind, overlayIconColor()));
        b->setToolTip(tip);
        lay->addWidget(b);
        return b;
    }

    // 让测量按钮的勾选状态跟随当前工具；先关互斥才能全部取消勾选
    void syncMeasureChecks()
    {
        if (!measureGroup) {
            return;
        }
        measureGroup->setExclusive(false);
        const QList<QAbstractButton*> btns = measureGroup->buttons();
        for (int i = 0; i < btns.size(); ++i) {
            const int id = btns[i]->property("measureTool").toInt();
            btns[i]->setChecked(id == static_cast<int>(tool));
        }
        measureGroup->setExclusive(true);
    }

    // 按 measureShown 重建测量按钮：删掉旧按钮，非测量工具跳过
    void rebuildMeasureButtons()
    {
        if (!measureBar || !measureBarLayout) {
            return;
        }
        if (!measureGroup) {
            measureGroup = new QButtonGroup(measureBar);
            measureGroup->setExclusive(true);
        }
        const QList<QAbstractButton*> old = measureGroup->buttons();
        for (int i = 0; i < old.size(); ++i) {
            measureGroup->removeButton(old[i]);
            measureBarLayout->removeWidget(old[i]);
            delete old[i];
        }
        if (measureShown.empty()) {
            syncMeasureBar();
            return;
        }
        // 每个按钮记下对应工具，点击即切换到该测量工具
        for (size_t i = 0; i < measureShown.size(); ++i) {
            const DrawTool item = measureShown[i];
            int kind = 0;
            QString tip;
            switch (item) {
            case ToolMeasureDist:
                kind = IconDist;
                tip = QString::fromUtf8("两点距离");
                break;
            case ToolMeasureAngle:
                kind = IconAngle;
                tip = QString::fromUtf8("三点夹角");
                break;
            case ToolMeasureRadius:
                kind = IconRadius;
                tip = QString::fromUtf8("圆半径");
                break;
            case ToolMeasureRect:
                kind = IconRect;
                tip = QString::fromUtf8("矩形宽高");
                break;
            case ToolMeasureEllipse:
                kind = IconEllipse;
                tip = QString::fromUtf8("椭圆半轴");
                break;
            default:
                break;
            }
            if (kind == 0) {
                continue;
            }
            QToolButton* b = makeOverlayBtn(measureBar, measureBarLayout, kind, tip);
            b->setCheckable(true);
            b->setProperty("measureTool", static_cast<int>(item));
            measureGroup->addButton(b);
            QObject::connect(b, &QToolButton::clicked, q, [this, item]() {
                q->setDrawTool(item);
            });
        }
        syncMeasureChecks();
        syncMeasureBar();
    }

    // 测量工具条只在调用方要求且有按钮时显示，然后重新摆放
    void syncMeasureBar()
    {
        if (!measureBar) {
            return;
        }
        measureBar->setVisible(measureBarWant && !measureShown.empty());
        placeOverlayToolbar();
    }

    // 建视图工具条与测量工具条（快照、缩放、适应、1:1 及测量按钮）
    void buildOverlayToolbar()
    {
        overlayBar = new QWidget(q);
        overlayBar->setObjectName(QStringLiteral("hsImageOverlayToolbar"));
        overlayBarLayout = new QBoxLayout(QBoxLayout::TopToBottom, overlayBar);
        overlayBarLayout->setContentsMargins(3, 2, 3, 2);
        overlayBarLayout->setSpacing(2);
        overlayBarLayout->setSizeConstraint(QLayout::SetFixedSize);
        measureBar = new QWidget(q);
        measureBar->setObjectName(QStringLiteral("hsImageMeasureToolbar"));
        measureBarLayout = new QBoxLayout(QBoxLayout::TopToBottom, measureBar);
        measureBarLayout->setContentsMargins(3, 2, 3, 2);
        measureBarLayout->setSpacing(2);
        measureBarLayout->setSizeConstraint(QLayout::SetFixedSize);
        measureBar->hide();
        applyOverlayToolbarSkin();
        QToolButton* snap = makeOverlayBtn(overlayBar, overlayBarLayout, IconSnap, QString::fromUtf8("保存快照（含图与叠加）"));
        QToolButton* zin = makeOverlayBtn(overlayBar, overlayBarLayout, IconZoomIn, QString::fromUtf8("放大"));
        QToolButton* zout = makeOverlayBtn(overlayBar, overlayBarLayout, IconZoomOut, QString::fromUtf8("缩小"));
        QToolButton* fit = makeOverlayBtn(overlayBar, overlayBarLayout, IconFit, QString::fromUtf8("等比例适应窗口"));
        QToolButton* one = makeOverlayBtn(overlayBar, overlayBarLayout, IconOne, QString::fromUtf8("实际像素"));
        QObject::connect(snap, &QToolButton::clicked, q, [this]() { promptSaveSnapshot(); });
        QObject::connect(zin, &QToolButton::clicked, q, [this]() { q->zoomIn(); });
        QObject::connect(zout, &QToolButton::clicked, q, [this]() { q->zoomOut(); });
        QObject::connect(fit, &QToolButton::clicked, q, [this]() { q->fitToWindow(); });
        QObject::connect(one, &QToolButton::clicked, q, [this]() { q->resetView(); });
        rebuildMeasureButtons();
        // 默认隐藏，需要时调用方 setOverlayToolbarVisible(true)
        overlayBar->hide();
        placeOverlayToolbar();
    }

    // 按皮肤写工具条样式表（不读主题），测量条套用同一套样式，再重画图标
    void applyOverlayToolbarSkin()
    {
        if (!overlayBar) {
            return;
        }
        QString css;
        switch (overlayBarSkin) {
        case ToolbarSkinTransparent:
            css = QStringLiteral(
                "QWidget#hsImageOverlayToolbar { background-color: rgba(0,0,0,0); border-radius: 0px; }"
                "QWidget#hsImageOverlayToolbar QToolButton { color: #F5F5F5; }");
            break;
        case ToolbarSkinLight:
            css = QStringLiteral(
                "QWidget#hsImageOverlayToolbar {"
                "  background-color: rgba(255,255,255,230);"
                "  border: 1px solid rgba(200,200,200,220); border-radius: 3px; }"
                "QWidget#hsImageOverlayToolbar QToolButton { color: #333333; }");
            break;
        case ToolbarSkinDarkSolid:
            css = QStringLiteral(
                "QWidget#hsImageOverlayToolbar {"
                "  background-color: rgb(28,28,28); border-radius: 3px; }"
                "QWidget#hsImageOverlayToolbar QToolButton { color: #F5F5F5; }");
            break;
        case ToolbarSkinLightSolid:
            css = QStringLiteral(
                "QWidget#hsImageOverlayToolbar {"
                "  background-color: rgb(245,245,245);"
                "  border: 1px solid #D0D0D0; border-radius: 3px; }"
                "QWidget#hsImageOverlayToolbar QToolButton { color: #333333; }");
            break;
        case ToolbarSkinDark:
        default:
            css = QStringLiteral(
                "QWidget#hsImageOverlayToolbar {"
                "  background-color: rgba(20,20,20,150); border-radius: 3px; }"
                "QWidget#hsImageOverlayToolbar QToolButton { color: #F5F5F5; }");
            break;
        }
        overlayBar->setStyleSheet(css);
        if (measureBar) {
            QString measureCss = css;
            measureCss.replace(QLatin1String("#hsImageOverlayToolbar"),
                               QLatin1String("#hsImageMeasureToolbar"));
            measureBar->setStyleSheet(measureCss);
        }
        refreshOverlayIcons();
    }

    // 纯色模式的填充色；棋盘格返回底格颜色，指定色优先
    QColor resolvedViewportColor() const
    {
        if (vpBg == ViewportCustom && vpBgCustom.isValid()) {
            return vpBgCustom;
        }
        if (vpBg == ViewportCheckerboard) {
            return vpBgCustom.isValid() ? vpBgCustom : QColor(0xf5, 0xf5, 0xf5);
        }
        if (vpBg == ViewportCheckerboardDark) {
            return vpBgCustom.isValid() ? vpBgCustom : QColor(0x2a, 0x2a, 0x2a);
        }
        if (vpBg == ViewportLight) {
            return QColor(0xf2, 0xf3, 0xf5);
        }
        if (vpBg == ViewportMid) {
            return QColor(0x5c, 0x5c, 0x5c);
        }
        return QColor(0x12, 0x12, 0x12);
    }

    // 在屏幕坐标画视口背景；棋盘格固定 12px 一格，不随缩放变化
    void paintViewportBackground(QPainter* painter)
    {
        const QRect vr = q->viewport()->rect();
        if (vpBg == ViewportCheckerboard || vpBg == ViewportCheckerboardDark) {
            const QColor c1 = resolvedViewportColor();
            const QColor c2 = (vpBg == ViewportCheckerboardDark)
                ? QColor(0x3d, 0x3d, 0x3d)
                : QColor(0xdb, 0xdb, 0xdb);
            const int cell = 12;
            painter->fillRect(vr, c1);
            const int x0 = vr.left();
            const int y0 = vr.top();
            for (int y = 0; y < vr.height(); y += cell) {
                for (int x = 0; x < vr.width(); x += cell) {
                    if (((x / cell) + (y / cell)) % 2 != 0) {
                        painter->fillRect(x0 + x, y0 + y, cell, cell, c2);
                    }
                }
            }
            return;
        }
        painter->fillRect(vr, resolvedViewportColor());
    }

    // 换底图；尺寸变化或视口无效时重新 fit
    void setFrame(const HsCommData::HsImage& fb)
    {
        const QSize oldSize = bufferPixelSize(frame);
        frame = fb;
        imageItem->setFrame(fb);
        vp.imageSize = bufferPixelSize(fb);
        if (oldSize != bufferPixelSize(fb) || !vp.isValid()) {
            vp.fit(viewSize());
            applyTransform();
        }
        noteFrame();
        syncStackVisibility();
        q->viewport()->update();
    }

    // 直接贴已备好的 QImage，不拷像素、不转 QPixmap；不更新 frame，取样仍读旧帧
    void setPrepared(const QImage& image)
    {
        const QSize oldSize = vp.imageSize;
        imageItem->setImage(image);
        vp.imageSize = image.size();
        if (oldSize != image.size() || !vp.isValid()) {
            vp.fit(viewSize());
            applyTransform();
        }
        noteFrame();
        q->viewport()->update();
    }

    // 换底图成功后按刷新策略清 Stack / 几何 ROI / 文字
    void applyRefreshAfterNewImage()
    {
        if (imageRefresh.refreshStack) {
            overlay.reset();
            rebuildStack();
        }
        pruneDrawings(imageRefresh.refreshDrawing, imageRefresh.refreshText);
        if (imageRefresh.refreshText) {
            imageDrawInfos.clear();
            windowDrawInfos.clear();
        }
        q->viewport()->update();
    }

    /**
     * @brief 按策略删 Drawing。几何 ROI 与 HsCommData::KindText 分开处理，避免清 ROI 时把文本工具写的字也清掉
     * @param dropGeom 删除非文本 ROI
     * @param dropText 删除 HsCommData::KindText
     */
    void pruneDrawings(bool dropGeom, bool dropText)
    {
        if (!dropGeom && !dropText) {
            return;
        }
        if (rubberActive) {
            GraphicItem* item = drawingById(activeId);
            const bool isText = item && item->graphic().kind == HsCommData::KindText;
            if ((isText && dropText) || (!isText && dropGeom)) {
                cancelDraw(false);
            }
        }
        QList<QString> drop;
        QHash<QString, GraphicItem*>::const_iterator it = drawingItems.constBegin();
        for (; it != drawingItems.constEnd(); ++it) {
            const bool isText = it.value()->graphic().kind == HsCommData::KindText;
            if ((isText && dropText) || (!isText && dropGeom)) {
                drop.push_back(it.key());
            }
        }
        const bool lostSel = !activeId.isEmpty() && drop.contains(activeId);
        for (int i = 0; i < drop.size(); ++i) {
            removeDrawing(drop[i]);
        }
        if (lostSel) {
            activeId.clear();
            activeHandle = -1;
            state = StateBrowse;
            q->viewport()->unsetCursor();
            emit q->roiSelected(HsCommData::Graphic());
        }
    }

    // 关掉 Ctrl 像素小窗
    void hideCtrlPixelPopup()
    {
        if (pixelPopup) {
            pixelPopup->hide();
        }
    }

    // 清空深度探针数据，小窗回到灰度 / RGB
    void clearDepthProbeData()
    {
        depthProbeKind = DepthProbeNone;
        depthProbeW = 0;
        depthProbeH = 0;
        depthProbeU16.clear();
        depthProbeF32.clear();
        depthProbeF64.clear();
        depthProbeGray = QImage();
    }

    // Ctrl 按住：在光标旁显示坐标与像素值；有深度探针时显示深度，否则显示灰度或 RGB
    void updateCtrlPixelPopup(const QPoint& viewPos)
    {
        if (!pixelPopup) {
            return;
        }
        QPoint pix;
        if (!q->mapWidgetToImagePixel(viewPos, &pix)) {
            hideCtrlPixelPopup();
            return;
        }

        QString type_label = QString::fromUtf8("灰度");
        QString value_text;
        QString raw_depth_text;
        QString extra_value_text;
        const QString extra_label = QString::fromUtf8("灰度显示");
        const size_t idx = static_cast<size_t>(pix.y()) * static_cast<size_t>(depthProbeW) +
            static_cast<size_t>(pix.x());

        // 深度探针：0 或非有限值视为无效；浮点深度按米存，显示时换成毫米
        if (depthProbeKind == DepthProbeU16 && depthProbeW > 0 && depthProbeH > 0 &&
            pix.x() >= 0 && pix.y() >= 0 && pix.x() < depthProbeW && pix.y() < depthProbeH &&
            idx < depthProbeU16.size())
        {
            const uint16_t depth = depthProbeU16[idx];
            type_label = QString::fromUtf8("深度");
            value_text = depth == 0 ? QString::fromUtf8("无效")
                                   : QString::fromUtf8("%1 mm").arg(depth);
            raw_depth_text = depth == 0 ? QString::fromUtf8("无效") : QString::number(depth);
            const QColor color = depthProbeGray.isNull()
                ? QColor()
                : depthProbeGray.pixelColor(pix);
            extra_value_text = depthProbeGray.isNull()
                ? QString()
                : QString::number(qGray(color.rgb()));
        } else if (depthProbeKind == DepthProbeF32 && depthProbeW > 0 && depthProbeH > 0 &&
                   pix.x() >= 0 && pix.y() >= 0 && pix.x() < depthProbeW &&
                   pix.y() < depthProbeH && idx < depthProbeF32.size())
        {
            const float depth = depthProbeF32[idx];
            type_label = QString::fromUtf8("深度");
            if (!std::isfinite(depth) || depth <= 0.0f) {
                value_text = QString::fromUtf8("无效");
                raw_depth_text = QString::fromUtf8("无效");
            } else {
                value_text = QString::fromUtf8("%1 mm").arg(depth * 1000.0f, 0, 'f', 1);
                raw_depth_text = QString::number(depth, 'f', 4);
            }
            const QColor color = depthProbeGray.isNull()
                ? QColor()
                : depthProbeGray.pixelColor(pix);
            extra_value_text = depthProbeGray.isNull()
                ? QString()
                : QString::number(qGray(color.rgb()));
        } else if (depthProbeKind == DepthProbeF64 && depthProbeW > 0 && depthProbeH > 0 &&
                   pix.x() >= 0 && pix.y() >= 0 && pix.x() < depthProbeW &&
                   pix.y() < depthProbeH && idx < depthProbeF64.size())
        {
            const double depth = depthProbeF64[idx];
            type_label = QString::fromUtf8("深度");
            if (!std::isfinite(depth) || depth <= 0.0) {
                value_text = QString::fromUtf8("无效");
                raw_depth_text = QString::fromUtf8("无效");
            } else {
                value_text = QString::fromUtf8("%1 mm").arg(depth * 1000.0, 0, 'f', 1);
                raw_depth_text = QString::number(depth, 'f', 6);
            }
            const QColor color = depthProbeGray.isNull()
                ? QColor()
                : depthProbeGray.pixelColor(pix);
            extra_value_text = depthProbeGray.isNull()
                ? QString()
                : QString::number(qGray(color.rgb()));
        }

        // 没有深度读数时退回底图像素值
        QString text;
        if (value_text.isEmpty()) {
            if (inspectIsColor(frame)) {
                int r = 0;
                int g = 0;
                int b = 0;
                if (!frame.sampleRgb(pix.x(), pix.y(), &r, &g, &b)) {
                    hideCtrlPixelPopup();
                    return;
                }
                text = QString::fromUtf8("X:%1  Y:%2\n(RGB): R:%3 G:%4 B:%5")
                           .arg(pix.x()).arg(pix.y()).arg(r).arg(g).arg(b);
            } else {
                double gray = 0;
                if (!frame.sampleGray(pix.x(), pix.y(), &gray)) {
                    hideCtrlPixelPopup();
                    return;
                }
                text = QString::fromUtf8("X:%1  Y:%2\n(灰度): %3")
                           .arg(pix.x()).arg(pix.y()).arg(qRound(gray));
            }
        } else {
            text = QString::fromUtf8("X:%1  Y:%2\n值(%3): %4")
                       .arg(pix.x()).arg(pix.y()).arg(type_label).arg(value_text);
            if (!raw_depth_text.isEmpty()) {
                text += QString::fromUtf8("\n值(深度原始): %1").arg(raw_depth_text);
            }
            if (!extra_value_text.isEmpty()) {
                text += QString::fromUtf8("\n值(%1): %2").arg(extra_label).arg(extra_value_text);
            }
        }
        pixelPopup->setText(text);
        pixelPopup->adjustSize();
        pixelPopup->move(q->mapToGlobal(viewPos) + QPoint(16, 0));
        pixelPopup->show();
    }

    // 视口客户区尺寸
    QSize viewSize() const
    {
        return q->viewport()->size();
    }

    // 把 part 写成 QTransform，并同步各层缩放、发 viewportChanged
    void applyTransform()
    {
        q->setTransform(vp.viewTransform(viewSize()));
        // 滚动条归零，避免 View 的滚动偏移与 part 叠成双重平移，ROI 落笔偏离图像坐标
        if (QScrollBar* h = q->horizontalScrollBar()) {
            h->blockSignals(true);
            h->setValue(0);
            h->blockSignals(false);
        }
        if (QScrollBar* v = q->verticalScrollBar()) {
            v->blockSignals(true);
            v->setValue(0);
            v->blockSignals(false);
        }
        const qreal s = vp.scale(viewSize());
        imageItem->setViewScale(s);
        QHash<QString, GraphicItem*>::iterator it = drawingItems.begin();
        for (; it != drawingItems.end(); ++it) {
            it.value()->setViewScale(s);
        }
        it = stackItems.begin();
        for (; it != stackItems.end(); ++it) {
            it.value()->setViewScale(s);
        }
        it = measureItems.begin();
        for (; it != measureItems.end(); ++it) {
            it.value()->setViewScale(s);
        }
        emit q->viewportChanged(vp.part);
    }

    // 以视口中心为锚缩放，倍率与滚轮相同
    void zoomBy(qreal factor)
    {
        if (!vp.isValid()) {
            return;
        }
        const QSize vs = viewSize();
        if (vs.isEmpty()) {
            return;
        }
        const QPointF center(vs.width() * 0.5, vs.height() * 0.5);
        const QPointF img = q->widgetToImage(center);
        const QRectF p = vp.part;
        if (p.width() <= 1e-6 || p.height() <= 1e-6) {
            return;
        }
        const QPointF rel((img.x() - p.x()) / p.width(), (img.y() - p.y()) / p.height());
        vp.zoomAt(img, rel, factor, vs);
        applyTransform();
        q->viewport()->update();
    }

    // 累计 Overlay 刷新次数，满 1s 更新 fps
    void noteFrame()
    {
        ++fpsCount;
        const qint64 ms = fpsClock.elapsed();
        if (ms >= 1000) {
            fps = fpsCount * 1000.0 / static_cast<qreal>(qMax(ms, static_cast<qint64>(1)));
            fpsCount = 0;
            fpsClock.restart();
        }
    }

    // frameId 与底图不一致则隐藏 Stack
    void syncStackVisibility()
    {
        stackObj.syncVisibility(frame);
    }

    // 生成递增 ROI id：前缀.序号
    QString nextId(const QString& prefix)
    {
        ++idSeq;
        return prefix + QLatin1Char('.') + QString::number(idSeq);
    }

    // 按 id 判断图元归哪一层：测量 id 或测量工具下的预览归测量层，其余归 Drawing
    WindowLayer layerOfId(const QString& id) const
    {
        if (measureItemId(id) || (id.startsWith(QLatin1String("preview")) && measureTool(tool))) {
            return LayerMeasure;
        }
        return LayerDrawing;
    }

    // 设置某一层父节点的显隐；前景层不在场景里，忽略
    void setLayerVisible(WindowLayer layer, bool visible)
    {
        switch (layer) {
        case LayerImage:
            imageItem->setVisible(visible);
            break;
        case LayerStack:
            stackLayer->setVisible(visible);
            break;
        case LayerDrawing:
            drawingLayer->setVisible(visible);
            break;
        case LayerMeasure:
            measureLayer->setVisible(visible);
            break;
        case LayerForeground:
            break;
        }
    }

    // 按 id 找可编辑图元，先查测量层再查 Drawing；找不到返回 0
    GraphicItem* drawingById(const QString& id) const
    {
        GraphicItem* item = measureItems.value(id, 0);
        if (item) {
            return item;
        }
        return drawingItems.value(id, 0);
    }

    // 切换选中并发 roiSelected；橡皮筋进行中不因空 id 清掉 activeId
    void setSelectedDrawing(const QString& id)
    {
        QHash<QString, GraphicItem*>::const_iterator it = drawingItems.constBegin();
        for (; it != drawingItems.constEnd(); ++it) {
            it.value()->setSelectedDrawing(it.key() == id);
        }
        it = measureItems.constBegin();
        for (; it != measureItems.constEnd(); ++it) {
            it.value()->setSelectedDrawing(it.key() == id);
        }
        if (!rubberActive || !id.isEmpty()) {
            activeId = id;
        }
        HsCommData::Graphic g;
        GraphicItem* item = drawingById(activeId);
        if (item && !id.isEmpty()) {
            g = item->graphic();
        }
        emit q->roiSelected(g);
    }

    // 按 id 更新或新建可编辑图元，放进 layerOfId 判定的层并从另一层移除
    GraphicItem* upsertDrawing(const HsCommData::Graphic& g)
    {
        const QString id = toQ(g.id);
        const qreal scale = vp.scale(viewSize());
        if (layerOfId(id) == LayerMeasure) {
            drawingObj.remove(id);
            return measureObj.upsert(g, scale);
        }
        measureObj.remove(id);
        return drawingObj.upsert(g, scale);
    }

    // 从测量层或 Drawing 层删除该 id 的图元
    void removeDrawing(const QString& id)
    {
        if (measureObj.contains(id)) {
            measureObj.remove(id);
            return;
        }
        drawingObj.remove(id);
    }

    // 按 overlay 重建 Stack Item 并同步显隐
    void rebuildStack()
    {
        stackObj.rebuild(vp.scale(viewSize()));
        syncStackVisibility();
    }

    // disp* 追加一条到 Stack：复制旧快照再加，frameId 取当前底图，样式用 overlayStyle
    void appendStack(HsCommData::Graphic g)
    {
        const int64_t fid = frame.isNull() ? 0 : frame.frameId;
        std::shared_ptr<HsCommData::FrameOverlay> sc = HsCommData::FrameOverlay::create(fid);
        if (overlay) {
            sc->items = overlay->items;
        }
        g.role = HsCommData::RoleStack;
        g.frameId = fid;
        g.geomStyle = overlayStyle;
        if (g.id.empty()) {
            g.id = fromQ(nextId(kindPrefix(g)));
        }
        g.updateBounds();
        sc->items.push_back(g);
        overlay = sc;
        rebuildStack();
        noteFrame();
        q->viewport()->update();
    }

    // 命中容差：约 8 屏幕像素折算到图像像素
    qreal imageTol() const
    {
        const qreal s = vp.scale(viewSize());
        return (s > 1e-6) ? (8.0 / s) : 8.0;
    }

    // 当前场景到视口的变换
    QTransform sceneToView() const
    {
        return q->viewportTransform();
    }

    // 命中手柄：先测当前选中，再测测量层；运行态不测 Drawing 层
    bool hitHandle(const QPointF& viewPos, QString* id, int* handle) const
    {
        const qreal hot = 10.0;
        if (!activeId.isEmpty()) {
            GraphicItem* item = drawingById(activeId);
            if (item && !(layerOfId(activeId) == LayerDrawing && mode == WindowRuntime)) {
                const HandleHit h = item->hitHandle(viewPos, sceneToView(), hot);
                if (h.isValid()) {
                    *id = activeId;
                    *handle = h.index;
                    return true;
                }
            }
        }
        QHash<QString, GraphicItem*>::const_iterator it = measureItems.constBegin();
        for (; it != measureItems.constEnd(); ++it) {
            const HandleHit h = it.value()->hitHandle(viewPos, sceneToView(), hot);
            if (h.isValid()) {
                *id = it.key();
                *handle = h.index;
                return true;
            }
        }
        if (mode == WindowRuntime) {
            return false;
        }
        it = drawingItems.constBegin();
        for (; it != drawingItems.constEnd(); ++it) {
            const HandleHit h = it.value()->hitHandle(viewPos, sceneToView(), hot);
            if (h.isValid()) {
                *id = it.key();
                *handle = h.index;
                return true;
            }
        }
        return false;
    }

    // 图像坐标点中可编辑图元：选中项优先，其次测量层，再次 Drawing（运行态跳过），后加的在上
    GraphicItem* hitDrawing(const QPointF& scenePos) const
    {
        GraphicItem* selected = drawingById(activeId);
        if (selected && measureItems.contains(activeId) && selected->hitShape(scenePos, imageTol())) {
            return selected;
        }
        if (mode != WindowRuntime && selected && selected->hitShape(scenePos, imageTol())) {
            return selected;
        }
        QList<GraphicItem*> items = measureItems.values();
        for (int i = items.size() - 1; i >= 0; --i) {
            if (items[i]->hitShape(scenePos, imageTol())) {
                return items[i];
            }
        }
        if (mode == WindowRuntime) {
            return 0;
        }
        items = drawingItems.values();
        for (int i = items.size() - 1; i >= 0; --i) {
            if (items[i]->hitShape(scenePos, imageTol())) {
                return items[i];
            }
        }
        return 0;
    }

    // 命中手柄或本体则进入编辑态，记下拖动前的快照
    bool tryBeginEdit(const QPoint& viewPos, const QPointF& img)
    {
        if (rubberActive || (mode != WindowDesign && mode != WindowRuntime)) {
            return false;
        }
        QString hid;
        int hh = -1;
        if (hitHandle(viewPos, &hid, &hh)) {
            activeId = hid;
            activeHandle = hh;
            GraphicItem* item = drawingById(hid);
            pressGraphic = item ? item->graphic() : HsCommData::Graphic();
            setSelectedDrawing(hid);
            state = StateEditHandle;
            if (item) {
                q->viewport()->setCursor(item->cursorForHandle(hh, sceneToView()));
            }
            return true;
        }
        GraphicItem* drawing = hitDrawing(img);
        if (drawing) {
            const QString id = toQ(drawing->graphic().id);
            setSelectedDrawing(id);
            activeId = id;
            activeHandle = kHandleBody;
            pressGraphic = drawing->graphic();
            state = StateEditHandle;
            q->viewport()->setCursor(Qt::SizeAllCursor);
            return true;
        }
        return false;
    }

    // 悬停时的光标形状：会话中为十字，手柄按方向，本体为移动
    void updateHoverCursor(const QPoint& viewPos, const QPointF& img)
    {
        if (sessionActive) {
            q->viewport()->setCursor(Qt::CrossCursor);
            return;
        }
        if (mode != WindowDesign || rubberActive) {
            q->viewport()->unsetCursor();
            return;
        }
        QString hid;
        int hh = -1;
        if (hitHandle(viewPos, &hid, &hh)) {
            GraphicItem* item = drawingById(hid);
            if (item) {
                q->viewport()->setCursor(item->cursorForHandle(hh, sceneToView()));
            } else {
                q->viewport()->setCursor(Qt::SizeAllCursor);
            }
            return;
        }
        if (hitDrawing(img)) {
            q->viewport()->setCursor(Qt::SizeAllCursor);
            return;
        }
        q->viewport()->unsetCursor();
    }

    // 点中 Stack 图元
    GraphicItem* hitStack(const QPointF& scenePos) const
    {
        return stackObj.hit(scenePos, imageTol());
    }

    // 在落笔点 p0 生成预览图元，尺寸取 1 像素，之后随拖动更新
    HsCommData::Graphic makePreview(DrawTool tool, const QPointF& p0) const
    {
        HsCommData::Graphic g;
        g.kind = toolKind(tool);
        g.role = HsCommData::RoleDrawing;
        g.geomStyle = penStyle;
        g.frameId = frame.frameId;
        g.visible = true;
        // 测量工具与同形状的绘制工具共用几何
        switch (tool) {
        case ToolMeasureRadius:
        case ToolCircle:
            g.geom.circleAt(0)->c = fromQ(p0);
            g.geom.circleAt(0)->r = 1;
            break;
        case ToolMeasureRect:
        case ToolRect1:
            g.geom = HsCommData::Rect1P();
            g.geom.rect1At(0)->r = HsCommData::Box2(p0.x(), p0.y(), 1, 1);
            break;
        case ToolRect2:
            g.geom = HsCommData::Rect2P();
            g.geom.rect2At(0)->c = fromQ(p0);
            g.geom.rect2At(0)->l1 = 1;
            g.geom.rect2At(0)->l2 = 1;
            g.geom.rect2At(0)->phi = 0;
            break;
        case ToolMeasureEllipse:
        case ToolEllipse:
            g.geom = HsCommData::EllipseP();
            g.geom.ellipseAt(0)->c = fromQ(p0);
            g.geom.ellipseAt(0)->ra = 1;
            g.geom.ellipseAt(0)->rb = 1;
            g.geom.ellipseAt(0)->phi = 0;
            break;
        case ToolMeasureDist:
        case ToolLine:
            g.geom = HsCommData::LineP();
            g.geom.lineAt(0)->a = fromQ(p0);
            g.geom.lineAt(0)->b = fromQ(p0);
            break;
        case ToolCross:
            g.cross.resize(1);
            g.cross[0].center = fromQ(p0);
            g.cross[0].size = crossSize;
            break;
        case ToolMeasureAngle:
        case ToolContour:
        case ToolPolygon:
            g.geom = HsCommData::HsContour();
            g.geom.contourAt(0)->pushPt(fromQ(p0));
            g.geom.contourAt(0)->closed = false;
            break;
        case ToolText:
            g.text.resize(1);
            g.text[0].anchor = fromQ(p0);
            g.text[0].content = fromQ(textDraft.isEmpty() ? QStringLiteral("Text") : textDraft);
            g.textStyle.fontPx = textFontPx > 0 ? static_cast<double>(textFontPx) : 14.0;
            g.textStyle.color = penStyle.stroke;
            break;
        default:
            break;
        }
        g.updateBounds();
        return g;
    }

    // 预览是否过小（尺寸不足 2 像素），过小则不提交
    bool finishPreviewIfTiny(const HsCommData::Graphic& g) const
    {
        if (g.kind == HsCommData::KindCross) {
            return !g.cross.empty() && g.cross[0].size >= 0.0 && g.cross[0].size < 2.0;
        }
        if (g.kind != HsCommData::KindObject || !g.geom.isInitialized()) {
            return false;
        }
        switch (g.geom.kind()) {
        case HsCommData::ObjCircle:
            return g.geom.circleAt(0)->r < 2.0;
        case HsCommData::ObjEllipse:
            return g.geom.ellipseAt(0)->ra < 2.0 || g.geom.ellipseAt(0)->rb < 2.0;
        case HsCommData::ObjRect1:
            return HsCommData::width(normalized(g.geom.rect1At(0)->r)) < 2.0 || HsCommData::height(normalized(g.geom.rect1At(0)->r)) < 2.0;
        case HsCommData::ObjRect2:
            return g.geom.rect2At(0)->l1 < 2.0 || g.geom.rect2At(0)->l2 < 2.0;
        case HsCommData::ObjLine:
            return QLineF(toQ(g.geom.lineAt(0)->a), toQ(g.geom.lineAt(0)->b)).length() < 2.0;
        default:
            return false;
        }
    }

    // 补 id、写入 Drawing 并选中，发 roiCommitted；会话中则同时结束会话
    void commitGraphic(HsCommData::Graphic g)
    {
        if (g.id.empty()) {
            g.id = fromQ(nextId(kindPrefix(g)));
        }
        g.role = HsCommData::RoleDrawing;
        if (g.geomStyle.widthPx <= 0) {
            g.geomStyle = penStyle;
        }
        g.frameId = frame.frameId;
        g.updateBounds();
        upsertDrawing(g);
        setSelectedDrawing(toQ(g.id));
        state = StateEditHandle;
        const bool ending = sessionActive;
        if (ending) {
            sessionActive = false;
            sessionDraft = false;
            crosshairOn = false;
            tool = ToolBrowse;
            clearBrushStorage();
            rubberActive = false;
            polyPts.clear();
        }
        emit q->roiCommitted(g);
        if (ending) {
            emit q->drawSessionFinished(g, true);
        }
    }

    // 文本工具单击：图像坐标落成 Drawing 文本，窗口坐标进 drawInfo
    void placeTextTool(const QPoint& widgetPos, const QPointF& imagePt)
    {
        emit q->textAnchorPicked(imagePt, QPointF(widgetPos));
        const QString msg = textDraft.isEmpty() ? QStringLiteral("Text") : textDraft;
        const int px = textFontPx > 0 ? textFontPx : 14;
        if (textSpace == DrawInfoWindow) {
            DrawInfoEntry e;
            e.text = msg;
            e.pos = QPointF(widgetPos);
            e.fontPx = px;
            e.color = penStyle.stroke.isUnset() ? QColor(255, 255, 255) : toQ(penStyle.stroke);
            windowDrawInfos.push_back(e);
            q->viewport()->update();
            return;
        }
        HsCommData::Graphic g;
        g.kind = HsCommData::KindText;
        g.role = HsCommData::RoleDrawing;
        g.textStyle.fontPx = px;
        g.textStyle.color = penStyle.stroke.isUnset() ? HsCommData::Color4::of(HsCommData::ColorWhite) : penStyle.stroke;
        g.text.resize(1);
        g.text[0].anchor = fromQ(imagePt);
        g.text[0].content = fromQ(msg);
        g.frameId = frame.frameId;
        g.visible = true;
        commitGraphic(g);
    }

    // 取消橡皮筋并清选中，可选回到浏览工具
    void cancelDraw(bool resetTool = true)
    {
        if (!activeId.isEmpty() && rubberActive) {
            removeDrawing(activeId);
        }
        rubberActive = false;
        polyPts.clear();
        activeId.clear();
        activeHandle = -1;
        state = StateBrowse;
        if (resetTool) {
            tool = ToolBrowse;
        }
        setSelectedDrawing(QString());
    }

    // 多边形闭合为 HsCommData::HsPoly 并提交；不足 3 点则取消
    void closePolygon()
    {
        if (!rubberActive || polyPts.size() < 3) {
            if (sessionActive) {
                cancelSession();
            } else {
                cancelDraw(false);
            }
            return;
        }
        removeDrawing(activeId);
        rubberActive = false;
        HsCommData::Graphic region;
        region.kind = HsCommData::KindObject;
        region.role = HsCommData::RoleDrawing;
        region.geomStyle = penStyle;
        region.frameId = frame.frameId;
        region.visible = true;
        HsCommData::HsPoly poly;
        poly.closed = true;
        const std::vector<HsCommData::Point2d> pts = toPts(polyPts);
        for (size_t i = 0; i < pts.size(); ++i) {
            poly.pushSeg(HsCommData::PolySeg(pts[i], static_cast<int>(i), static_cast<int>(i)));
        }
        region.geom = HsCommData::HsObject(poly);
        region.updateBounds();
        polyPts.clear();
        activeId.clear();
        commitGraphic(region);
    }

    // 结束折线并提交；多边形走 closePolygon，不足 2 点则取消
    void finishPolyline()
    {
        if (!rubberActive) {
            if (sessionActive) {
                cancelSession();
            }
            return;
        }
        if (tool == ToolPolygon) {
            closePolygon();
            return;
        }
        GraphicItem* item = drawingById(activeId);
        if (!item || polyPts.size() < 2) {
            if (sessionActive) {
                cancelSession();
            } else {
                cancelDraw(false);
            }
            return;
        }
        HsCommData::Graphic g = item->graphic();
        removeDrawing(activeId);
        rubberActive = false;
        g.geom.contourAt(0)->setPts(toPts(polyPts));
        g.geom.contourAt(0)->closed = false;
        g.kind = HsCommData::KindObject;
        g.id.clear();
        polyPts.clear();
        activeId.clear();
        commitGraphic(g);
    }

    // 在设备坐标画测量读数，不进场景 Item
    // 锚点：线段中点、圆和椭圆右端、矩形左上、夹角顶点
    void paintMeasureLabels(QPainter* painter, const QSize& vs)
    {
        if (!painter) {
            return;
        }
        const QTransform toDevice = q->viewportTransform();
        QHash<QString, GraphicItem*>::const_iterator it = measureItems.constBegin();
        for (; it != measureItems.constEnd(); ++it) {
            const bool preview = rubberActive && it.key() == activeId;
            const HsCommData::Graphic& g = it.value()->graphic();
            const QString text = measureLabel(tool, preview, g, imageCalib);
            if (text.isEmpty() || !g.geom.isInitialized()) {
                continue;
            }
            QPointF anchor;
            if (g.geom.kind() == HsCommData::ObjLine) {
                const HsCommData::LineP* ln = g.geom.lineAt(0);
                if (!ln) {
                    continue;
                }
                anchor = QPointF((ln->a.x + ln->b.x) * 0.5, (ln->a.y + ln->b.y) * 0.5);
            } else if (g.geom.kind() == HsCommData::ObjCircle) {
                const HsCommData::CircleP* c = g.geom.circleAt(0);
                if (!c) {
                    continue;
                }
                anchor = QPointF(c->c.x + c->r, c->c.y);
            } else if (g.geom.kind() == HsCommData::ObjRect1) {
                const HsCommData::Rect1P* rc = g.geom.rect1At(0);
                if (!rc) {
                    continue;
                }
                const HsCommData::Box2 box = HsCommData::normalized(rc->r);
                anchor = QPointF(box.x, box.y);
            } else if (g.geom.kind() == HsCommData::ObjEllipse) {
                const HsCommData::EllipseP* el = g.geom.ellipseAt(0);
                if (!el) {
                    continue;
                }
                anchor = QPointF(el->c.x + el->ra, el->c.y);
            } else if (g.geom.kind() == HsCommData::ObjContour) {
                const HsCommData::HsContour* ct = g.geom.contourAt(0);
                if (!ct || ct->ptCount() < 2) {
                    continue;
                }
                const size_t at = ct->ptCount() >= 3 ? 1 : ct->ptCount() - 1;
                anchor = QPointF(ct->ptAt(at).x, ct->ptAt(at).y);
            } else {
                continue;
            }
            paintDeviceText(painter, toDevice.map(anchor) + QPointF(8, -8), text, 13,
                            QColor(0xFA, 0xFA, 0xFA), vs);
        }
    }

    // 记录光标位置并取样灰度，发 cursorMoved；HUD 或像素检验打开时重绘
    void updateCursor(const QPoint& viewPos)
    {
        cursorViewPos = viewPos;
        cursorPt = q->widgetToImage(viewPos);
        cursorValid = frame.sampleGray(cursorPt.x(), cursorPt.y(), &cursorGray);
        emit q->cursorMoved(cursorPt, cursorGray, cursorValid);
        if (hudVisible || pixelInspect) {
            q->viewport()->update();
        }
    }

    // 低倍像素检验：右上角画光标周围 7x7 邻域色块表
    void drawNeighborTable(QPainter* painter, int cx, int cy, const QSize& vpSize)
    {
        const bool color = inspectIsColor(frame);
        const int n = kInspectNeighborHalf * 2 + 1;
        const int cellW = color ? kInspectNeighborCellRgbW : kInspectNeighborCell;
        const int cellH = color ? kInspectNeighborCellRgbH : kInspectNeighborCell;
        const int pad = 6;
        const int titleH = 18;
        const int boxW = n * cellW + pad * 2;
        const int boxH = n * cellH + pad * 2 + titleH;
        QRect box(vpSize.width() - boxW - 8, 8, boxW, boxH);
        painter->fillRect(box, QColor(0, 0, 0, 188));
        painter->setPen(QColor(0x52, 0xC4, 0x1A));
        QFont title = q->font();
        title.setPixelSize(11);
        painter->setFont(title);
        painter->drawText(QRect(box.left() + pad, box.top() + 2, box.width() - pad * 2, titleH),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          color ? QStringLiteral("(%1, %2)  RGB").arg(cx).arg(cy)
                                : QStringLiteral("(%1, %2)").arg(cx).arg(cy));

        QFont cellFont = q->font();
        cellFont.setStyleHint(QFont::Monospace);
        cellFont.setPixelSize(color ? 9 : 10);
        painter->setFont(cellFont);

        // 每格用像素颜色填底、写数值；图外格画灰色短横；中心格蓝框
        for (int row = 0; row < n; ++row) {
            for (int col = 0; col < n; ++col) {
                const int px = cx - kInspectNeighborHalf + col;
                const int py = cy - kInspectNeighborHalf + row;
                const QRect cellRect(box.left() + pad + col * cellW,
                                     box.top() + pad + titleH + row * cellH,
                                     cellW, cellH);
                const bool center = (row == kInspectNeighborHalf && col == kInspectNeighborHalf);
                int r = 0;
                int g = 0;
                int b = 0;
                const bool ok = frame.sampleRgb(px, py, &r, &g, &b);
                if (ok) {
                    painter->fillRect(cellRect, QColor(r, g, b));
                    painter->setPen(inspectTextColor(r, g, b));
                    if (color) {
                        painter->drawText(cellRect, Qt::AlignCenter,
                                          QStringLiteral("%1\n%2\n%3").arg(r).arg(g).arg(b));
                    } else {
                        painter->drawText(cellRect, Qt::AlignCenter, QString::number(r));
                    }
                } else {
                    painter->fillRect(cellRect, QColor(32, 32, 32));
                    painter->setPen(QColor(90, 90, 90));
                    painter->drawText(cellRect, Qt::AlignCenter, QStringLiteral("-"));
                }
                painter->setPen(center ? QPen(QColor(0x18, 0x90, 0xFF), 2)
                                       : QPen(QColor(255, 255, 255, 40), 1));
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(cellRect.adjusted(0, 0, -1, -1));
            }
        }
    }

    // 光标旁的 RGB / 灰度浮窗，靠近边缘时翻到另一侧
    void drawHoverProbe(QPainter* painter, int cx, int cy)
    {
        int r = 0;
        int g = 0;
        int b = 0;
        if (!frame.sampleRgb(cx, cy, &r, &g, &b)) {
            return;
        }
        const bool color = inspectIsColor(frame);
        QFont f = q->font();
        f.setStyleHint(QFont::Monospace);
        f.setPixelSize(12);
        painter->setFont(f);
        const int pw = 48;
        const int ph = color ? 54 : 22;
        QPoint pos(cursorViewPos.x() + 16, cursorViewPos.y() + 12);
        const QSize vp = q->viewport()->size();
        if (pos.x() + pw > vp.width() - 8) {
            pos.setX(cursorViewPos.x() - pw - 12);
        }
        if (pos.y() + ph > vp.height() - 28) {
            pos.setY(cursorViewPos.y() - ph - 8);
        }
        if (pos.x() < 8) {
            pos.setX(8);
        }
        if (pos.y() < 8) {
            pos.setY(8);
        }
        const QRect box(pos, QSize(pw, ph));
        painter->fillRect(box, QColor(32, 32, 32, 220));
        painter->setPen(QColor(245, 245, 245));
        if (color) {
            painter->drawText(box, Qt::AlignCenter,
                              QStringLiteral("%1\n%2\n%3").arg(r).arg(g).arg(b));
        } else {
            painter->drawText(box, Qt::AlignCenter, QString::number(r));
        }
    }

    // 像素检验：高倍画格子，低倍画邻域表，数值读原图
    void drawPixelInspect(QPainter* painter)
    {
        // 高倍：原图像素填满可见格；低倍：邻域表加光标格。数值读 HsCommData::HsImage，不读 LOD
        if (!pixelInspect || frame.isNull()) {
            return;
        }

        const QTransform xf = painter->worldTransform();
        painter->save();
        painter->resetTransform();
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setRenderHint(QPainter::TextAntialiasing, false);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

        // 可见像素范围；够大且格数不超上限才画格子
        const QSize vpSize = viewSize();
        const qreal scale = vp.scale(vpSize);
        const QRectF vis = q->mapToScene(q->viewport()->rect()).boundingRect();
        const int x0 = qMax(0, qFloor(vis.left()));
        const int y0 = qMax(0, qFloor(vis.top()));
        const int x1 = qMin(frame.width - 1, qCeil(vis.right()) - 1);
        const int y1 = qMin(frame.height - 1, qCeil(vis.bottom()) - 1);
        const int cols = (x1 >= x0) ? (x1 - x0 + 1) : 0;
        const int rows = (y1 >= y0) ? (y1 - y0 + 1) : 0;
        const bool grid = (scale >= kInspectMinCellPx) && cols > 0 && rows > 0
            && (cols * rows <= kInspectMaxCells);

        const int cx = qFloor(cursorPt.x());
        const int cy = qFloor(cursorPt.y());

        if (grid) {
            QFont f = q->font();
            f.setStyleHint(QFont::Monospace);
            const bool showRgb = inspectIsColor(frame);
            const int fontPx = qBound(8, qRound(scale * (showRgb ? 0.20 : 0.28)), 16);
            f.setPixelSize(fontPx);
            painter->setFont(f);

            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    int r = 0;
                    int g = 0;
                    int b = 0;
                    if (!frame.sampleRgb(x, y, &r, &g, &b)) {
                        continue;
                    }
                    const QRect cell = pixelDeviceRect(xf, x, y);
                    painter->fillRect(cell, QColor(r, g, b));
                    painter->setPen(QPen(QColor(255, 255, 255, 70), 1));
                    painter->setBrush(Qt::NoBrush);
                    painter->drawRect(cell.adjusted(0, 0, -1, -1));
                    painter->setPen(inspectTextColor(r, g, b));
                    if (showRgb) {
                        painter->drawText(cell, Qt::AlignCenter,
                                          QStringLiteral("%1\n%2\n%3").arg(r).arg(g).arg(b));
                    } else {
                        painter->drawText(cell, Qt::AlignCenter, QString::number(r));
                    }
                }
            }

            if (cursorValid && cx >= x0 && cx <= x1 && cy >= y0 && cy <= y1) {
                const QRect cell = pixelDeviceRect(xf, cx, cy);
                painter->setPen(QPen(QColor(0x18, 0x90, 0xFF), 2));
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(cell.adjusted(1, 1, -2, -2));
            }
        } else if (cursorValid) {
            if (scale >= 4.0 && cx >= 0 && cy >= 0 && cx < frame.width && cy < frame.height) {
                const QRect cell = pixelDeviceRect(xf, cx, cy);
                painter->setPen(QPen(QColor(0x52, 0xC4, 0x1A), 1));
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(cell.adjusted(0, 0, -1, -1));
            }
            drawNeighborTable(painter, cx, cy, vpSize);
        }

        if (cursorValid) {
            drawHoverProbe(painter, cx, cy);
        }

        painter->restore();
    }

    // 释放画笔掩码、预览图与撤销栈
    void clearBrushStorage()
    {
        brushBuf.clear();
        brushUndo.clear();
        brushOverlay = QImage();
        brushW = 0;
        brushH = 0;
        brushCount = 0;
        brushStroke = false;
        brushErase = false;
    }

    // 按底图尺寸分配画笔掩码与透明预览图；内存不足返回 false
    bool allocBrush()
    {
        clearBrushStorage();
        const int w = vp.imageSize.width();
        const int h = vp.imageSize.height();
        if (w <= 0 || h <= 0) {
            return false;
        }
        const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
        try {
            brushBuf.assign(n, 0);
        } catch (...) {
            clearBrushStorage();
            return false;
        }
        brushOverlay = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        if (brushOverlay.isNull()) {
            clearBrushStorage();
            return false;
        }
        brushOverlay.fill(0);
        brushW = w;
        brushH = h;
        brushCount = 0;
        return true;
    }

    // 涂抹预览色（预乘）：优先填充色，否则描边色配 140 透明度，都未设则用橙色
    QRgb brushStampColor() const
    {
        HsCommData::Color4 c = penStyle.fill;
        if (c.isUnset()) {
            c = penStyle.stroke;
            if (c.isUnset()) {
                c = HsCommData::Color4(0xFA, 0xAD, 0x14, 140);
            } else {
                c.a = 140;
            }
        }
        const int a = static_cast<int>(c.a);
        return qRgba(static_cast<int>(c.r) * a / 255,
                     static_cast<int>(c.g) * a / 255,
                     static_cast<int>(c.b) * a / 255,
                     a);
    }

    // 画笔光标圈颜色：描边色优先，其次填充色，缺省橙色
    QColor brushRingColor() const
    {
        HsCommData::Color4 c = penStyle.stroke.isUnset() ? penStyle.fill : penStyle.stroke;
        if (c.isUnset()) {
            return QColor(0xFA, 0xAD, 0x14);
        }
        return QColor(c.r, c.g, c.b);
    }

    // 图像坐标 floor 到整数像素
    QPoint imagePixel(const QPointF& img) const
    {
        return QPoint(qFloor(img.x()), qFloor(img.y()));
    }

    // 只重绘图像像素矩形对应的视口区域，四周外扩 2px
    void repaintImageRect(const QRect& imgRect)
    {
        if (!imgRect.isValid() || imgRect.isEmpty()) {
            return;
        }
        const QPointF tl = q->imageToWidget(QPointF(imgRect.left(), imgRect.top()));
        const QPointF br = q->imageToWidget(QPointF(imgRect.right() + 1, imgRect.bottom() + 1));
        q->viewport()->update(QRectF(tl, br).normalized().toAlignedRect().adjusted(-2, -2, 2, 2));
    }

    // 重绘画笔圈所在区域；仅画笔会话中有效
    void repaintBrushRing(const QPointF& img)
    {
        if (!sessionActive || tool != ToolBrush) {
            return;
        }
        const int pad = qCeil(brushRadius) + 3;
        const int x = qFloor(img.x()) - pad;
        const int y = qFloor(img.y()) - pad;
        repaintImageRect(QRect(x, y, pad * 2 + 2, pad * 2 + 2));
    }

    // 以 (cx, cy) 为圆心按画笔半径盖一个圆点（或擦除），同步掩码、预览与计数；返回改动的像素矩形
    QRect stampDisk(int cx, int cy, bool erase)
    {
        if (brushOverlay.isNull() || brushW <= 0 || brushH <= 0) {
            return QRect();
        }
        const double r = brushRadius;
        const double r2 = r * r;
        const int ri = qMax(0, qCeil(r));
        const int x0 = qMax(0, cx - ri);
        const int y0 = qMax(0, cy - ri);
        const int x1 = qMin(brushW - 1, cx + ri);
        const int y1 = qMin(brushH - 1, cy + ri);
        if (x0 > x1 || y0 > y1) {
            return QRect();
        }
        const QRgb color = erase ? 0 : brushStampColor();
        for (int y = y0; y <= y1; ++y) {
            unsigned char* row = &brushBuf[static_cast<size_t>(y) * static_cast<size_t>(brushW)];
            QRgb* dst = reinterpret_cast<QRgb*>(brushOverlay.scanLine(y));
            const double dy = static_cast<double>(y - cy);
            for (int x = x0; x <= x1; ++x) {
                const double dx = static_cast<double>(x - cx);
                if (dx * dx + dy * dy > r2) {
                    continue;
                }
                if (erase) {
                    if (row[x] != 0) {
                        row[x] = 0;
                        dst[x] = 0;
                        --brushCount;
                    }
                } else if (row[x] == 0) {
                    row[x] = 1;
                    dst[x] = color;
                    ++brushCount;
                }
            }
        }
        return QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    }

    // 沿 a→b 按约 1/3 半径的步长连续盖点，避免快速拖动时断笔
    void stampBetween(const QPoint& a, const QPoint& b, bool erase)
    {
        const int step = qMax(1, qCeil(brushRadius / 3.0));
        const double dx = static_cast<double>(b.x() - a.x());
        const double dy = static_cast<double>(b.y() - a.y());
        const double len = qSqrt(dx * dx + dy * dy);
        const int n = qMax(1, qCeil(len / static_cast<double>(step)));
        QRect dirty;
        for (int i = 0; i <= n; ++i) {
            const int x = qRound(static_cast<double>(a.x()) + dx * static_cast<double>(i) / static_cast<double>(n));
            const int y = qRound(static_cast<double>(a.y()) + dy * static_cast<double>(i) / static_cast<double>(n));
            dirty |= stampDisk(x, y, erase);
        }
        repaintImageRect(dirty);
    }

    // 开始一笔：先把当前掩码压入撤销栈（最多 8 步），再在起点盖点
    void beginBrushStroke(const QPointF& img, bool erase)
    {
        if (brushOverlay.isNull()) {
            return;
        }
        if (brushUndo.size() >= 8) {
            brushUndo.remove(0);
        }
        brushUndo.append(brushBuf);
        brushStroke = true;
        brushErase = erase;
        brushLastPx = imagePixel(img);
        repaintImageRect(stampDisk(brushLastPx.x(), brushLastPx.y(), erase));
    }

    // 笔划进行中：从上一点连到当前点
    void moveBrushStroke(const QPointF& img)
    {
        if (!brushStroke) {
            return;
        }
        const QPoint px = imagePixel(img);
        stampBetween(brushLastPx, px, brushErase);
        brushLastPx = px;
    }

    // 撤销一步：恢复上一份掩码并据此重画预览、重算计数
    void undoBrushStroke()
    {
        if (brushStroke || brushUndo.isEmpty() || brushOverlay.isNull()) {
            return;
        }
        brushBuf.swap(brushUndo.last());
        brushUndo.remove(brushUndo.size() - 1);
        brushOverlay.fill(0);
        brushCount = 0;
        const QRgb color = brushStampColor();
        for (int y = 0; y < brushH; ++y) {
            const unsigned char* row = &brushBuf[static_cast<size_t>(y) * static_cast<size_t>(brushW)];
            QRgb* dst = reinterpret_cast<QRgb*>(brushOverlay.scanLine(y));
            for (int x = 0; x < brushW; ++x) {
                if (row[x] != 0) {
                    dst[x] = color;
                    ++brushCount;
                }
            }
        }
        q->viewport()->update();
    }

    // 清掉全部涂抹，可撤销
    void clearBrushInk()
    {
        if (brushStroke || brushOverlay.isNull() || brushCount <= 0) {
            return;
        }
        if (brushUndo.size() >= 8) {
            brushUndo.remove(0);
        }
        brushUndo.append(brushBuf);
        std::fill(brushBuf.begin(), brushBuf.end(), static_cast<unsigned char>(0));
        brushOverlay.fill(0);
        brushCount = 0;
        q->viewport()->update();
    }

    // 把画笔掩码逐行扫成游程区域，并算出包围框
    HsCommData::HsRegion regionFromBrush() const
    {
        HsCommData::HsRegion out;
        if (brushW <= 0 || brushH <= 0 || brushBuf.empty()) {
            return out;
        }
        int minC = brushW;
        int maxC = -1;
        int minR = -1;
        int maxR = -1;
        for (int y = 0; y < brushH; ++y) {
            const unsigned char* row = &brushBuf[static_cast<size_t>(y) * static_cast<size_t>(brushW)];
            int x = 0;
            while (x < brushW) {
                while (x < brushW && row[x] == 0) {
                    ++x;
                }
                if (x >= brushW) {
                    break;
                }
                const int cb = x;
                while (x < brushW && row[x] != 0) {
                    ++x;
                }
                const int ce = x - 1;
                out.pushRun(HsCommData::Chord(y, cb, ce));
                if (minR < 0) {
                    minR = y;
                }
                maxR = y;
                if (cb < minC) {
                    minC = cb;
                }
                if (ce > maxC) {
                    maxC = ce;
                }
            }
        }
        if (minR >= 0) {
            out.bounds = HsCommData::IRect(minC, minR, maxC - minC + 1, maxR - minR + 1);
        }
        return out;
    }

    // 取消绘制会话：丢弃橡皮筋或草稿，回到浏览，发 drawSessionFinished(false)
    void cancelSession()
    {
        if (!sessionActive) {
            return;
        }
        sessionActive = false;
        crosshairOn = false;
        brushStroke = false;
        brushRightArm = false;
        if (rubberActive) {
            cancelDraw(false);
        } else if (sessionDraft && !activeId.isEmpty()) {
            removeDrawing(activeId);
            activeId.clear();
            activeHandle = -1;
            setSelectedDrawing(QString());
        }
        sessionDraft = false;
        clearBrushStorage();
        tool = ToolBrowse;
        state = StateBrowse;
        q->viewport()->unsetCursor();
        q->viewport()->update();
        emit q->drawSessionFinished(HsCommData::Graphic(), false);
    }

    // 换底图后：无图、尺寸变化或策略要求清 ROI 时取消进行中的会话
    void abandonSessionIfImageChanged(const QSize& oldSize)
    {
        if (!sessionActive) {
            return;
        }
        const bool sizeChanged = oldSize != vp.imageSize;
        const bool noImage = frame.isNull() || vp.imageSize.isEmpty();
        if (noImage || sizeChanged || imageRefresh.refreshDrawing) {
            cancelSession();
        }
    }

    // 开始绘制会话：运行态、无图或工具不支持时失败；画笔先分配掩码，再清掉选中与橡皮筋
    bool beginSession(DrawTool next)
    {
        if (sessionActive || mode == WindowRuntime) {
            return false;
        }
        if (frame.isNull() || vp.imageSize.isEmpty() || !sessionToolOk(next)) {
            return false;
        }
        if (next == ToolBrush && !allocBrush()) {
            return false;
        }
        if (rubberActive) {
            cancelDraw(false);
        }
        sessionActive = true;
        sessionDraft = false;
        crosshairOn = false;
        brushRightArm = false;
        tool = next;
        state = StateBrowse;
        rubberActive = false;
        polyPts.clear();
        activeId.clear();
        activeHandle = -1;
        setSelectedDrawing(QString());
        q->setFocus();
        q->viewport()->setCursor(Qt::CrossCursor);
        q->viewport()->update();
        return true;
    }

    // 提交会话中的形状：折线 / 多边形走 finishPolyline；其余取草稿，过小则取消，测量工具补测量 id
    void commitSessionShape()
    {
        if (tool == ToolPolygon || tool == ToolContour) {
            if (!rubberActive) {
                cancelSession();
                return;
            }
            finishPolyline();
            return;
        }
        if (!rubberActive && !sessionDraft) {
            cancelSession();
            return;
        }
        GraphicItem* item = drawingById(activeId);
        if (!item) {
            cancelSession();
            return;
        }
        HsCommData::Graphic g = item->graphic();
        sessionDraft = false;
        removeDrawing(activeId);
        rubberActive = false;
        polyPts.clear();
        activeId.clear();
        state = StateBrowse;
        if (finishPreviewIfTiny(g)) {
            cancelSession();
            return;
        }
        if (tool == ToolMeasureAngle) {
            const HsCommData::HsContour* ct = g.geom.isInitialized() ? g.geom.contourAt(0) : nullptr;
            if (!ct || ct->ptCount() < 3) {
                cancelSession();
                return;
            }
            g.id = fromQ(nextId(QStringLiteral("m.ang")));
        } else if (tool == ToolMeasureDist) {
            g.id = fromQ(nextId(QStringLiteral("m.dist")));
        } else if (tool == ToolMeasureRadius) {
            g.id = fromQ(nextId(QStringLiteral("m.rad")));
        } else if (tool == ToolMeasureRect) {
            g.id = fromQ(nextId(QStringLiteral("m.rect")));
        } else if (tool == ToolMeasureEllipse) {
            g.id = fromQ(nextId(QStringLiteral("m.ell")));
        } else {
            g.id.clear();
        }
        commitGraphic(g);
    }

    // 提交画笔会话：掩码转区域后作为一条 Drawing 提交，没涂则取消
    void commitSessionBrush()
    {
        brushStroke = false;
        brushRightArm = false;
        HsCommData::HsRegion region = regionFromBrush();
        if (brushCount <= 0 || region.isEmpty()) {
            cancelSession();
            return;
        }
        HsCommData::Graphic g;
        g.kind = HsCommData::KindObject;
        g.role = HsCommData::RoleDrawing;
        g.geomStyle = penStyle;
        g.geom = region;
        g.frameId = frame.frameId;
        g.visible = true;
        commitGraphic(g);
    }

    // 提交当前会话，按工具分到画笔或形状
    void commitSession()
    {
        if (!sessionActive) {
            return;
        }
        if (tool == ToolBrush) {
            commitSessionBrush();
            return;
        }
        commitSessionShape();
    }
};

// 图像窗口：关滚动条与自动锚点，场景坐标 = 图像像素；默认测量条含全部五种测量
HsImageWindow::HsImageWindow(QWidget* parent)
    : QGraphicsView(parent)
    , d(new Private(this))
{
    registerGraphicMetaTypes();

    setScene(&d->scene);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setCacheMode(QGraphicsView::CacheNone);
    setRenderHint(QPainter::Antialiasing, true);
    setBackgroundBrush(Qt::NoBrush);
    if (viewport()) {
        viewport()->setAutoFillBackground(false);
    }
    setStyleSheet(QStringLiteral("QGraphicsView { border: none; background: transparent; }"));
    d->measureShown.push_back(ToolMeasureDist);
    d->measureShown.push_back(ToolMeasureAngle);
    d->measureShown.push_back(ToolMeasureRadius);
    d->measureShown.push_back(ToolMeasureRect);
    d->measureShown.push_back(ToolMeasureEllipse);
    d->buildOverlayToolbar();
}

// 释放 Pimpl
HsImageWindow::~HsImageWindow()
{
    if (d) {
        delete d;
        d = nullptr;
    }
}

// 从原始像素换底图：指针或尺寸非法则清空底图，编码无法识别则保持现状
void HsImageWindow::setImage(const uint8_t* data, int width, int height, int strideBytes,
                             const QString& encoding)
{
    setImage(data, width, height, strideBytes, encoding, 0);
}

void HsImageWindow::setImage(const uint8_t* data, int width, int height, int strideBytes,
                             const QString& encoding, int64_t frameId)
{
    if (!data || width <= 0 || height <= 0 || strideBytes <= 0) {
        clearImage();
        return;
    }
    qint64 id = frameId;
    if (id <= 0) {
        id = (d->frame.frameId != 0) ? d->frame.frameId : 1;
    }
    const HsCommData::HsImage fb = HsCommData::HsImage::fromRaw(data, width, height, strideBytes,
                                                encoding.toLatin1().constData(), id);
    if (fb.isNull()) {
        return;
    }
    // 深度探针不随普通换图清除，由壳层用 clearDepthProbe / setDepthProbe* 显式管理
    const QSize oldSize = d->vp.imageSize;
    d->setFrame(fb);
    d->abandonSessionIfImageChanged(oldSize);
    d->applyRefreshAfterNewImage();
}

// 直接贴已备好的图；不更新 frame，取样与 HUD 仍读上一次 setImage 的底图
void HsImageWindow::setPreparedImage(const QImage& image)
{
    if (!d || image.isNull()) {
        return;
    }
    d->setPrepared(image);
}

// 同 setImage，保留旧调用名
void HsImageWindow::setImageRaw(const uint8_t* data, int width, int height, int strideBytes,
                                const QString& encoding)
{
    setImage(data, width, height, strideBytes, encoding);
}

// 用 cv::Mat 换底图：空图清底图，非 8UC1/8UC3/8UC4 忽略；未启用 OpenCV 时为空操作
void HsImageWindow::setImage(const cv::Mat& mat)
{
#ifdef HSIMAGE_HAS_OPENCV
    if (mat.empty()) {
        clearImage();
        return;
    }
    const cv::Mat src = mat.isContinuous() ? mat : mat.clone();
    const char* enc = nullptr;
    switch (src.type()) {
    case CV_8UC1:
        enc = "mono8";
        break;
    case CV_8UC3:
        enc = "bgr8";
        break;
    case CV_8UC4:
        enc = "bgra8";
        break;
    default:
        return;
    }
    setImage(src.data, src.cols, src.rows, static_cast<int>(src.step),
             QString::fromLatin1(enc));
#else
    Q_UNUSED(mat);
#endif
}

// 解码 JPEG/PNG 等压缩数据换底图：空数据清底图，解码失败保持现状
void HsImageWindow::setCompressedImage(const QByteArray& data, const QString& formatHint)
{
    if (data.isEmpty()) {
        clearImage();
        return;
    }

    QString hint = formatHint.toLower();
    if (hint.contains(QLatin1Char(';'))) {
        hint = hint.section(QLatin1Char(';'), 0, 0).trimmed();
    }
    const char* fmt = nullptr;
    if (hint.contains(QStringLiteral("png"))) {
        fmt = "PNG";
    } else if (hint.contains(QStringLiteral("jpeg")) || hint.contains(QStringLiteral("jpg"))) {
        fmt = "JPEG";
    }

    QImage decoded;
    const bool ok = fmt ? decoded.loadFromData(data, fmt) : decoded.loadFromData(data);
    if (!ok || decoded.isNull()) {
        return;
    }
    const qint64 id = (d->frame.frameId != 0) ? d->frame.frameId : 1;
    const QSize oldSize = d->vp.imageSize;
    d->setFrame(frameFromQImage(decoded, id));
    d->abandonSessionIfImageChanged(oldSize);
    d->applyRefreshAfterNewImage();
}

// 清空底图与深度探针、图像坐标文字，保留 Stack / Drawing；进行中的会话先取消
void HsImageWindow::clearImage()
{
    if (d->sessionActive) {
        d->cancelSession();
    }
    d->clearDepthProbeData();
    d->frame = HsCommData::HsImage();
    d->imageItem->clear();
    d->vp.imageSize = QSize();
    d->vp.part = QRectF();
    d->imageDrawInfos.clear();
    d->cursorValid = false;
    d->applyTransform();
    d->syncStackVisibility();
    d->hideCtrlPixelPopup();
    viewport()->update();
}

// 是否有底图
bool HsImageWindow::hasImage() const
{
    return !d->frame.isNull();
}

// 当前底图像素尺寸，无图为空
QSize HsImageWindow::currentImageSize() const
{
    return d->frame.isNull() ? QSize() : bufferPixelSize(d->frame);
}

// 同 currentImageSize
QSize HsImageWindow::imageSize() const
{
    return currentImageSize();
}

// 当前底图帧号，无图为 0
qint64 HsImageWindow::currentFrameId() const
{
    return d->frame.isNull() ? 0 : d->frame.frameId;
}

// disp* 描边色；填充取同色、透明度 40
void HsImageWindow::setOverlayColor(const QColor& color)
{
    d->overlayStyle.stroke = fromQ(color);
    QColor fill = color;
    fill.setAlpha(40);
    d->overlayStyle.fill = fromQ(fill);
}

// disp* 线宽，下限 0.5
void HsImageWindow::setOverlayWidth(double widthPx)
{
    d->overlayStyle.widthPx = qMax(0.5, widthPx);
}

// 追加圆到 Stack
void HsImageWindow::dispCircle(double x, double y, double r)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom.circleAt(0)->c = HsCommData::Point2d(x, y);
    g.geom.circleAt(0)->r = r;
    d->appendStack(g);
}

// 追加圆到 Stack（协议类型）
void HsImageWindow::dispCircle(const HsCommData::CircleP& circle)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = circle;
    d->appendStack(g);
}

// 追加椭圆到 Stack，phi 按系统角度设置解释
void HsImageWindow::dispEllipse(double x, double y, double ra, double rb, double phi)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = HsCommData::EllipseP();
    g.geom.ellipseAt(0)->c = HsCommData::Point2d(x, y);
    g.geom.ellipseAt(0)->ra = ra;
    g.geom.ellipseAt(0)->rb = rb;
    g.geom.ellipseAt(0)->setAng(phi);
    d->appendStack(g);
}

// 追加椭圆到 Stack（协议类型）
void HsImageWindow::dispEllipse(const HsCommData::EllipseP& ellipse)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = ellipse;
    d->appendStack(g);
}

// 追加轴对齐矩形到 Stack
void HsImageWindow::dispRect1(double left, double top, double width, double height)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = HsCommData::Rect1P();
    g.geom.rect1At(0)->r = HsCommData::Box2(left, top, width, height);
    d->appendStack(g);
}

// 追加轴对齐矩形到 Stack（协议类型）
void HsImageWindow::dispRect1(const HsCommData::Rect1P& rect)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = rect;
    d->appendStack(g);
}

// 追加轴对齐矩形到 Stack（HsCommData::Box2）
void HsImageWindow::dispRect1(const HsCommData::Box2& box)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = HsCommData::Rect1P();
    g.geom.rect1At(0)->r = box;
    d->appendStack(g);
}

// 追加旋转矩形到 Stack，phi 按系统角度设置解释
void HsImageWindow::dispRect2(double x, double y, double l1, double l2, double phi)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = HsCommData::Rect2P();
    g.geom.rect2At(0)->c = HsCommData::Point2d(x, y);
    g.geom.rect2At(0)->l1 = l1;
    g.geom.rect2At(0)->l2 = l2;
    g.geom.rect2At(0)->setAng(phi);
    d->appendStack(g);
}

// 追加旋转矩形到 Stack（协议类型）
void HsImageWindow::dispRect2(const HsCommData::Rect2P& rect)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = rect;
    d->appendStack(g);
}

// 追加直线到 Stack
void HsImageWindow::dispLine(double x1, double y1, double x2, double y2)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = HsCommData::LineP();
    g.geom.lineAt(0)->a = HsCommData::Point2d(x1, y1);
    g.geom.lineAt(0)->b = HsCommData::Point2d(x2, y2);
    d->appendStack(g);
}

// 追加直线到 Stack（协议类型）
void HsImageWindow::dispLine(const HsCommData::LineP& line)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = line;
    d->appendStack(g);
}

// 追加十字到 Stack，size 小于 0 表示穿窗
void HsImageWindow::dispCross(double x, double y, double size, double angle)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindCross;
    g.cross.resize(1);
    g.cross[0].center = HsCommData::Point2d(x, y);
    g.cross[0].size = size;
    g.cross[0].setAng(angle);
    d->appendStack(g);
}

// 追加十字到 Stack（协议点）
void HsImageWindow::dispCross(const HsCommData::Point2d& center, double size, double angle)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindCross;
    g.cross.resize(1);
    g.cross[0].center = center;
    g.cross[0].size = size;
    g.cross[0].setAng(angle);
    d->appendStack(g);
}

// 追加箭头到 Stack，(x1,y1) 为尾、(x2,y2) 为头
void HsImageWindow::dispArrow(double x1, double y1, double x2, double y2)
{
    HsCommData::HsArrow arrow;
    arrow.tail = HsCommData::Point2d(x1, y1);
    arrow.head = HsCommData::Point2d(x2, y2);
    dispArrow(arrow);
}

// 追加箭头到 Stack（协议类型）
void HsImageWindow::dispArrow(const HsCommData::HsArrow& arrow)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindArrow;
    g.arrow.resize(1);
    g.arrow[0] = arrow;
    d->appendStack(g);
}

// 追加行程区域到 Stack
void HsImageWindow::dispRegion(const HsCommData::HsRegion& region)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = region;
    d->appendStack(g);
}

// 追加 XLD / 折线到 Stack
void HsImageWindow::dispContour(const HsCommData::HsContour& contour)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = contour;
    d->appendStack(g);
}

// 整页替换 Stack；frameId 与底图一致才显示
void HsImageWindow::setOverlay(const std::shared_ptr<const HsCommData::FrameOverlay>& overlay)
{
    d->overlay = overlay;
    d->rebuildStack();
    d->noteFrame();
    viewport()->update();
}

// 由图形列表构造 overlay 再整页替换
void HsImageWindow::setOverlay(const std::vector<HsCommData::Graphic>& items, int64_t frameId)
{
    std::shared_ptr<HsCommData::FrameOverlay> sc = HsCommData::FrameOverlay::create(frameId);
    sc->items = items;
    setOverlay(sc);
}

// 替换 Stack，frameId 取当前底图
void HsImageWindow::setOverlays(const std::vector<HsCommData::Graphic>& items)
{
    setOverlay(items, d->frame.frameId);
}

// 当前 Stack 协议快照
std::shared_ptr<const HsCommData::FrameOverlay> HsImageWindow::overlay() const
{
    return d->overlay;
}

// 清空 Stack
void HsImageWindow::clearWindow()
{
    d->overlay.reset();
    d->rebuildStack();
    viewport()->update();
}

// 同 clearWindow
void HsImageWindow::clearOverlays()
{
    clearWindow();
}

// Stack 层显隐，不影响 Drawing
void HsImageWindow::setOverlayVisible(bool visible)
{
    setStackVisible(visible);
}

// Stack 层开关是否打开
bool HsImageWindow::isOverlayVisible() const
{
    return d->stackVisible;
}

// 替换 Drawing 层 ROI：按 id 更新或插入，列表里没有的删除；未设描边的用默认画笔
void HsImageWindow::setDrawings(const std::vector<HsCommData::Graphic>& rois)
{
    QSet<QString> seen;
    for (size_t i = 0; i < rois.size(); ++i) {
        HsCommData::Graphic g = rois[i];
        g.role = HsCommData::RoleDrawing;
        if (g.geomStyle.stroke.isUnset()) {
            g.geomStyle = HsCommData::GeomStyle::drawingStyle();
        }
        g.updateBounds();
        seen.insert(toQ(g.id));
        d->upsertDrawing(g);
    }
    QList<QString> drop;
    QHash<QString, GraphicItem*>::iterator it = d->drawingItems.begin();
    for (; it != d->drawingItems.end(); ++it) {
        if (!seen.contains(it.key())) {
            drop.push_back(it.key());
        }
    }
    for (int i = 0; i < drop.size(); ++i) {
        d->removeDrawing(drop[i]);
    }
    viewport()->update();
}

// 当前 Drawing 层全部 ROI（不含测量）
std::vector<HsCommData::Graphic> HsImageWindow::drawings() const
{
    std::vector<HsCommData::Graphic> out;
    out.reserve(static_cast<size_t>(d->drawingItems.size()));
    QHash<QString, GraphicItem*>::const_iterator it = d->drawingItems.constBegin();
    for (; it != d->drawingItems.constEnd(); ++it) {
        out.push_back(it.value()->graphic());
    }
    return out;
}

// 清空测量层；选中项在其中则取消选中
void HsImageWindow::clearMeasures()
{
    const bool dropSel = d->measureObj.contains(d->activeId);
    d->measureObj.clear();
    if (dropSel) {
        d->activeId.clear();
        d->activeHandle = -1;
        d->state = StateBrowse;
        viewport()->unsetCursor();
        emit roiSelected(HsCommData::Graphic());
    }
    if (viewport()) {
        viewport()->update();
    }
}
// 删除一条测量，id 不是测量则不动
void HsImageWindow::removeMeasure(const QString& id)
{
    if (!d->measureItems.contains(id)) {
        return;
    }
    const bool dropSel = (id == d->activeId);
    d->removeDrawing(id);
    if (dropSel) {
        d->activeId.clear();
        d->activeHandle = -1;
        d->state = StateBrowse;
        viewport()->unsetCursor();
        emit roiSelected(HsCommData::Graphic());
    }
    if (viewport()) {
        viewport()->update();
    }
}
// 清空 Drawing；会话或橡皮筋进行中先取消
void HsImageWindow::clearDrawings()
{
    if (d->sessionActive) {
        d->cancelSession();
    } else if (d->rubberActive) {
        d->cancelDraw(false);
    }
    const QList<QString> ids = d->drawingItems.keys();
    for (int i = 0; i < ids.size(); ++i) {
        d->removeDrawing(ids[i]);
    }
    d->activeId.clear();
    d->activeHandle = -1;
    d->state = StateBrowse;
    viewport()->unsetCursor();
    viewport()->update();
    emit roiSelected(HsCommData::Graphic());
}

// 清掉 Stack、Drawing、测量和全部文字，保留底图；先回到浏览工具
void HsImageWindow::clearDisplay()
{
    if (d->sessionActive) {
        d->cancelSession();
    } else if (d->rubberActive) {
        d->cancelDraw(true);
    }
    if (d->tool != ToolBrowse) {
        setDrawTool(ToolBrowse);
    }
    clearOverlays();
    clearDrawings();
    clearMeasures();
    clearDrawInfo();
    if (viewport()) {
        viewport()->update();
    }
}

// 当前选中 ROI；无选中时 id 为空
HsCommData::Graphic HsImageWindow::selectedDrawing() const
{
    GraphicItem* item = d->drawingById(d->activeId);
    return item ? item->graphic() : HsCommData::Graphic();
}

// 新绘 ROI 的画笔；已选中则同步改样式
void HsImageWindow::setDrawingStyle(const HsCommData::GeomStyle& style)
{
    d->penStyle = style;
    GraphicItem* item = d->drawingById(d->activeId);
    if (item) {
        HsCommData::Graphic g = item->graphic();
        g.geomStyle = style;
        item->setGraphic(g);
        viewport()->update();
        emit roiSelected(g);
    }
}

// 当前画笔
HsCommData::GeomStyle HsImageWindow::drawingStyle() const
{
    return d->penStyle;
}

// ROI 画笔颜色：描边为该色，填充同色、透明度 40；已选中则同步
void HsImageWindow::setPenColor(const QColor& color)
{
    d->penStyle.stroke = fromQ(color);
    QColor fill = color;
    fill.setAlpha(40);
    d->penStyle.fill = fromQ(fill);
    GraphicItem* item = d->drawingById(d->activeId);
    if (item) {
        HsCommData::Graphic g = item->graphic();
        g.geomStyle.stroke = d->penStyle.stroke;
        g.geomStyle.fill = d->penStyle.fill;
        item->setGraphic(g);
        viewport()->update();
        emit roiSelected(g);
    }
}

// ROI 画笔线宽，下限 0.5；已选中则同步
void HsImageWindow::setPenWidth(double widthPx)
{
    d->penStyle.widthPx = qMax(0.5, widthPx);
    GraphicItem* item = d->drawingById(d->activeId);
    if (item) {
        HsCommData::Graphic g = item->graphic();
        g.geomStyle.widthPx = d->penStyle.widthPx;
        item->setGraphic(g);
        viewport()->update();
        emit roiSelected(g);
    }
}

// 底图层显隐
void HsImageWindow::setImageVisible(bool visible)
{
    d->imageItem->setVisible(visible);
}

// 底图是否可见
bool HsImageWindow::isImageVisible() const
{
    return d->imageItem->isVisible();
}

// 图像坐标系中的可见矩形 part
QRectF HsImageWindow::part() const
{
    return d->vp.part;
}

// 设置可见矩形并同步宽高比
void HsImageWindow::setPart(const QRectF& part)
{
    d->vp.part = part;
    d->vp.syncAspect(d->viewSize());
    d->applyTransform();
    viewport()->update();
}

// 整幅图适应窗口
void HsImageWindow::fitToWindow()
{
    d->vp.fit(d->viewSize());
    d->applyTransform();
    viewport()->update();
}

// 1:1 显示，窗口中心对准图像中心
void HsImageWindow::zoomActualSize()
{
    if (d->vp.imageSize.isEmpty() || d->viewSize().isEmpty()) {
        return;
    }
    const QSize vp = d->viewSize();
    QRectF part(0, 0, vp.width(), vp.height());
    part.moveCenter(QPointF(d->vp.imageSize.width() * 0.5, d->vp.imageSize.height() * 0.5));
    d->vp.part = part;
    d->applyTransform();
    viewport()->update();
}

// 同 zoomActualSize
void HsImageWindow::resetView()
{
    zoomActualSize();
}

// 以视口中心为锚放大
void HsImageWindow::zoomIn()
{
    d->zoomBy(kZoomInFactor);
}

// 以视口中心为锚缩小
void HsImageWindow::zoomOut()
{
    d->zoomBy(kZoomOutFactor);
}

// 当前缩放（屏幕像素 / 图像像素）
qreal HsImageWindow::zoomScale() const
{
    return d->vp.scale(d->viewSize());
}

// 设计态可画 ROI；运行态取消会话、回到浏览并隐藏 Drawing
void HsImageWindow::setWindowMode(WindowMode mode)
{
    d->mode = mode;
    if (mode == WindowRuntime) {
        if (d->sessionActive) {
            d->cancelSession();
        }
        d->tool = ToolBrowse;
        d->state = StateBrowse;
        d->rubberActive = false;
        d->setLayerVisible(LayerDrawing, false);
        d->setSelectedDrawing(QString());
    } else {
        d->setLayerVisible(LayerDrawing, d->drawingVisible);
    }
    viewport()->update();
}

// 当前窗口模式
WindowMode HsImageWindow::windowMode() const
{
    return d->mode;
}

// 切换绘制工具；会话中或 ToolBrush 忽略，运行态只允许浏览和测量
void HsImageWindow::setDrawTool(DrawTool tool)
{
    if (d->sessionActive || tool == ToolBrush) {
        return;
    }
    if (d->mode == WindowRuntime && tool != ToolBrowse && !measureTool(tool)) {
        d->tool = ToolBrowse;
        d->state = StateBrowse;
        d->syncMeasureChecks();
        emit drawToolChanged(static_cast<int>(d->tool));
        return;
    }
    if (d->rubberActive) {
        d->cancelDraw(false);
    }
    d->tool = tool;
    d->state = StateBrowse;
    if (tool != ToolBrowse) {
        d->setSelectedDrawing(QString());
    }
    d->syncMeasureChecks();
    emit drawToolChanged(static_cast<int>(d->tool));
}

// 设置像素比例，未写入的单位按 1:1；只影响测量与 HUD 读数
void HsImageWindow::setImageCalib(const HsCommData::ImageCalib& calib)
{
    d->imageCalib = calib;
    if (viewport()) {
        viewport()->update();
    }
}

// 当前像素比例
HsCommData::ImageCalib HsImageWindow::imageCalib() const
{
    return d->imageCalib;
}

// 当前绘制工具
DrawTool HsImageWindow::drawTool() const
{
    return d->tool;
}

// 开始一次独占绘制会话
bool HsImageWindow::beginDrawSession(DrawTool tool)
{
    return d->beginSession(tool);
}

// 是否正在绘制会话中
bool HsImageWindow::drawSessionActive() const
{
    return d->sessionActive;
}

// 提交当前会话
void HsImageWindow::commitDrawSession()
{
    d->commitSession();
}


// 取消未提交的橡皮筋，并回到浏览工具
void HsImageWindow::cancelActiveDraw()
{
    d->cancelDraw();
    if (viewport()) {
        viewport()->update();
    }
}

// 弹出保存快照对话框
void HsImageWindow::promptSaveSnapshot()
{
    d->promptSaveSnapshot();
}

// 弹出导出原图对话框
void HsImageWindow::promptSaveOriginal()
{
    d->promptSaveOriginal();
}
// 取消当前会话，不保留草稿
void HsImageWindow::cancelDrawSession()
{
    d->cancelSession();
}

// 画笔半径夹到 0.5..256 并保留一位小数
void HsImageWindow::setBrushRadius(double radiusPx)
{
    if (radiusPx < 0.5) {
        radiusPx = 0.5;
    }
    if (radiusPx > 256.0) {
        radiusPx = 256.0;
    }
    d->brushRadius = qRound(radiusPx * 10.0) / 10.0;
    if (d->sessionActive && d->tool == ToolBrush) {
        viewport()->update();
    }
}

// 当前画笔半径（图像像素）
double HsImageWindow::brushRadius() const
{
    return d->brushRadius;
}

// 文本工具要写下的字符串
void HsImageWindow::setDrawText(const QString& text)
{
    d->textDraft = text;
}

// 文本工具当前字符串
QString HsImageWindow::drawText() const
{
    return d->textDraft;
}

// 文本工具锚点坐标系
void HsImageWindow::setTextToolSpace(DrawInfoSpace space)
{
    d->textSpace = space;
}

// 文本工具当前坐标系
DrawInfoSpace HsImageWindow::textToolSpace() const
{
    return d->textSpace;
}

// 文本工具字号（屏幕像素），下限 1
void HsImageWindow::setTextFontPx(int px)
{
    d->textFontPx = qMax(1, px);
}

// 文本工具当前字号
int HsImageWindow::textFontPx() const
{
    return d->textFontPx;
}

// 新十字的半宽，小于 0 表示穿窗
void HsImageWindow::setCrossSize(double size)
{
    d->crossSize = size;
}

// 新十字的半宽
double HsImageWindow::crossSize() const
{
    return d->crossSize;
}

// 按 id 更新或插入一条 Drawing 并选中；id 空则当作新提交
void HsImageWindow::setDrawing(const HsCommData::Graphic& roi)
{
    HsCommData::Graphic g = roi;
    g.role = HsCommData::RoleDrawing;
    if (g.geomStyle.stroke.isUnset()) {
        g.geomStyle = d->penStyle;
    }
    g.frameId = d->frame.frameId;
    g.updateBounds();
    if (g.id.empty()) {
        d->commitGraphic(g);
        return;
    }
    d->upsertDrawing(g);
    d->setSelectedDrawing(toQ(g.id));
    viewport()->update();
}

// 读选中圆，kind 不符返回 false
bool HsImageWindow::selectedAsCircle(double* x, double* y, double* r) const
{
    const HsCommData::Graphic g = selectedDrawing();
    if (g.id.empty() || g.kind != HsCommData::KindObject || g.geom.kind() != HsCommData::ObjCircle) {
        return false;
    }
    if (x) {
        *x = g.geom.circleAt(0)->c.x;
    }
    if (y) {
        *y = g.geom.circleAt(0)->c.y;
    }
    if (r) {
        *r = g.geom.circleAt(0)->r;
    }
    return true;
}

// 读选中椭圆，转角按系统角度设置输出
bool HsImageWindow::selectedAsEllipse(double* x, double* y, double* ra, double* rb, double* phi) const
{
    const HsCommData::Graphic g = selectedDrawing();
    if (g.id.empty() || g.kind != HsCommData::KindObject || g.geom.kind() != HsCommData::ObjEllipse) {
        return false;
    }
    if (x) {
        *x = g.geom.ellipseAt(0)->c.x;
    }
    if (y) {
        *y = g.geom.ellipseAt(0)->c.y;
    }
    if (ra) {
        *ra = g.geom.ellipseAt(0)->ra;
    }
    if (rb) {
        *rb = g.geom.ellipseAt(0)->rb;
    }
    if (phi) {
        *phi = g.geom.ellipseAt(0)->ang();
    }
    return true;
}

// 读选中轴对齐矩形，先规范化再输出左上与宽高
bool HsImageWindow::selectedAsRect1(double* left, double* top, double* width, double* height) const
{
    const HsCommData::Graphic g = selectedDrawing();
    if (g.id.empty() || g.kind != HsCommData::KindObject || g.geom.kind() != HsCommData::ObjRect1) {
        return false;
    }
    const HsCommData::Box2 r = normalized(g.geom.rect1At(0)->r);
    if (left) {
        *left = HsCommData::left(r);
    }
    if (top) {
        *top = HsCommData::top(r);
    }
    if (width) {
        *width = HsCommData::width(r);
    }
    if (height) {
        *height = HsCommData::height(r);
    }
    return true;
}

// 读选中旋转矩形，转角按系统角度设置输出
bool HsImageWindow::selectedAsRect2(double* x, double* y, double* l1, double* l2, double* phi) const
{
    const HsCommData::Graphic g = selectedDrawing();
    if (g.id.empty() || g.kind != HsCommData::KindObject || g.geom.kind() != HsCommData::ObjRect2) {
        return false;
    }
    if (x) {
        *x = g.geom.rect2At(0)->c.x;
    }
    if (y) {
        *y = g.geom.rect2At(0)->c.y;
    }
    if (l1) {
        *l1 = g.geom.rect2At(0)->l1;
    }
    if (l2) {
        *l2 = g.geom.rect2At(0)->l2;
    }
    if (phi) {
        *phi = g.geom.rect2At(0)->ang();
    }
    return true;
}

// 读选中直线两端
bool HsImageWindow::selectedAsLine(double* x1, double* y1, double* x2, double* y2) const
{
    const HsCommData::Graphic g = selectedDrawing();
    if (g.id.empty() || g.kind != HsCommData::KindObject || g.geom.kind() != HsCommData::ObjLine) {
        return false;
    }
    if (x1) {
        *x1 = g.geom.lineAt(0)->a.x;
    }
    if (y1) {
        *y1 = g.geom.lineAt(0)->a.y;
    }
    if (x2) {
        *x2 = g.geom.lineAt(0)->b.x;
    }
    if (y2) {
        *y2 = g.geom.lineAt(0)->b.y;
    }
    return true;
}

// 读选中十字：中心、半宽、转角
bool HsImageWindow::selectedAsCross(double* x, double* y, double* size, double* angle) const
{
    const HsCommData::Graphic g = selectedDrawing();
    if (g.id.empty() || g.kind != HsCommData::KindCross || g.cross.empty()) {
        return false;
    }
    if (x) {
        *x = g.cross[0].center.x;
    }
    if (y) {
        *y = g.cross[0].center.y;
    }
    if (size) {
        *size = g.cross[0].size;
    }
    if (angle) {
        *angle = g.cross[0].ang();
    }
    return true;
}

// 读选中箭头，(x1,y1) 为尾、(x2,y2) 为头
bool HsImageWindow::selectedAsArrow(double* x1, double* y1, double* x2, double* y2) const
{
    const HsCommData::Graphic g = selectedDrawing();
    if (g.id.empty() || g.kind != HsCommData::KindArrow || g.arrow.empty()) {
        return false;
    }
    if (x1) {
        *x1 = g.arrow[0].tail.x;
    }
    if (y1) {
        *y1 = g.arrow[0].tail.y;
    }
    if (x2) {
        *x2 = g.arrow[0].head.x;
    }
    if (y2) {
        *y2 = g.arrow[0].head.y;
    }
    return true;
}

// HUD（坐标 / 灰度条）显隐
void HsImageWindow::setHudVisible(bool visible)
{
    d->hudVisible = visible;
    viewport()->update();
}

// HUD 是否打开
bool HsImageWindow::isHudVisible() const
{
    return d->hudVisible;
}

// 信息条文本
void HsImageWindow::setImageInfoText(const QString& text)
{
    d->imageInfoText = text;
    viewport()->update();
}

// 当前信息条文本
QString HsImageWindow::imageInfoText() const
{
    return d->imageInfoText;
}

// 像素检验开关，与 HUD 独立；同时让底图关掉设备缓存
void HsImageWindow::setPixelInspect(bool on)
{
    d->pixelInspect = on;
    d->imageItem->setPixelInspect(on);
    viewport()->update();
}

// 像素检验是否打开
bool HsImageWindow::isPixelInspect() const
{
    return d->pixelInspect;
}

// 实时标记（Demo 跟帧刷新时置位）
void HsImageWindow::setLive(bool live)
{
    d->live = live;
    viewport()->update();
}

// 是否处于实时标记
bool HsImageWindow::isLive() const
{
    return d->live;
}

// Stack 层显隐
void HsImageWindow::setStackVisible(bool visible)
{
    d->stackVisible = visible;
    d->syncStackVisibility();
}

// Stack 层开关是否打开
bool HsImageWindow::isStackVisible() const
{
    return d->stackVisible;
}

// Drawing 层显隐；运行态只记开关，层保持隐藏
void HsImageWindow::setDrawingVisible(bool visible)
{
    d->drawingVisible = visible;
    if (d->mode != WindowRuntime) {
        d->setLayerVisible(LayerDrawing, visible);
    }
}

// Drawing 层开关是否打开
bool HsImageWindow::isDrawingVisible() const
{
    return d->drawingVisible;
}

// 图像坐标取灰度，floor 落到像素
bool HsImageWindow::sampleGray(const QPointF& imagePt, double* gray) const
{
    return d->frame.sampleGray(imagePt.x(), imagePt.y(), gray);
}

// 图像坐标取 RGB
bool HsImageWindow::sampleRgb(const QPointF& imagePt, int* r, int* g, int* b) const
{
    return d->frame.sampleRgb(imagePt.x(), imagePt.y(), r, g, b);
}

namespace {

// 把带行跨度的深度图逐行拷成紧排数组；strideBytes 不大于 0 时按宽度计算
template <typename T>
void copyDepthRows(const T* data, int width, int height, int strideBytes, std::vector<T>* out)
{
    out->clear();
    if (!data || width <= 0 || height <= 0 || !out) {
        return;
    }
    const int effective = strideBytes > 0 ? strideBytes
                                          : static_cast<int>(sizeof(T) * static_cast<size_t>(width));
    out->resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    const unsigned char* base = reinterpret_cast<const unsigned char*>(data);
    for (int y = 0; y < height; ++y) {
        const T* src = reinterpret_cast<const T*>(base + y * effective);
        std::copy(src, src + width, out->begin() + static_cast<size_t>(y) * static_cast<size_t>(width));
    }
}

}  // namespace

// 设 16 位深度探针（毫米），清掉其他类型；数据无效则探针为空
void HsImageWindow::setDepthProbeU16(const uint16_t* data, int width, int height, int strideBytes)
{
    d->depthProbeF32.clear();
    d->depthProbeF64.clear();
    copyDepthRows(data, width, height, strideBytes, &d->depthProbeU16);
    d->depthProbeKind = d->depthProbeU16.empty() ? Private::DepthProbeNone : Private::DepthProbeU16;
    d->depthProbeW = d->depthProbeU16.empty() ? 0 : width;
    d->depthProbeH = d->depthProbeU16.empty() ? 0 : height;
}

// 设 32 位浮点深度探针（米），清掉其他类型
void HsImageWindow::setDepthProbeF32(const float* data, int width, int height, int strideBytes)
{
    d->depthProbeU16.clear();
    d->depthProbeF64.clear();
    copyDepthRows(data, width, height, strideBytes, &d->depthProbeF32);
    d->depthProbeKind = d->depthProbeF32.empty() ? Private::DepthProbeNone : Private::DepthProbeF32;
    d->depthProbeW = d->depthProbeF32.empty() ? 0 : width;
    d->depthProbeH = d->depthProbeF32.empty() ? 0 : height;
}

// 设 64 位浮点深度探针（米），清掉其他类型
void HsImageWindow::setDepthProbeF64(const double* data, int width, int height, int strideBytes)
{
    d->depthProbeU16.clear();
    d->depthProbeF32.clear();
    copyDepthRows(data, width, height, strideBytes, &d->depthProbeF64);
    d->depthProbeKind = d->depthProbeF64.empty() ? Private::DepthProbeNone : Private::DepthProbeF64;
    d->depthProbeW = d->depthProbeF64.empty() ? 0 : width;
    d->depthProbeH = d->depthProbeF64.empty() ? 0 : height;
}

// 设深度探针的归一化灰度图，供小窗「灰度显示」一行
void HsImageWindow::setDepthProbeGray(const QImage& gray)
{
    d->depthProbeGray = gray;
}

// 清掉深度探针
void HsImageWindow::clearDepthProbe()
{
    d->clearDepthProbeData();
}

// 控件坐标 → 图像坐标
QPointF HsImageWindow::widgetToImage(const QPointF& widgetPos) const
{
    // 取当前视口变换的逆，任意缩放倍率通用；不可逆时返回原点
    bool ok = false;
    const QTransform inv = viewportTransform().inverted(&ok);
    if (!ok) {
        return QPointF();
    }
    return inv.map(widgetPos);
}

// 图像坐标 → 控件坐标
QPointF HsImageWindow::imageToWidget(const QPointF& imagePt) const
{
    return viewportTransform().map(imagePt);
}

// 控件坐标落到图像整数像素；无图或在图外返回 false
bool HsImageWindow::mapWidgetToImagePixel(const QPointF& widgetPos, QPoint* pixel) const
{
    if (!pixel || d->frame.isNull()) {
        return false;
    }
    const QPointF img = widgetToImage(widgetPos);
    const int x = qFloor(img.x());
    const int y = qFloor(img.y());
    if (x < 0 || y < 0 || x >= d->frame.width || y >= d->frame.height) {
        return false;
    }
    *pixel = QPoint(x, y);
    return true;
}

// 叠加文字：清空该坐标系下全部条目再放这一条，message 为空则只清空
void HsImageWindow::drawInfo(DrawInfoSpace space, const QString& message, const QPointF& position,
                             int fontSizePx, const QColor& color)
{
    QVector<DrawInfoEntry>& list = (space == DrawInfoWindow) ? d->windowDrawInfos : d->imageDrawInfos;
    list.clear();
    if (!message.isEmpty()) {
        DrawInfoEntry e;
        e.text = message;
        e.pos = position;
        e.fontPx = fontSizePx > 0 ? fontSizePx : 1;
        e.color = color.isValid() ? color : QColor(255, 255, 255);
        list.push_back(e);
    }
    viewport()->update();
}

// 追加一条叠加文字，message 为空则忽略
void HsImageWindow::addDrawInfo(DrawInfoSpace space, const QString& message, const QPointF& position,
                                int fontSizePx, const QColor& color)
{
    if (message.isEmpty()) {
        return;
    }
    DrawInfoEntry e;
    e.text = message;
    e.pos = position;
    e.fontPx = fontSizePx > 0 ? fontSizePx : 1;
    e.color = color.isValid() ? color : QColor(255, 255, 255);
    if (space == DrawInfoWindow) {
        d->windowDrawInfos.push_back(e);
    } else {
        d->imageDrawInfos.push_back(e);
    }
    viewport()->update();
}

// 清除一种坐标系的叠加文字
void HsImageWindow::clearDrawInfo(DrawInfoSpace space)
{
    if (space == DrawInfoWindow) {
        d->windowDrawInfos.clear();
    } else {
        d->imageDrawInfos.clear();
    }
    viewport()->update();
}

// 清除两种坐标系的全部叠加文字
void HsImageWindow::clearDrawInfo()
{
    d->imageDrawInfos.clear();
    d->windowDrawInfos.clear();
    viewport()->update();
}

// 保存当前原图像素，不含叠加；失败时写 error
bool HsImageWindow::saveCurrentFrame(const QString& filePath, QString* error) const
{
    if (filePath.isEmpty()) {
        if (error) {
            *error = QString::fromUtf8("保存路径为空");
        }
        return false;
    }
    const QImage saved = qImageFromFrame(d->frame);
    if (d->frame.isNull() || saved.isNull()) {
        if (error) {
            *error = QString::fromUtf8("无图像");
        }
        return false;
    }
    if (!saved.save(filePath)) {
        if (error) {
            *error = QString::fromUtf8("保存图像失败: %1").arg(filePath);
        }
        return false;
    }
    return true;
}

// 截取视口当前显示（底图、叠加与文字），浮动工具条不在视口里故不含
bool HsImageWindow::saveSnapshot(const QString& filePath, QString* error) const
{
    if (filePath.isEmpty()) {
        if (error) {
            *error = QString::fromUtf8("保存路径为空");
        }
        return false;
    }
    QWidget* vp = viewport();
    if (!vp) {
        if (error) {
            *error = QString::fromUtf8("无视图");
        }
        return false;
    }
    const QPixmap pix = vp->grab();
    if (pix.isNull()) {
        if (error) {
            *error = QString::fromUtf8("截取显示失败");
        }
        return false;
    }
    if (!pix.save(filePath)) {
        if (error) {
            *error = QString::fromUtf8("保存快照失败: %1").arg(filePath);
        }
        return false;
    }
    return true;
}

// 浮动视图工具条显隐
void HsImageWindow::setOverlayToolbarVisible(bool visible)
{
    if (!d->overlayBar) {
        return;
    }
    d->overlayBar->setVisible(visible);
    d->placeOverlayToolbar();
}

// 视图工具条是否可见
bool HsImageWindow::isOverlayToolbarVisible() const
{
    return d->overlayBar && d->overlayBar->isVisible();
}
// 测量工具条显隐；没有测量按钮时仍不显示
void HsImageWindow::setMeasureToolbarVisible(bool visible)
{
    d->measureBarWant = visible;
    d->syncMeasureBar();
}

// 测量工具条是否可见
bool HsImageWindow::isMeasureToolbarVisible() const
{
    return d->measureBar && d->measureBar->isVisible();
}


// 工具条停靠角
void HsImageWindow::setOverlayToolbarCorner(Qt::Corner corner)
{
    d->overlayBarCorner = corner;
    d->placeOverlayToolbar();
}

// 当前停靠角
Qt::Corner HsImageWindow::overlayToolbarCorner() const
{
    return d->overlayBarCorner;
}

// 按钮垂直或水平排列，两条工具条同步
void HsImageWindow::setOverlayToolbarOrientation(Qt::Orientation orientation)
{
    d->overlayBarOrient = orientation;
    if (d->overlayBarLayout) {
        d->overlayBarLayout->setDirection(
            orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    }
    if (d->measureBarLayout) {
        d->measureBarLayout->setDirection(
            orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    }
    d->placeOverlayToolbar();
}

// 当前排列方向
Qt::Orientation HsImageWindow::overlayToolbarOrientation() const
{
    return d->overlayBarOrient;
}

// 视图工具条控件
QWidget* HsImageWindow::overlayToolbar() const
{
    return d->overlayBar;
}

// 在视图工具条末尾加一个控件，归工具条所有
void HsImageWindow::addOverlayToolbarWidget(QWidget* widget)
{
    if (!widget || !d->overlayBarLayout) {
        return;
    }
    widget->setParent(d->overlayBar);
    d->overlayBarLayout->addWidget(widget);
    d->placeOverlayToolbar();
}

// 设置测量工具条显示哪些按钮：只保留测量工具并去重，然后重建按钮
void HsImageWindow::setMeasureTools(const std::vector<DrawTool>& tools)
{
    d->measureShown.clear();
    for (size_t i = 0; i < tools.size(); ++i) {
        if (!measureTool(tools[i])) {
            continue;
        }
        bool seen = false;
        for (size_t k = 0; k < d->measureShown.size(); ++k) {
            if (d->measureShown[k] == tools[i]) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            d->measureShown.push_back(tools[i]);
        }
    }
    d->rebuildMeasureButtons();
}

// 当前显示的测量按钮
std::vector<DrawTool> HsImageWindow::measureTools() const
{
    return d->measureShown;
}

// 已提交测量的读数，每条一行「名称  读数」，随当前标定换算
QStringList HsImageWindow::measureResultLines() const
{
    QStringList lines;
    QHash<QString, GraphicItem*>::const_iterator it = d->measureItems.constBegin();
    for (; it != d->measureItems.constEnd(); ++it) {
        const HsCommData::Graphic& g = it.value()->graphic();
        const QString text = measureLabel(d->tool, false, g, d->imageCalib);
        if (text.isEmpty()) {
            continue;
        }
        QString name = QString::fromUtf8("测量");
        if (g.id.compare(0, 6, "m.dist") == 0) {
            name = QString::fromUtf8("距离");
        } else if (g.id.compare(0, 5, "m.ang") == 0) {
            name = QString::fromUtf8("夹角");
        } else if (g.id.compare(0, 5, "m.rad") == 0) {
            name = QString::fromUtf8("圆");
        } else if (g.id.compare(0, 6, "m.rect") == 0) {
            name = QString::fromUtf8("矩形");
        } else if (g.id.compare(0, 5, "m.ell") == 0) {
            name = QString::fromUtf8("椭圆");
        }
        lines.append(name + QStringLiteral("  ") + text);
    }
    return lines;
}

// 下次换底图时是否清 Stack / ROI / 文字
void HsImageWindow::setImageRefreshPolicy(const ImageRefreshPolicy& policy)
{
    d->imageRefresh = policy;
}

// 当前换底图刷新策略
ImageRefreshPolicy HsImageWindow::imageRefreshPolicy() const
{
    return d->imageRefresh;
}

// 视口背景风格
void HsImageWindow::setViewportBackground(ViewportBackground mode)
{
    d->vpBg = mode;
    if (viewport()) {
        viewport()->update();
    }
}

// 当前视口背景风格
ViewportBackground HsImageWindow::viewportBackground() const
{
    return d->vpBg;
}

// 指定色：Custom 为填充色，棋盘格为底格色；无效色恢复风格默认
void HsImageWindow::setViewportBackgroundCustomColor(const QColor& color)
{
    d->vpBgCustom = color;
    if (viewport()) {
        viewport()->update();
    }
}

// 当前指定色，无效表示未指定
QColor HsImageWindow::viewportBackgroundCustomColor() const
{
    return d->vpBgCustom;
}

// 切到 Custom 并设填充色
void HsImageWindow::setBackgroundColor(const QColor& color)
{
    d->vpBgCustom = color;
    d->vpBg = ViewportCustom;
    if (viewport()) {
        viewport()->update();
    }
}

// 当前实际填充参考色
QColor HsImageWindow::backgroundColor() const
{
    return d->resolvedViewportColor();
}

// 工具条皮肤，换皮肤后重画图标
void HsImageWindow::setOverlayToolbarSkin(OverlayToolbarSkin skin)
{
    d->overlayBarSkin = skin;
    d->applyOverlayToolbarSkin();
}

// 当前工具条皮肤
OverlayToolbarSkin HsImageWindow::overlayToolbarSkin() const
{
    return d->overlayBarSkin;
}

// Overlay 刷新频率（Hz）
qreal HsImageWindow::fps() const
{
    return d->fps;
}

// 视口背景：屏幕坐标纯色或棋盘格，不走场景变换
void HsImageWindow::drawBackground(QPainter* painter, const QRectF&)
{
    painter->save();
    painter->resetTransform();
    d->paintViewportBackground(painter);
    painter->restore();
}

// 画画笔预览、像素检验、测量读数、十字线、drawInfo、信息条与 HUD，不改场景 Item
void HsImageWindow::drawForeground(QPainter* painter, const QRectF& exposed)
{
    // 画笔会话：在场景坐标贴掩码预览，再画光标圈
    if (d->sessionActive && d->tool == ToolBrush && !d->brushOverlay.isNull()) {
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
        const QRectF src = exposed.intersected(QRectF(0, 0, d->brushW, d->brushH));
        if (!src.isEmpty()) {
            painter->drawImage(src, d->brushOverlay, src);
        }
        if (d->cursorValid) {
            QPen ring(d->brushRingColor());
            ring.setCosmetic(true);
            ring.setWidthF(1.0);
            painter->setPen(ring);
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(d->cursorPt, d->brushRadius, d->brushRadius);
        }
        painter->restore();
    }

    if (d->pixelInspect) {
        d->drawPixelInspect(painter);
    }

    // 以下都在设备坐标画
    painter->save();
    painter->resetTransform();

    const QSize vs = viewport()->size();
    d->paintMeasureLabels(painter, vs);
    QString calibText;
    if (d->cursorValid) {
        const std::string cal = HsCommData::formatPoint(d->cursorPt.x(), d->cursorPt.y(), d->imageCalib);
        if (!cal.empty()) {
            calibText = QString::fromUtf8(cal.c_str());
        }
    }
    if (d->crosshairOn && d->sessionActive && d->tool != ToolBrush) {
        QColor crossColor(255, 0, 0);
        if (!d->penStyle.stroke.isUnset()) {
            crossColor = toQ(d->penStyle.stroke);
        }
        QPen crossPen(crossColor);
        crossPen.setCosmetic(true);
        crossPen.setWidthF(1.0);
        painter->setPen(crossPen);
        const QPoint p = d->cursorViewPos;
        painter->drawLine(0, p.y(), vs.width(), p.y());
        painter->drawLine(p.x(), 0, p.x(), vs.height());
    }
    if (!d->frame.isNull()) {
        const QTransform toDevice = viewportTransform();
        for (int i = 0; i < d->imageDrawInfos.size(); ++i) {
            const DrawInfoEntry& info = d->imageDrawInfos.at(i);
            paintDeviceText(painter, toDevice.map(info.pos), info.text, info.fontPx, info.color, vs);
        }
    }
    for (int i = 0; i < d->windowDrawInfos.size(); ++i) {
        const DrawInfoEntry& info = d->windowDrawInfos.at(i);
        paintDeviceText(painter, info.pos, info.text, info.fontPx, info.color, vs);
    }

    if (!d->imageInfoText.isEmpty()) {
        QFont infoFont = font();
        infoFont.setPixelSize(12);
        painter->setFont(infoFont);
        painter->setPen(QColor(0xFA, 0xFA, 0xFA));
        const int y = d->hudVisible ? (calibText.isEmpty() ? 30 : 48) : 8;
        painter->drawText(QRect(12, y, qMax(1, viewport()->width() - 24), 20),
                          Qt::AlignVCenter | Qt::AlignLeft, d->imageInfoText);
    }

    if (!d->hudVisible) {
        painter->restore();
        return;
    }

    // HUD：顶部坐标与像素值，有标定时第二行显示换算坐标；底部状态栏显示尺寸、缩放、FPS
    QFont f = font();
    f.setPixelSize(12);
    painter->setFont(f);

    QString xText = QStringLiteral("--");
    QString yText = QStringLiteral("--");
    const bool color = inspectIsColor(d->frame);
    QString valueLabel = color ? QStringLiteral("RGB") : QStringLiteral("Gray");
    QString valueText = QStringLiteral("--");
    if (d->cursorValid) {
        xText = QString::number(d->cursorPt.x(), 'f', 2);
        yText = QString::number(d->cursorPt.y(), 'f', 2);
        if (color) {
            int r = 0;
            int g = 0;
            int b = 0;
            if (d->frame.sampleRgb(d->cursorPt.x(), d->cursorPt.y(), &r, &g, &b)) {
                valueText = QStringLiteral("%1, %2, %3").arg(r).arg(g).arg(b);
            }
        } else {
            valueText = QString::number(d->cursorGray, 'f', 0);
        }
    }

    painter->setPen(QColor(0x52, 0xC4, 0x1A));
    const QString top = QStringLiteral("X: %1  Y: %2  %3: %4")
                            .arg(xText)
                            .arg(yText)
                            .arg(valueLabel)
                            .arg(valueText);
    painter->drawText(QRect(12, 8, 560, 20), Qt::AlignVCenter | Qt::AlignLeft, top);
    if (!calibText.isEmpty()) {
        painter->drawText(QRect(12, 26, qMax(1, viewport()->width() - 24), 18),
                          Qt::AlignVCenter | Qt::AlignLeft, calibText);
    }

    const QRect vp = QRect(QPoint(0, 0), viewport()->size());
    const int barH = 22;
    const QRect bar(0, vp.height() - barH, vp.width(), barH);
    painter->fillRect(bar, QColor(0, 0, 0, 180));
    painter->setPen(QColor(0x52, 0xC4, 0x1A));

    QString sizeText = QStringLiteral("-- x --");
    if (!d->frame.isNull()) {
        sizeText = QStringLiteral("%1 x %2").arg(d->frame.width).arg(d->frame.height);
    }
    const int zoomPct = qRound(d->vp.scale(d->viewSize()) * 100.0);
    const QString bottom = QStringLiteral("%1    %2%    (%3, %4)    %5: %6    FPS: %7%8")
                               .arg(sizeText)
                               .arg(zoomPct)
                               .arg(xText)
                               .arg(yText)
                               .arg(valueLabel)
                               .arg(valueText)
                               .arg(d->fps, 0, 'f', 1)
                               .arg(d->live ? QStringLiteral("    Live") : QString());
    painter->drawText(bar.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, bottom);
    painter->restore();
}

// 窗口尺寸变化时按新宽高比修正 part，并重新摆放工具条
void HsImageWindow::resizeEvent(QResizeEvent* e)
{
    QGraphicsView::resizeEvent(e);
    if (d->vp.isValid()) {
        d->vp.syncAspect(d->viewSize());
        d->applyTransform();
    }
    d->placeOverlayToolbar();
}

// 给布局一个固定的建议尺寸，不随图像或 sceneRect 撑大
QSize HsImageWindow::sizeHint() const
{
    return QSize(320, 240);
}

// 最小建议尺寸，允许窗口缩得很小
QSize HsImageWindow::minimumSizeHint() const
{
    return QSize(64, 64);
}

// 首次显示时，有图但还没 fit 的补做一次
void HsImageWindow::showEvent(QShowEvent* e)
{
    QGraphicsView::showEvent(e);
    if (d->vp.imageSize.isValid() && !d->vp.isValid()) {
        d->vp.fit(d->viewSize());
        d->applyTransform();
    }
    d->placeOverlayToolbar();
}

// 滚轮以光标为锚点缩放
void HsImageWindow::wheelEvent(QWheelEvent* e)
{
    if (!d->vp.isValid()) {
        return;
    }
    const QPointF img = widgetToImage(e->pos());
    const QRectF p = d->vp.part;
    const QPointF rel((img.x() - p.x()) / p.width(), (img.y() - p.y()) / p.height());
    const qreal factor = (e->angleDelta().y() > 0) ? 1.25 : 0.8;
    d->vp.zoomAt(img, rel, factor, d->viewSize());
    d->applyTransform();
    d->updateCursor(e->pos());
    e->accept();
}

// 按下：中键平移 / 右键收尾 / 左键编辑、点选或开始绘制
void HsImageWindow::mousePressEvent(QMouseEvent* e)
{
    setFocus();
    d->lastViewPos = e->pos();
    d->pressImage = widgetToImage(e->pos());
    d->updateCursor(e->pos());

    // 中键：开始平移，绘制中不打断橡皮筋
    if (e->button() == Qt::MiddleButton) {
        if (d->state != StateDraw) {
            d->state = StatePan;
        }
        e->accept();
        return;
    }

    // 右键：画笔会话待松手判定，多边形 / 轮廓收尾，其他会话提交当前形状
    if (e->button() == Qt::RightButton) {
        if (d->sessionActive && d->tool == ToolBrush) {
            d->brushRightArm = true;
            d->brushRightView = e->pos();
            d->skipNextContextMenu = true;
            e->accept();
            return;
        }
        if (d->rubberActive && (d->tool == ToolPolygon || d->tool == ToolContour)) {
            d->finishPolyline();
            d->skipNextContextMenu = true;
        } else if (d->sessionActive) {
            // 会话中右键与 Halcon draw_* 一致：结束并提交当前形状            d->skipNextContextMenu = true;
            d->commitSessionShape();
        }
        e->accept();
        return;
    }

    if (e->button() != Qt::LeftButton) {
        e->accept();
        return;
    }

    // 会话已有草稿时，左键只允许编辑自己的草稿，点别处不做事
    if (d->sessionActive && d->sessionDraft) {
        GraphicItem* mine = d->drawingById(d->activeId);
        bool onMine = false;
        if (mine) {
            QString hid;
            int hh = -1;
            if (d->hitHandle(e->pos(), &hid, &hh) && hid == d->activeId) {
                onMine = true;
            } else if (mine->hitShape(d->pressImage, d->imageTol())) {
                onMine = true;
            }
        }
        if (onMine) {
            d->tryBeginEdit(e->pos(), d->pressImage);
        }
        e->accept();
        return;
    }

    // 非会话：先试编辑已有 Drawing，再试点选 Stack（仅设计态）
    if (!d->sessionActive && d->tryBeginEdit(e->pos(), d->pressImage)) {
        e->accept();
        return;
    }

    if (!d->sessionActive && d->mode == WindowDesign) {
        GraphicItem* stack = d->hitStack(d->pressImage);
        if (stack) {
            emit stackItemClicked(toQ(stack->graphic().id));
            e->accept();
            return;
        }
    }

    // 浏览工具，或运行态下的非测量工具：左键拖动平移并取消选中
    if (!d->sessionActive && (d->tool == ToolBrowse
        || (d->mode == WindowRuntime && !measureTool(d->tool)))) {
        d->state = StatePan;
        d->setSelectedDrawing(QString());
        e->accept();
        return;
    }

    if (d->sessionActive && d->tool == ToolBrush) {
        d->beginBrushStroke(d->pressImage, false);
        e->accept();
        return;
    }

    if (!d->rubberActive) {
        d->setSelectedDrawing(QString());
    }

    if (d->tool == ToolText) {
        d->placeTextTool(e->pos(), d->pressImage);
        e->accept();
        return;
    }

    // 逐点工具：首点建预览；多边形点回首点 10px 内闭合；测角满 3 点直接提交
    if (d->tool == ToolContour || d->tool == ToolPolygon || d->tool == ToolMeasureAngle) {
        if (!d->rubberActive) {
            HsCommData::Graphic g = d->makePreview(d->tool, d->pressImage);
            g.id = fromQ(d->nextId(QStringLiteral("preview")));
            d->polyPts.clear();
            d->polyPts.push_back(d->pressImage);
            d->upsertDrawing(g);
            d->activeId = toQ(g.id);
            d->rubberActive = true;
            d->state = StateDraw;
        } else {
            const qreal closePx = d->polyPts.isEmpty()
                ? 1e9
                : QLineF(mapFromScene(d->polyPts.first()), e->pos()).length();
            if (d->tool == ToolPolygon && d->polyPts.size() >= 3 && closePx <= 10.0) {
                d->closePolygon();
            } else {
                d->polyPts.push_back(d->pressImage);
                GraphicItem* item = d->drawingById(d->activeId);
                if (item) {
                    HsCommData::Graphic g = item->graphic();
                    g.geom.contourAt(0)->setPts(toPts(d->polyPts));
                    item->setGraphic(g);
                    if (d->tool == ToolMeasureAngle && d->polyPts.size() >= 3) {
                        HsCommData::Graphic done = g;
                        d->removeDrawing(d->activeId);
                        d->rubberActive = false;
                        d->polyPts.clear();
                        done.id = fromQ(d->nextId(QStringLiteral("m.ang")));
                        d->commitGraphic(done);
                    }
                }
            }
        }
        e->accept();
        return;
    }

    // 拖拽型工具：以按下点建预览，进入橡皮筋
    HsCommData::Graphic g = d->makePreview(d->tool, d->pressImage);
    g.id = fromQ(d->nextId(QStringLiteral("preview")));
    d->upsertDrawing(g);
    d->activeId = toQ(g.id);
    d->rubberActive = true;
    d->state = StateDraw;
    e->accept();
}

// 移动：刷新光标与 HUD，按状态平移、拖柄编辑、拉伸橡皮筋或画笔涂抹
void HsImageWindow::mouseMoveEvent(QMouseEvent* e)
{
    // 光标、十字线、画笔圈与 Ctrl 像素小窗
    const QPointF prevRing = d->brushRingPt;
    d->updateCursor(e->pos());
    if (d->sessionActive && d->tool != ToolBrush) {
        d->crosshairOn = true;
        viewport()->update();
    }
    if (d->sessionActive && d->tool == ToolBrush) {
        d->repaintBrushRing(prevRing);
        d->repaintBrushRing(d->cursorPt);
        d->brushRingPt = d->cursorPt;
    }
    if (e->modifiers().testFlag(Qt::ControlModifier)) {
        d->updateCtrlPixelPopup(e->pos());
    } else {
        d->hideCtrlPixelPopup();
    }
    const QPointF img = widgetToImage(e->pos());

    if (!(e->buttons() & Qt::LeftButton) && !(e->buttons() & Qt::MiddleButton)
        && !(e->buttons() & Qt::RightButton)) {
        d->updateHoverCursor(e->pos(), img);
    }

    // 中键随时可平移；左键平移只在 StatePan 下
    if ((e->buttons() & Qt::MiddleButton)
        && (d->state == StatePan || d->state == StateDraw)) {
        const qreal s = d->vp.scale(d->viewSize());
        if (s > 1e-6) {
            const QPointF delta = e->pos() - d->lastViewPos;
            d->vp.panImage(QPointF(-delta.x() / s, -delta.y() / s));
            d->applyTransform();
        }
        d->lastViewPos = e->pos();
        if (d->state == StatePan || !(e->buttons() & Qt::LeftButton)) {
            e->accept();
            return;
        }
    } else if (d->state == StatePan && (e->buttons() & Qt::LeftButton)) {
        const qreal s = d->vp.scale(d->viewSize());
        if (s > 1e-6) {
            const QPointF delta = e->pos() - d->lastViewPos;
            d->vp.panImage(QPointF(-delta.x() / s, -delta.y() / s));
            d->applyTransform();
        }
        d->lastViewPos = e->pos();
        e->accept();
        return;
    }

    // 画笔右键拖过起拖距离才转成擦除笔画；已在笔画中则继续涂抹
    if (d->brushRightArm && (e->buttons() & Qt::RightButton) && !(e->buttons() & Qt::MiddleButton)) {
        const QPoint delta = e->pos() - d->brushRightView;
        if (delta.manhattanLength() >= QApplication::startDragDistance()) {
            d->brushRightArm = false;
            d->beginBrushStroke(d->pressImage, true);
            d->moveBrushStroke(img);
        }
        e->accept();
        return;
    }

    if (d->brushStroke && (e->buttons() & (Qt::LeftButton | Qt::RightButton))) {
        d->moveBrushStroke(img);
        e->accept();
        return;
    }

    // 编辑：以按下时的图形为基准整体平移或拖柄，实时发 roiSelected
    if (d->state == StateEditHandle && (e->buttons() & Qt::LeftButton) &&
        (d->activeHandle >= 0 || d->activeHandle == kHandleBody)) {
        GraphicItem* item = d->drawingById(d->activeId);
        if (item) {
            HsCommData::Graphic g = d->pressGraphic;
            if (d->activeHandle == kHandleBody) {
                translateGraphic(&g, img - d->pressImage);
            } else {
                applyHandleDrag(&g, d->activeHandle, img);
            }
            item->setGraphic(g);
            viewport()->update();
            emit roiSelected(item->graphic());
        }
        e->accept();
        return;
    }

    // 橡皮筋：按工具类型用当前点更新预览几何
    if (d->state == StateDraw && d->rubberActive) {
        GraphicItem* item = d->drawingById(d->activeId);
        if (item) {
            HsCommData::Graphic g = item->graphic();
            switch (d->tool) {
            case ToolMeasureRadius:
            case ToolCircle:
                g.geom.circleAt(0)->r = QLineF(toQ(g.geom.circleAt(0)->c), img).length();
                break;
            case ToolMeasureRect:
            case ToolRect1:
                g.geom.rect1At(0)->r = fromQ(QRectF(d->pressImage, img).normalized());
                break;
            case ToolRect2: {
                const QPointF delta = img - toQ(g.geom.rect2At(0)->c);
                g.geom.rect2At(0)->setAngRad(qAtan2(delta.y(), delta.x()));
                g.geom.rect2At(0)->l1 = qMax(1.0, QLineF(toQ(g.geom.rect2At(0)->c), img).length());
                g.geom.rect2At(0)->l2 = qMax(1.0, g.geom.rect2At(0)->l1 * 0.6);
                break;
            }
            case ToolMeasureEllipse:
            case ToolEllipse: {
                g.geom.ellipseAt(0)->ra = qMax(1.0, qAbs(img.x() - g.geom.ellipseAt(0)->c.x));
                g.geom.ellipseAt(0)->rb = qMax(1.0, qAbs(img.y() - g.geom.ellipseAt(0)->c.y));
                break;
            }
            case ToolMeasureDist:
            case ToolLine:
                g.geom.lineAt(0)->b = fromQ(img);
                break;
            case ToolCross: {
                const qreal len = QLineF(d->pressImage, img).length();
                if (!g.cross.empty()) {
                    g.cross[0].size = (len >= 2.0) ? len : d->crossSize;
                }
                break;
            }
            case ToolMeasureAngle:
            case ToolContour:
            case ToolPolygon: {
                QVector<QPointF> pts = d->polyPts;
                pts.push_back(img);
                g.geom.contourAt(0)->setPts(toPts(pts));
                break;
            }
            default:
                break;
            }
            g.updateBounds();
            item->setGraphic(g);
            viewport()->update();
            emit roiSelected(item->graphic());
        }
        e->accept();
        return;
    }

    e->accept();
}

// 松开：结束平移 / 编辑 / 橡皮筋，并提交或丢弃 ROI
void HsImageWindow::mouseReleaseEvent(QMouseEvent* e)
{
    d->updateCursor(e->pos());

    // 画笔右键没拖动就松开，视为提交会话
    if (d->brushRightArm && e->button() == Qt::RightButton) {
        d->brushRightArm = false;
        d->commitSession();
        e->accept();
        return;
    }

    if (d->brushStroke) {
        const bool endStroke = d->brushErase
            ? (e->button() == Qt::RightButton)
            : (e->button() == Qt::LeftButton);
        if (endStroke) {
            d->brushStroke = false;
            e->accept();
            return;
        }
    }

    if (d->state == StatePan) {
        d->state = d->activeId.isEmpty() ? StateBrowse : StateEditHandle;
        d->updateHoverCursor(e->pos(), widgetToImage(e->pos()));
        e->accept();
        return;
    }

    // 编辑结束：非会话时发 roiCommitted，并以新图形作下次拖动基准
    if (d->state == StateEditHandle &&
        (d->activeHandle >= 0 || d->activeHandle == kHandleBody)) {
        GraphicItem* item = d->drawingById(d->activeId);
        if (item && !d->sessionActive) {
            emit roiCommitted(item->graphic());
            d->pressGraphic = item->graphic();
        } else if (item) {
            d->pressGraphic = item->graphic();
        }
        d->activeHandle = -1;
        d->updateHoverCursor(e->pos(), widgetToImage(e->pos()));
        e->accept();
        return;
    }

    if (d->state == StateDraw && d->rubberActive &&
        d->tool != ToolContour && d->tool != ToolPolygon && d->tool != ToolMeasureAngle) {
        GraphicItem* item = d->drawingById(d->activeId);
        if (!item) {
            d->cancelDraw();
            e->accept();
            return;
        }
        HsCommData::Graphic g = item->graphic();
        if (d->sessionActive) {
            // 会话中松手只留草稿，等右键或 Enter 提交；拖得太小则取消会话
            if (d->finishPreviewIfTiny(g)) {
                d->cancelSession();
            } else {
                d->rubberActive = false;
                d->sessionDraft = true;
                d->state = StateEditHandle;
                d->activeHandle = -1;
                d->pressGraphic = g;
                d->setSelectedDrawing(d->activeId);
            }
            e->accept();
            return;
        }
        // 非会话：去掉预览，按工具前缀分配 id 后提交；太小则丢弃
        d->removeDrawing(d->activeId);
        d->rubberActive = false;
        if (d->finishPreviewIfTiny(g)) {
            d->state = StateBrowse;
            if (d->sessionActive) {
                d->cancelSession();
            }
        } else {
            if (d->tool == ToolMeasureDist) {
                g.id = fromQ(d->nextId(QStringLiteral("m.dist")));
            } else if (d->tool == ToolMeasureRadius) {
                g.id = fromQ(d->nextId(QStringLiteral("m.rad")));
            } else if (d->tool == ToolMeasureRect) {
                g.id = fromQ(d->nextId(QStringLiteral("m.rect")));
            } else if (d->tool == ToolMeasureEllipse) {
                g.id = fromQ(d->nextId(QStringLiteral("m.ell")));
            } else {
                g.id.clear();
            }
            d->commitGraphic(g);
        }
        e->accept();
        return;
    }

    e->accept();
}

// 左键双击适应窗口；会话中忽略，有橡皮筋先取消
void HsImageWindow::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) {
        e->accept();
        return;
    }
    if (d->sessionActive) {
        e->accept();
        return;
    }
    if (d->rubberActive) {
        d->cancelDraw(false);
    }
    d->activeHandle = -1;
    d->state = StateBrowse;
    if (!d->vp.imageSize.isEmpty()) {
        fitToWindow();
    }
    e->accept();
}

// 快捷键：Ctrl 像素小窗、Esc 取消、Enter 提交、Delete 删除、Backspace 撤点
void HsImageWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Control) {
        d->updateCtrlPixelPopup(d->cursorViewPos);
        e->accept();
        return;
    }
    // Esc：取消会话或橡皮筋，都没有则取消选中并回到浏览
    if (e->key() == Qt::Key_Escape) {
        if (d->sessionActive) {
            d->cancelSession();
        } else if (d->rubberActive) {
            d->cancelDraw();
        } else {
            d->setSelectedDrawing(QString());
            d->state = StateBrowse;
            d->tool = ToolBrowse;
        }
        viewport()->update();
        e->accept();
        return;
    }
    // 画笔会话：Ctrl+Z 撤销一笔，Delete 清空墨迹
    if (d->sessionActive && d->tool == ToolBrush && e->key() == Qt::Key_Z
        && (e->modifiers() & Qt::ControlModifier)) {
        d->undoBrushStroke();
        e->accept();
        return;
    }
    if (d->sessionActive && d->tool == ToolBrush && e->key() == Qt::Key_Delete) {
        d->clearBrushInk();
        e->accept();
        return;
    }
    // 非会话：Delete 只删选中的测量
    if (!d->sessionActive && e->key() == Qt::Key_Delete && d->measureItems.contains(d->activeId)) {
        d->removeDrawing(d->activeId);
        d->activeId.clear();
        d->activeHandle = -1;
        d->state = StateBrowse;
        viewport()->unsetCursor();
        viewport()->update();
        emit roiSelected(HsCommData::Graphic());
        e->accept();
        return;
    }
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && d->sessionActive) {
        d->commitSession();
        e->accept();
        return;
    }
    // 多边形 / 轮廓：Backspace 撤最后一点，撤空则取消；Enter 收尾
    if (e->key() == Qt::Key_Backspace && d->rubberActive &&
        (d->tool == ToolContour || d->tool == ToolPolygon)) {
        if (!d->polyPts.isEmpty()) {
            d->polyPts.pop_back();
        }
        if (d->polyPts.isEmpty()) {
            if (d->sessionActive) {
                d->cancelSession();
            } else {
                d->cancelDraw();
            }
        } else {
            GraphicItem* item = d->drawingById(d->activeId);
            if (item) {
                HsCommData::Graphic g = item->graphic();
                g.geom.contourAt(0)->setPts(toPts(d->polyPts));
                item->setGraphic(g);
            }
        }
        e->accept();
        return;
    }
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) &&
        d->rubberActive && (d->tool == ToolContour || d->tool == ToolPolygon)) {
        d->finishPolyline();
        e->accept();
        return;
    }
    QGraphicsView::keyPressEvent(e);
}

// 松开 Ctrl 时隐藏像素小窗
void HsImageWindow::keyReleaseEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Control) {
        d->hideCtrlPixelPopup();
        e->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(e);
}

// 光标离开：置无效、关十字线和像素小窗，并刷新 HUD
void HsImageWindow::leaveEvent(QEvent* e)
{
    d->cursorValid = false;
    d->crosshairOn = false;
    d->hideCtrlPixelPopup();
    if (d->hudVisible || d->pixelInspect || d->sessionActive) {
        viewport()->update();
    }
    QGraphicsView::leaveEvent(e);
}

// 右键菜单：会话中、刚被右键收尾吃掉、或多边形未闭合时不弹；命中测量时带上其 id
void HsImageWindow::contextMenuEvent(QContextMenuEvent* e)
{
    if (d->sessionActive) {
        d->skipNextContextMenu = false;
        e->accept();
        return;
    }
    if (d->skipNextContextMenu) {
        d->skipNextContextMenu = false;
        e->accept();
        return;
    }
    if (d->rubberActive && (d->tool == ToolPolygon || d->tool == ToolContour)) {
        e->accept();
        return;
    }

    QString measureId;
    GraphicItem* hit = d->hitDrawing(widgetToImage(e->pos()));
    if (hit && d->measureItems.contains(toQ(hit->graphic().id))) {
        measureId = toQ(hit->graphic().id);
    }
    QList<QAction*> extras;
    emit contextMenuExtrasRequested(&extras);
    HsImageWindowMenu::popup(this, e->globalPos(), !d->frame.isNull(), d->vp.isValid(),
                             d->rubberActive, measureId, extras);
}

}

#ifdef HSIMAGE_MOC_FROM_INTDIR
// 用宏展开，避免 Linux AUTOMOC 把这行当作 moc 源并去 src/display 找头文件
#define HSIMAGE_MOC_HSIMAGEWINDOW "moc_HsImageWindow.cpp"
#include HSIMAGE_MOC_HSIMAGEWINDOW
#endif

