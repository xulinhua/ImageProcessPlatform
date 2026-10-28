/**
 * @file HsImageWindow.cpp
 * @brief HsImageWindow???
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

// ??????飺?????????????????????????????????
static const int kHandleBody = -2;          ///< ???????壨???????
static const qreal kInspectMinCellPx = 28.0; ///< ??????????С?????????????
static const int kInspectMaxCells = 1600;    ///< ???????????????
static const int kInspectNeighborHalf = 3;   ///< ????????????
static const int kInspectNeighborCell = 22;  ///< ?????????磨?????
static const int kInspectNeighborCellRgbW = 36; ///< ????????????????
static const int kInspectNeighborCellRgbH = 42; ///< ???????????????
static const qreal kZoomInFactor = 1.25;  ///< ??????????????
static const qreal kZoomOutFactor = 0.8;  ///< ??С?????????????

// ?????????????????
enum OverlayIconKind {
    IconSnap = 1,   ///< ???????
    IconZoomIn,     ///< ???
    IconZoomOut,    ///< ??С
    IconFit,        ///< ???????
    IconOne,        ///< ???????
    IconDist,       ///< ???????
    IconAngle,      ///< ????н?
    IconRadius,     ///< ???
    IconRect,       ///< ???ο???
    IconEllipse     ///< ???????
};

// ?????????? 18x18 ?????????
static QIcon paintOverlayIcon(int kind, const QColor& color)
{
    QPixmap pm(18, 18);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
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

// ?? HsCommData::Graphic ???? Qt ??????????????
void registerGraphicMetaTypes()
{
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    qRegisterMetaType<HsCommData::Graphic>("HsCommData::Graphic");
}

enum InteractionState {
    StateBrowse = 0, ///< ????
    StatePan,        ///< ???
    StateDraw,       ///< ?????????? ROI
    StateEditHandle  ///< ??????? ROI ?????
};

// ???????????? ROI id ??
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

// ??????????? GraphicKind
static HsCommData::GraphicKind toolKind(DrawTool t)
{
    switch (t) {
    case ToolCross: return HsCommData::KindCross;
    case ToolText:  return HsCommData::KindText;
    default:        return HsCommData::KindObject;
    }
}

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

// ??????????
static bool measureTool(DrawTool t)
{
    return t == ToolMeasureDist || t == ToolMeasureAngle || t == ToolMeasureRadius
        || t == ToolMeasureRect || t == ToolMeasureEllipse;
}

static bool measureItemId(const QString& id)
{
    return id.startsWith(QLatin1String("m.dist"))
        || id.startsWith(QLatin1String("m.ang"))
        || id.startsWith(QLatin1String("m.rad"))
        || id.startsWith(QLatin1String("m.rect"))
        || id.startsWith(QLatin1String("m.ell"));
}


// ????????κ?????????
static QString measureLabel(DrawTool hint, bool preview, const HsCommData::Graphic& g,
                            const HsCommData::ImageCalib& cal)
{
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

// ???????????????豸????????ê???????????? map?????
// ???????任???????Σ??????????????????????????????????????
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


// ?? Gray8 ??????????? RGB
static bool inspectIsColor(const HsCommData::HsImage& frame)
{
    return frame.encoding != HsCommData::EncodeMono;
}

// ??????????Э?? Point2d
static std::vector<HsCommData::Point2d> toPts(const QVector<QPointF>& v)
{
    std::vector<HsCommData::Point2d> o;
    o.reserve(static_cast<size_t>(v.size()));
    for (int i = 0; i < v.size(); ++i) {
        o.push_back(fromQ(v[i]));
    }
    return o;
}

// ???????????????????????????
static QColor inspectTextColor(int r, int g, int b)
{
    const int y = (r * 299 + g * 587 + b * 114) / 1000;
    return (y > 160) ? QColor(20, 20, 20) : QColor(245, 245, 245);
}


// ??????? HsCommData::Graphic???? HsCommData::HsRegion ???
static void translateGraphic(HsCommData::Graphic* g, const QPointF& delta)
{
    if (!g || (qFuzzyIsNull(delta.x()) && qFuzzyIsNull(delta.y()))) {
        return;
    }
    const HsCommData::Point2d d = fromQ(delta);
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

// ?????????Σ?pos ????????
static void applyHandleDrag(HsCommData::Graphic* g, int handle, const QPointF& pos)
{
    if (!g) {
        return;
    }
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
    HsImageWindow* q; ///< ??????
    QGraphicsScene scene; ///< ??????????????????? 1:1
    ImageLayer imageLayer;
    StackLayer stackObj;
    DrawingLayer drawingObj;
    MeasureLayer measureObj;
    ForegroundLayer foreObj;
    ImageItem* imageItem; ///< ???
    LayerItem* stackLayer; ///< Stack ???????
    LayerItem* drawingLayer; ///< Drawing ???????
    LayerItem* measureLayer; ///< ???????????
    QHash<QString, GraphicItem*>& stackItems; ///< Stack ????
    QHash<QString, GraphicItem*>& drawingItems; ///< Drawing ????
    QHash<QString, GraphicItem*>& measureItems; ///< ????????
    HsCommData::HsImage& frame; ///< ??????
    std::shared_ptr<const HsCommData::FrameOverlay>& overlay; ///< Stack Э??
    Viewport vp; ///< ?????????????
    WindowMode mode; ///< ???/????
    DrawTool tool; ///< ???????
    InteractionState state; ///< ??????

    QString activeId; ///< ??л????????? id
    int activeHandle; ///< ????±?kHandleBody ????????
    QPoint lastViewPos; ///< ??????????????
    QPointF pressImage; ///< ??????????????
    HsCommData::Graphic pressGraphic; ///< ??????????????ο???
    bool rubberActive; ///< ??????? ROI
    QVector<QPointF> polyPts; ///< ????/????????е?
    bool skipNextContextMenu; ///< ???????????????????

    bool& hudVisible; ///< HUD
    bool pixelInspect; ///< ???????
    bool live; ///< ?????
    bool& stackVisible; ///< Stack ????????
    bool& drawingVisible; ///< Drawing ????????
    HsCommData::GeomStyle& penStyle; ///< ?? ROI ????
    HsCommData::GeomStyle& overlayStyle; ///< disp* ???? Stack ??????
    QString textDraft; ///< ?????????????
    DrawInfoSpace textSpace; ///< ????????????
    int textFontPx; ///< ???????????????????
    double crossSize; ///< ???????С?? 0 ?????????
    QPointF cursorPt; ///< ??????????
    QPoint cursorViewPos; ///< ?????????
    double cursorGray; ///< ??????
    bool cursorValid; ///< ????????Ч
    QVector<DrawInfoEntry>& imageDrawInfos; ///< ??????????????
    QVector<DrawInfoEntry>& windowDrawInfos; ///< ???????????????
    QString& imageInfoText; ///< HUD ???????
    QLabel* pixelPopup;    ///< Ctrl ?????????????С??
    enum DepthProbeKind {
        DepthProbeNone = 0,
        DepthProbeU16,  ///< ????
        DepthProbeF32,  ///< ??
        DepthProbeF64   ///< ??
    };
    DepthProbeKind depthProbeKind; ///< Ctrl С????????????
    int depthProbeW;               ///< ????
    int depthProbeH;               ///< ????
    std::vector<uint16_t> depthProbeU16;
    std::vector<float> depthProbeF32;
    std::vector<double> depthProbeF64;
    QImage depthProbeGray; ///< ????????????????????????
    QWidget* overlayBar;           ///< ????????????????????????
    QBoxLayout* overlayBarLayout;  ///< ????????
    Qt::Corner overlayBarCorner;   ///< ??????
    Qt::Orientation overlayBarOrient; ///< ??????з???
    OverlayToolbarSkin overlayBarSkin; ///< ?????????
    std::vector<DrawTool> measureShown; ///< ???????????????????
    QButtonGroup* measureGroup;         ///< ???????????
    QWidget* measureBar;                ///< ??????????
    QBoxLayout* measureBarLayout;       ///< ??????????
    bool measureBarWant;                ///< ????????????????????
    ViewportBackground vpBg;       ///< ?????????
    QColor vpBgCustom;             ///< Custom / ??????????Ч=??????
    ImageRefreshPolicy imageRefresh; ///< ??????????? Stack / ROI / ????

    QElapsedTimer fpsClock; ///< Overlay fps ???
    int fpsCount; ///< ??????????
    qreal fps; ///< Overlay Hz
    HsCommData::ImageCalib imageCalib; ///< ????????????1:1
    int idSeq; ///< ROI id ???

    bool sessionActive; ///< ????????
    bool sessionDraft; ///< ?????????????д??????????????????
    bool crosshairOn; ///< ?????????????????????
    double brushRadius; ///< ??????????????????λС??
    int brushW;
    int brushH;
    qint64 brushCount; ///< ??????? 0 ???????
    bool brushStroke; ///< ??????
    bool brushErase;
    bool brushRightArm; ///< ?????????????????????????????????????????
    QPoint brushRightView; ///< ?????????????????
    QPoint brushLastPx;
    QPointF brushRingPt;
    std::vector<unsigned char> brushBuf;
    QImage brushOverlay;
    QVector<std::vector<unsigned char> > brushUndo;

    // ?????????????????
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
        // ???? scene?????? QGraphicsView ?? item ??Χ????????У????? part ???
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

    // ??????????????????????????
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

    // ???????????????????????????
    void promptSaveSnapshot()
    {
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const QString dir = pictures.isEmpty() ? QDir::homePath() : pictures;
        const QString selected = QFileDialog::getSaveFileName(
            q,
            QString::fromUtf8("???????"),
            QDir(dir).filePath(QStringLiteral("snapshot_%1.png").arg(stamp)),
            QStringLiteral("PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp);;All files (*)"));
        if (selected.isEmpty()) {
            return;
        }
        QString err;
        if (!q->saveSnapshot(selected, &err)) {
            QMessageBox::warning(q, QString::fromUtf8("???????"), err);
        }
    }

    // ????????????????????????
    void promptSaveOriginal()
    {
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const QString dir = pictures.isEmpty() ? QDir::homePath() : pictures;
        const QString selected = QFileDialog::getSaveFileName(
            q,
            QString::fromUtf8("??????"),
            QDir(dir).filePath(QStringLiteral("image_%1.png").arg(stamp)),
            QStringLiteral("PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp);;All files (*)"));
        if (selected.isEmpty()) {
            return;
        }
        QString err;
        if (!q->saveCurrentFrame(selected, &err)) {
            QMessageBox::warning(q, QString::fromUtf8("???????"), err);
        }
    }

    // ??????????????????????????
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

    // ????????????????а??
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

    // ?????????????????
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

    // ??????????????????????????
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

    // ?? measureShown ??????????????????
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
        for (size_t i = 0; i < measureShown.size(); ++i) {
            const DrawTool item = measureShown[i];
            int kind = 0;
            QString tip;
            switch (item) {
            case ToolMeasureDist:
                kind = IconDist;
                tip = QString::fromUtf8("???????");
                break;
            case ToolMeasureAngle:
                kind = IconAngle;
                tip = QString::fromUtf8("????н?");
                break;
            case ToolMeasureRadius:
                kind = IconRadius;
                tip = QString::fromUtf8("???");
                break;
            case ToolMeasureRect:
                kind = IconRect;
                tip = QString::fromUtf8("???ο???");
                break;
            case ToolMeasureEllipse:
                kind = IconEllipse;
                tip = QString::fromUtf8("???????");
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

    // ????????????????????????????????????
    void syncMeasureBar()
    {
        if (!measureBar) {
            return;
        }
        measureBar->setVisible(measureBarWant && !measureShown.empty());
        placeOverlayToolbar();
    }

    // ?????????????????
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
        QToolButton* snap = makeOverlayBtn(overlayBar, overlayBarLayout, IconSnap, QString::fromUtf8("?????????????????"));
        QToolButton* zin = makeOverlayBtn(overlayBar, overlayBarLayout, IconZoomIn, QString::fromUtf8("???"));
        QToolButton* zout = makeOverlayBtn(overlayBar, overlayBarLayout, IconZoomOut, QString::fromUtf8("??С"));
        QToolButton* fit = makeOverlayBtn(overlayBar, overlayBarLayout, IconFit, QString::fromUtf8("????????????"));
        QToolButton* one = makeOverlayBtn(overlayBar, overlayBarLayout, IconOne, QString::fromUtf8("???????"));
        QObject::connect(snap, &QToolButton::clicked, q, [this]() { promptSaveSnapshot(); });
        QObject::connect(zin, &QToolButton::clicked, q, [this]() { q->zoomIn(); });
        QObject::connect(zout, &QToolButton::clicked, q, [this]() { q->zoomOut(); });
        QObject::connect(fit, &QToolButton::clicked, q, [this]() { q->fitToWindow(); });
        QObject::connect(one, &QToolButton::clicked, q, [this]() { q->resetView(); });
        rebuildMeasureButtons();
        // ??????????/????????? setOverlayToolbarVisible(true)
        overlayBar->hide();
        placeOverlayToolbar();
    }

    // ?????д???????????????????
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

    // ???????????????????????
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

    // ??????????????????????????????
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

    // ????????????? fit
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

    // ????????????????????????????? QPixmap
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

    // ???????????????`Stack / ???? ROI / ????
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
     * @brief ??????? Drawing?????? ROI ?? HsCommData::KindText ???????????????????
     * @param dropGeom ????????? ROI
     * @param dropText ??? HsCommData::KindText
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

    // ??? Ctrl ????С??
    void hideCtrlPixelPopup()
    {
        if (pixelPopup) {
            pixelPopup->hide();
        }
    }

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

    // Ctrl ??????????????/??????????????? RGB
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

        QString type_label = QString::fromUtf8("???");
        QString value_text;
        QString raw_depth_text;
        QString extra_value_text;
        const QString extra_label = QString::fromUtf8("??????");
        const size_t idx = static_cast<size_t>(pix.y()) * static_cast<size_t>(depthProbeW) +
            static_cast<size_t>(pix.x());

        if (depthProbeKind == DepthProbeU16 && depthProbeW > 0 && depthProbeH > 0 &&
            pix.x() >= 0 && pix.y() >= 0 && pix.x() < depthProbeW && pix.y() < depthProbeH &&
            idx < depthProbeU16.size())
        {
            const uint16_t depth = depthProbeU16[idx];
            type_label = QString::fromUtf8("???");
            value_text = depth == 0 ? QString::fromUtf8("??Ч")
                                   : QString::fromUtf8("%1 mm").arg(depth);
            raw_depth_text = depth == 0 ? QString::fromUtf8("??Ч") : QString::number(depth);
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
            type_label = QString::fromUtf8("???");
            if (!std::isfinite(depth) || depth <= 0.0f) {
                value_text = QString::fromUtf8("??Ч");
                raw_depth_text = QString::fromUtf8("??Ч");
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
            type_label = QString::fromUtf8("???");
            if (!std::isfinite(depth) || depth <= 0.0) {
                value_text = QString::fromUtf8("??Ч");
                raw_depth_text = QString::fromUtf8("??Ч");
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
                text = QString::fromUtf8("X:%1  Y:%2\n(???): %3")
                           .arg(pix.x()).arg(pix.y()).arg(qRound(gray));
            }
        } else {
            text = QString::fromUtf8("X:%1  Y:%2\n?(%3): %4")
                       .arg(pix.x()).arg(pix.y()).arg(type_label).arg(value_text);
            if (!raw_depth_text.isEmpty()) {
                text += QString::fromUtf8("\n?(?????): %1").arg(raw_depth_text);
            }
            if (!extra_value_text.isEmpty()) {
                text += QString::fromUtf8("\n?(%1): %2").arg(extra_label).arg(extra_value_text);
            }
        }
        pixelPopup->setText(text);
        pixelPopup->adjustSize();
        pixelPopup->move(q->mapToGlobal(viewPos) + QPoint(16, 0));
        pixelPopup->show();
    }

    // ??????????
    QSize viewSize() const
    {
        return q->viewport()->size();
    }

    // ?? part д?? QTransform??????????????????????
    void applyTransform()
    {
        q->setTransform(vp.viewTransform(viewSize()));
        // ?????????????? View ?????part ???????????ROI ???????????
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

    // ??????????ê??????????????????
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

    // ??? Overlay ?????????? 1s ???? fps
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

    // frameId ??????????? Stack
    void syncStackVisibility()
    {
        stackObj.syncVisibility(frame);
    }

    // ??????? ROI id
    QString nextId(const QString& prefix)
    {
        ++idSeq;
        return prefix + QLatin1Char('.') + QString::number(idSeq);
    }

    // ??id ??Drawing Item??????? 0
    WindowLayer layerOfId(const QString& id) const
    {
        if (measureItemId(id) || (id.startsWith(QLatin1String("preview")) && measureTool(tool))) {
            return LayerMeasure;
        }
        return LayerDrawing;
    }

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

    GraphicItem* drawingById(const QString& id) const
    {
        GraphicItem* item = measureItems.value(id, 0);
        if (item) {
            return item;
        }
        return drawingItems.value(id, 0);
    }

    // ?л???У????????????????? id ??? activeId
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

    // ??id ????????Drawing Item
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

    // ???????? Drawing Item
    void removeDrawing(const QString& id)
    {
        if (measureObj.contains(id)) {
            measureObj.remove(id);
            return;
        }
        drawingObj.remove(id);
    }

    // ?? overlay ??? Stack Item
    void rebuildStack()
    {
        stackObj.rebuild(vp.scale(viewSize()));
        syncStackVisibility();
    }

    // disp*?????????? Stack??frameId ???????????????????
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

    // ???????? 8 ???????????????????
    qreal imageTol() const
    {
        const qreal s = vp.scale(viewSize());
        return (s > 1e-6) ? (8.0 / s) : 8.0;
    }

    // ??????任????????????
    QTransform sceneToView() const
    {
        return q->viewportTransform();
    }

    // ????????е????
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

    // ???????????? Drawing
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

    // ?????????????????
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

    // ?????????????
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

    // ????Stack ??
    GraphicItem* hitStack(const QPointF& scenePos) const
    {
        return stackObj.hit(scenePos, imageTol());
    }

    // ??????????? HsCommData::Graphic
    HsCommData::Graphic makePreview(DrawTool tool, const QPointF& p0) const
    {
        HsCommData::Graphic g;
        g.kind = toolKind(tool);
        g.role = HsCommData::RoleDrawing;
        g.geomStyle = penStyle;
        g.frameId = frame.frameId;
        g.visible = true;
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

    // ??????С????
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

    // ?? id??д?? Drawing ?????? roiCommitted
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

    // ????????????????????? Drawing??????????? drawInfo
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

    // ?????????????в???????????
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

    // ????α??? HsCommData::HsRegion ????
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

    // ?????????????????? closePolygon
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

    // ??????????HUD
    // ???????????????????
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

    // ?????????飺????????
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

    // ?????? RGB/??????
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

    // ????????????????????????
    void drawPixelInspect(QPainter* painter)
    {
        // ???????????????????????????????????? HsCommData::HsImage?????? LOD
        if (!pixelInspect || frame.isNull()) {
            return;
        }

        const QTransform xf = painter->worldTransform();
        painter->save();
        painter->resetTransform();
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setRenderHint(QPainter::TextAntialiasing, false);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

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

    QColor brushRingColor() const
    {
        HsCommData::Color4 c = penStyle.stroke.isUnset() ? penStyle.fill : penStyle.stroke;
        if (c.isUnset()) {
            return QColor(0xFA, 0xAD, 0x14);
        }
        return QColor(c.r, c.g, c.b);
    }

    QPoint imagePixel(const QPointF& img) const
    {
        return QPoint(qFloor(img.x()), qFloor(img.y()));
    }

    void repaintImageRect(const QRect& imgRect)
    {
        if (!imgRect.isValid() || imgRect.isEmpty()) {
            return;
        }
        const QPointF tl = q->imageToWidget(QPointF(imgRect.left(), imgRect.top()));
        const QPointF br = q->imageToWidget(QPointF(imgRect.right() + 1, imgRect.bottom() + 1));
        q->viewport()->update(QRectF(tl, br).normalized().toAlignedRect().adjusted(-2, -2, 2, 2));
    }

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

    void moveBrushStroke(const QPointF& img)
    {
        if (!brushStroke) {
            return;
        }
        const QPoint px = imagePixel(img);
        stampBetween(brushLastPx, px, brushErase);
        brushLastPx = px;
    }

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

    // ????δ????????????????????λ???
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

// ??????????????????????=????
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

// ??? Pimpl
HsImageWindow::~HsImageWindow()
{
    if (d) {
        delete d;
        d = nullptr;
    }
}

// ????????????????????????棬????????
void HsImageWindow::setImage(const uint8_t* data, int width, int height, int strideBytes,
                             const QString& encoding)
{
    if (!data || width <= 0 || height <= 0 || strideBytes <= 0) {
        clearImage();
        return;
    }
    const qint64 id = (d->frame.frameId != 0) ? d->frame.frameId : 1;
    const HsCommData::HsImage fb = HsCommData::HsImage::fromRaw(data, width, height, strideBytes,
                                                encoding.toLatin1().constData(), id);
    if (fb.isNull()) {
        return;
    }
    // ??????????????????????? clearDepthProbe / setDepthProbe* ???????
    const QSize oldSize = d->vp.imageSize;
    d->setFrame(fb);
    d->abandonSessionIfImageChanged(oldSize);
    d->applyRefreshAfterNewImage();
}

void HsImageWindow::setPreparedImage(const QImage& image)
{
    if (!d || image.isNull()) {
        return;
    }
    d->setPrepared(image);
}

// setImage ?????
void HsImageWindow::setImageRaw(const uint8_t* data, int width, int height, int strideBytes,
                                const QString& encoding)
{
    setImage(data, width, height, strideBytes, encoding);
}

// OpenCV Mat ????
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

// ???? JPEG/PNG ?????????????
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

// ??????????? Stack / Drawing
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

// ????е??
bool HsImageWindow::hasImage() const
{
    return !d->frame.isNull();
}

// ????????????
QSize HsImageWindow::currentImageSize() const
{
    return d->frame.isNull() ? QSize() : bufferPixelSize(d->frame);
}

// currentImageSize ?????
QSize HsImageWindow::imageSize() const
{
    return currentImageSize();
}

// ?????????
qint64 HsImageWindow::currentFrameId() const
{
    return d->frame.isNull() ? 0 : d->frame.frameId;
}

// ??????????
void HsImageWindow::setOverlayColor(const QColor& color)
{
    d->overlayStyle.stroke = fromQ(color);
    QColor fill = color;
    fill.setAlpha(40);
    d->overlayStyle.fill = fromQ(fill);
}

// ???????
void HsImageWindow::setOverlayWidth(double widthPx)
{
    d->overlayStyle.widthPx = qMax(0.5, widthPx);
}

// ?????? Stack
void HsImageWindow::dispCircle(double x, double y, double r)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom.circleAt(0)->c = HsCommData::Point2d(x, y);
    g.geom.circleAt(0)->r = r;
    d->appendStack(g);
}

// ?????? Stack??Э???????????
void HsImageWindow::dispCircle(const HsCommData::CircleP& circle)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = circle;
    d->appendStack(g);
}

// ???????? Stack
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

// ???????? Stack??Э???????????
void HsImageWindow::dispEllipse(const HsCommData::EllipseP& ellipse)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = ellipse;
    d->appendStack(g);
}

// ??????????ε? Stack
void HsImageWindow::dispRect1(double left, double top, double width, double height)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = HsCommData::Rect1P();
    g.geom.rect1At(0)->r = HsCommData::Box2(left, top, width, height);
    d->appendStack(g);
}

// ??????????ε? Stack??Э???????
void HsImageWindow::dispRect1(const HsCommData::Rect1P& rect)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = rect;
    d->appendStack(g);
}

// ??????????ε? Stack??HsCommData::Box2 ?????
void HsImageWindow::dispRect1(const HsCommData::Box2& box)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = HsCommData::Rect1P();
    g.geom.rect1At(0)->r = box;
    d->appendStack(g);
}

// ?????????ε? Stack
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

// ?????????ε? Stack??Э???????????
void HsImageWindow::dispRect2(const HsCommData::Rect2P& rect)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = rect;
    d->appendStack(g);
}

// ??????? Stack
void HsImageWindow::dispLine(double x1, double y1, double x2, double y2)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = HsCommData::LineP();
    g.geom.lineAt(0)->a = HsCommData::Point2d(x1, y1);
    g.geom.lineAt(0)->b = HsCommData::Point2d(x2, y2);
    d->appendStack(g);
}

// ??????? Stack??Э???????
void HsImageWindow::dispLine(const HsCommData::LineP& line)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = line;
    d->appendStack(g);
}

// ??????? Stack
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

// ??????? Stack???????
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

// ??????? Stack
void HsImageWindow::dispArrow(double x1, double y1, double x2, double y2)
{
    HsCommData::HsArrow arrow;
    arrow.tail = HsCommData::Point2d(x1, y1);
    arrow.head = HsCommData::Point2d(x2, y2);
    dispArrow(arrow);
}

// ??????? Stack
void HsImageWindow::dispArrow(const HsCommData::HsArrow& arrow)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindArrow;
    g.arrow.resize(1);
    g.arrow[0] = arrow;
    d->appendStack(g);
}

// ????г????? Stack
void HsImageWindow::dispRegion(const HsCommData::HsRegion& region)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = region;
    d->appendStack(g);
}

// ??? XLD / ????? Stack
void HsImageWindow::dispContour(const HsCommData::HsContour& contour)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.geom = contour;
    d->appendStack(g);
}

// ???????????frameId ?????????
void HsImageWindow::setOverlay(const std::shared_ptr<const HsCommData::FrameOverlay>& overlay)
{
    d->overlay = overlay;
    d->rebuildStack();
    d->noteFrame();
    viewport()->update();
}

// ??????б????? overlay
void HsImageWindow::setOverlay(const std::vector<HsCommData::Graphic>& items, int64_t frameId)
{
    std::shared_ptr<HsCommData::FrameOverlay> sc = HsCommData::FrameOverlay::create(frameId);
    sc->items = items;
    setOverlay(sc);
}

// ?滻 Stack??frameId ???????
void HsImageWindow::setOverlays(const std::vector<HsCommData::Graphic>& items)
{
    setOverlay(items, d->frame.frameId);
}

// ??? Stack Э??
std::shared_ptr<const HsCommData::FrameOverlay> HsImageWindow::overlay() const
{
    return d->overlay;
}

// ??? Stack
void HsImageWindow::clearWindow()
{
    d->overlay.reset();
    d->rebuildStack();
    viewport()->update();
}

// clearWindow ?????
void HsImageWindow::clearOverlays()
{
    clearWindow();
}

// Stack ????????????? Drawing
void HsImageWindow::setOverlayVisible(bool visible)
{
    setStackVisible(visible);
}

// Stack ????????
bool HsImageWindow::isOverlayVisible() const
{
    return d->stackVisible;
}

// ???????? Drawing ?? ROI
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

// ??? Drawing ????? ROI
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

// ????????
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
// ???????????????
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
// ??? Drawing?????????????????
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

// ??????????????????????????????
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

// ?????? ROI???????? id ???
HsCommData::Graphic HsImageWindow::selectedDrawing() const
{
    GraphicItem* item = d->drawingById(d->activeId);
    return item ? item->graphic() : HsCommData::Graphic();
}

// ??? ROI ????????????????????
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

// ???????
HsCommData::GeomStyle HsImageWindow::drawingStyle() const
{
    return d->penStyle;
}

// ???? ROI ???????
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

// ???? ROI ???????
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

// ?????????
void HsImageWindow::setImageVisible(bool visible)
{
    d->imageItem->setVisible(visible);
}

// ??????????
bool HsImageWindow::isImageVisible() const
{
    return d->imageItem->isVisible();
}

// ?????????е??????? part
QRectF HsImageWindow::part() const
{
    return d->vp.part;
}

// ?????????β?????????
void HsImageWindow::setPart(const QRectF& part)
{
    d->vp.part = part;
    d->vp.syncAspect(d->viewSize());
    d->applyTransform();
    viewport()->update();
}

// ????????????
void HsImageWindow::fitToWindow()
{
    d->vp.fit(d->viewSize());
    d->applyTransform();
    viewport()->update();
}

// 1:1 ??????????????????????
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

// ???? zoomActualSize
void HsImageWindow::resetView()
{
    zoomActualSize();
}

// ??????????ê???
void HsImageWindow::zoomIn()
{
    d->zoomBy(kZoomInFactor);
}

// ??????????ê??С
void HsImageWindow::zoomOut()
{
    d->zoomBy(kZoomOutFactor);
}

// ??????????????? / ????????
qreal HsImageWindow::zoomScale() const
{
    return d->vp.scale(d->viewSize());
}

// ??????? ROI??????????? Drawing
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

// ?????????
WindowMode HsImageWindow::windowMode() const
{
    return d->mode;
}

// ?л???????????????????^
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

// д?????????????е??λ??1:1
void HsImageWindow::setImageCalib(const HsCommData::ImageCalib& calib)
{
    d->imageCalib = calib;
    if (viewport()) {
        viewport()->update();
    }
}

// ??????????
HsCommData::ImageCalib HsImageWindow::imageCalib() const
{
    return d->imageCalib;
}

// ??????????
DrawTool HsImageWindow::drawTool() const
{
    return d->tool;
}

bool HsImageWindow::beginDrawSession(DrawTool tool)
{
    return d->beginSession(tool);
}

bool HsImageWindow::drawSessionActive() const
{
    return d->sessionActive;
}

void HsImageWindow::commitDrawSession()
{
    d->commitSession();
}


// ???δ?????????
void HsImageWindow::cancelActiveDraw()
{
    d->cancelDraw();
    if (viewport()) {
        viewport()->update();
    }
}

// ???????????????
void HsImageWindow::promptSaveSnapshot()
{
    d->promptSaveSnapshot();
}

// ???????????????
void HsImageWindow::promptSaveOriginal()
{
    d->promptSaveOriginal();
}
void HsImageWindow::cancelDrawSession()
{
    d->cancelSession();
}

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

double HsImageWindow::brushRadius() const
{
    return d->brushRadius;
}

// ???????????????
void HsImageWindow::setDrawText(const QString& text)
{
    d->textDraft = text;
}

// ??????????????
QString HsImageWindow::drawText() const
{
    return d->textDraft;
}

// ????????????
void HsImageWindow::setTextToolSpace(DrawInfoSpace space)
{
    d->textSpace = space;
}

// ??????????????
DrawInfoSpace HsImageWindow::textToolSpace() const
{
    return d->textSpace;
}

// ??????????
void HsImageWindow::setTextFontPx(int px)
{
    d->textFontPx = qMax(1, px);
}

// ????????????
int HsImageWindow::textFontPx() const
{
    return d->textFontPx;
}

// ????????
void HsImageWindow::setCrossSize(double size)
{
    d->crossSize = size;
}

// ???????????
double HsImageWindow::crossSize() const
{
    return d->crossSize;
}

// ?? id ??????????? Drawing
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

// ??????
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

// ????????
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

// ????????????
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

// ????????????
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

// ????????
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

// ????????
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

// ????м??
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

// HUD??????????????????
void HsImageWindow::setHudVisible(bool visible)
{
    d->hudVisible = visible;
    viewport()->update();
}

// HUD ?????
bool HsImageWindow::isHudVisible() const
{
    return d->hudVisible;
}

// ?????????
void HsImageWindow::setImageInfoText(const QString& text)
{
    d->imageInfoText = text;
    viewport()->update();
}

// ????????????
QString HsImageWindow::imageInfoText() const
{
    return d->imageInfoText;
}

// ??????飺?????????? HUD ????
void HsImageWindow::setPixelInspect(bool on)
{
    d->pixelInspect = on;
    d->imageItem->setPixelInspect(on);
    viewport()->update();
}

// ????????????
bool HsImageWindow::isPixelInspect() const
{
    return d->pixelInspect;
}

// ????????Demo ?????λ??
void HsImageWindow::setLive(bool live)
{
    d->live = live;
    viewport()->update();
}

// ??????????
bool HsImageWindow::isLive() const
{
    return d->live;
}

// Stack ??????
void HsImageWindow::setStackVisible(bool visible)
{
    d->stackVisible = visible;
    d->syncStackVisibility();
}

// Stack ????????
bool HsImageWindow::isStackVisible() const
{
    return d->stackVisible;
}

// Drawing ??????
void HsImageWindow::setDrawingVisible(bool visible)
{
    d->drawingVisible = visible;
    if (d->mode != WindowRuntime) {
        d->setLayerVisible(LayerDrawing, visible);
    }
}

// Drawing ????????
bool HsImageWindow::isDrawingVisible() const
{
    return d->drawingVisible;
}

// ????????????floor ??????
bool HsImageWindow::sampleGray(const QPointF& imagePt, double* gray) const
{
    return d->frame.sampleGray(imagePt.x(), imagePt.y(), gray);
}

// ???????? RGB
bool HsImageWindow::sampleRgb(const QPointF& imagePt, int* r, int* g, int* b) const
{
    return d->frame.sampleRgb(imagePt.x(), imagePt.y(), r, g, b);
}

namespace {

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

void HsImageWindow::setDepthProbeU16(const uint16_t* data, int width, int height, int strideBytes)
{
    d->depthProbeF32.clear();
    d->depthProbeF64.clear();
    copyDepthRows(data, width, height, strideBytes, &d->depthProbeU16);
    d->depthProbeKind = d->depthProbeU16.empty() ? Private::DepthProbeNone : Private::DepthProbeU16;
    d->depthProbeW = d->depthProbeU16.empty() ? 0 : width;
    d->depthProbeH = d->depthProbeU16.empty() ? 0 : height;
}

void HsImageWindow::setDepthProbeF32(const float* data, int width, int height, int strideBytes)
{
    d->depthProbeU16.clear();
    d->depthProbeF64.clear();
    copyDepthRows(data, width, height, strideBytes, &d->depthProbeF32);
    d->depthProbeKind = d->depthProbeF32.empty() ? Private::DepthProbeNone : Private::DepthProbeF32;
    d->depthProbeW = d->depthProbeF32.empty() ? 0 : width;
    d->depthProbeH = d->depthProbeF32.empty() ? 0 : height;
}

void HsImageWindow::setDepthProbeF64(const double* data, int width, int height, int strideBytes)
{
    d->depthProbeU16.clear();
    d->depthProbeF32.clear();
    copyDepthRows(data, width, height, strideBytes, &d->depthProbeF64);
    d->depthProbeKind = d->depthProbeF64.empty() ? Private::DepthProbeNone : Private::DepthProbeF64;
    d->depthProbeW = d->depthProbeF64.empty() ? 0 : width;
    d->depthProbeH = d->depthProbeF64.empty() ? 0 : height;
}

void HsImageWindow::setDepthProbeGray(const QImage& gray)
{
    d->depthProbeGray = gray;
}

void HsImageWindow::clearDepthProbe()
{
    d->clearDepthProbeData();
}

// ???????????????
QPointF HsImageWindow::widgetToImage(const QPointF& widgetPos) const
{
    // ??? QTransform ???棬????????????
    bool ok = false;
    const QTransform inv = viewportTransform().inverted(&ok);
    if (!ok) {
        return QPointF();
    }
    return inv.map(widgetPos);
}

// ???????????????
QPointF HsImageWindow::imageToWidget(const QPointF& imagePt) const
{
    return viewportTransform().map(imagePt);
}

// ????????????????????
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

// ??????????滻?????????????????
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

// ??????????????
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

// ????????????????????
void HsImageWindow::clearDrawInfo(DrawInfoSpace space)
{
    if (space == DrawInfoWindow) {
        d->windowDrawInfos.clear();
    } else {
        d->imageDrawInfos.clear();
    }
    viewport()->update();
}

// ??????????????
void HsImageWindow::clearDrawInfo()
{
    d->imageDrawInfos.clear();
    d->windowDrawInfos.clear();
    viewport()->update();
}

// ???浱???????
bool HsImageWindow::saveCurrentFrame(const QString& filePath, QString* error) const
{
    if (filePath.isEmpty()) {
        if (error) {
            *error = QString::fromUtf8("????·?????");
        }
        return false;
    }
    const QImage saved = qImageFromFrame(d->frame);
    if (d->frame.isNull() || saved.isNull()) {
        if (error) {
            *error = QString::fromUtf8("?????");
        }
        return false;
    }
    if (!saved.save(filePath)) {
        if (error) {
            *error = QString::fromUtf8("??????????: %1").arg(filePath);
        }
        return false;
    }
    return true;
}

// ????????????????????????????????????
bool HsImageWindow::saveSnapshot(const QString& filePath, QString* error) const
{
    if (filePath.isEmpty()) {
        if (error) {
            *error = QString::fromUtf8("????·?????");
        }
        return false;
    }
    QWidget* vp = viewport();
    if (!vp) {
        if (error) {
            *error = QString::fromUtf8("?????");
        }
        return false;
    }
    const QPixmap pix = vp->grab();
    if (pix.isNull()) {
        if (error) {
            *error = QString::fromUtf8("?????????");
        }
        return false;
    }
    if (!pix.save(filePath)) {
        if (error) {
            *error = QString::fromUtf8("??????????: %1").arg(filePath);
        }
        return false;
    }
    return true;
}

// ??????????????
void HsImageWindow::setOverlayToolbarVisible(bool visible)
{
    if (!d->overlayBar) {
        return;
    }
    d->overlayBar->setVisible(visible);
    d->placeOverlayToolbar();
}

// ???????????
bool HsImageWindow::isOverlayToolbarVisible() const
{
    return d->overlayBar && d->overlayBar->isVisible();
}
// ????????????????δ???ò?????????????
void HsImageWindow::setMeasureToolbarVisible(bool visible)
{
    d->measureBarWant = visible;
    d->syncMeasureBar();
}

// ???????????????
bool HsImageWindow::isMeasureToolbarVisible() const
{
    return d->measureBar && d->measureBar->isVisible();
}


// ????????????
void HsImageWindow::setOverlayToolbarCorner(Qt::Corner corner)
{
    d->overlayBarCorner = corner;
    d->placeOverlayToolbar();
}

// ????????
Qt::Corner HsImageWindow::overlayToolbarCorner() const
{
    return d->overlayBarCorner;
}

// ?????????????????
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

// ??????з???
Qt::Orientation HsImageWindow::overlayToolbarOrientation() const
{
    return d->overlayBarOrient;
}

// ?????????????
QWidget* HsImageWindow::overlayToolbar() const
{
    return d->overlayBar;
}

// ????????β??????
void HsImageWindow::addOverlayToolbarWidget(QWidget* widget)
{
    if (!widget || !d->overlayBarLayout) {
        return;
    }
    widget->setParent(d->overlayBar);
    d->overlayBarLayout->addWidget(widget);
    d->placeOverlayToolbar();
}

// ???ù????????????Щ???????
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

// ??????????????????
std::vector<DrawTool> HsImageWindow::measureTools() const
{
    return d->measureShown;
}

// ?????????????
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
        QString name = QString::fromUtf8("????");
        if (g.id.compare(0, 6, "m.dist") == 0) {
            name = QString::fromUtf8("????");
        } else if (g.id.compare(0, 5, "m.ang") == 0) {
            name = QString::fromUtf8("?н?");
        } else if (g.id.compare(0, 5, "m.rad") == 0) {
            name = QString::fromUtf8("?");
        } else if (g.id.compare(0, 6, "m.rect") == 0) {
            name = QString::fromUtf8("????");
        } else if (g.id.compare(0, 5, "m.ell") == 0) {
            name = QString::fromUtf8("???");
        }
        lines.append(name + QStringLiteral("  ") + text);
    }
    return lines;
}

// ??λ?????????? Stack / ROI / ????
void HsImageWindow::setImageRefreshPolicy(const ImageRefreshPolicy& policy)
{
    d->imageRefresh = policy;
}

// ??????????????
ImageRefreshPolicy HsImageWindow::imageRefreshPolicy() const
{
    return d->imageRefresh;
}

// ?????????
void HsImageWindow::setViewportBackground(ViewportBackground mode)
{
    d->vpBg = mode;
    if (viewport()) {
        viewport()->update();
    }
}

// ????????????
ViewportBackground HsImageWindow::viewportBackground() const
{
    return d->vpBg;
}

// ????????Custom ????????????
void HsImageWindow::setViewportBackgroundCustomColor(const QColor& color)
{
    d->vpBgCustom = color;
    if (viewport()) {
        viewport()->update();
    }
}

// ?????????
QColor HsImageWindow::viewportBackgroundCustomColor() const
{
    return d->vpBgCustom;
}

// ?е? Custom ????????
void HsImageWindow::setBackgroundColor(const QColor& color)
{
    d->vpBgCustom = color;
    d->vpBg = ViewportCustom;
    if (viewport()) {
        viewport()->update();
    }
}

// ?????????ο??
QColor HsImageWindow::backgroundColor() const
{
    return d->resolvedViewportColor();
}

// ?????????????
void HsImageWindow::setOverlayToolbarSkin(OverlayToolbarSkin skin)
{
    d->overlayBarSkin = skin;
    d->applyOverlayToolbarSkin();
}

// ????????????
OverlayToolbarSkin HsImageWindow::overlayToolbarSkin() const
{
    return d->overlayBarSkin;
}

// Overlay ???????Hz??
qreal HsImageWindow::fps() const
{
    return d->fps;
}

// ???????????????????????
void HsImageWindow::drawBackground(QPainter* painter, const QRectF&)
{
    painter->save();
    painter->resetTransform();
    d->paintViewportBackground(painter);
    painter->restore();
}

// ?? drawInfo??????????? HUD????????? Item
void HsImageWindow::drawForeground(QPainter* painter, const QRectF& exposed)
{
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

// ??????仯???? part ?????
void HsImageWindow::resizeEvent(QResizeEvent* e)
{
    QGraphicsView::resizeEvent(e);
    if (d->vp.isValid()) {
        d->vp.syncAspect(d->viewSize());
        d->applyTransform();
    }
    d->placeOverlayToolbar();
}

// ??????????????????????罻??????????? sceneRect ??????
QSize HsImageWindow::sizeHint() const
{
    return QSize(320, 240);
}

// ??????????С?????????????????
QSize HsImageWindow::minimumSizeHint() const
{
    return QSize(64, 64);
}

// ???????????δ fit ?????????
void HsImageWindow::showEvent(QShowEvent* e)
{
    QGraphicsView::showEvent(e);
    if (d->vp.imageSize.isValid() && !d->vp.isValid()) {
        d->vp.fit(d->viewSize());
        d->applyTransform();
    }
    d->placeOverlayToolbar();
}

// ?????????ê????
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

// ????????/ ????/ ????浶
void HsImageWindow::mousePressEvent(QMouseEvent* e)
{
    setFocus();
    d->lastViewPos = e->pos();
    d->pressImage = widgetToImage(e->pos());
    d->updateCursor(e->pos());

    if (e->button() == Qt::MiddleButton) {
        if (d->state != StateDraw) {
            d->state = StatePan;
        }
        e->accept();
        return;
    }

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
            // ??????? Halcon draw_* ??????????????????????????????            d->skipNextContextMenu = true;
            d->commitSessionShape();
        }
        e->accept();
        return;
    }

    if (e->button() != Qt::LeftButton) {
        e->accept();
        return;
    }

    // ????????в???????????????????????????????
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

    HsCommData::Graphic g = d->makePreview(d->tool, d->pressImage);
    g.id = fromQ(d->nextId(QStringLiteral("preview")));
    d->upsertDrawing(g);
    d->activeId = toQ(g.id);
    d->rubberActive = true;
    d->state = StateDraw;
    e->accept();
}

// ???????????????????????????
void HsImageWindow::mouseMoveEvent(QMouseEvent* e)
{
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

// ??????? ROI ????????
void HsImageWindow::mouseReleaseEvent(QMouseEvent* e)
{
    d->updateCursor(e->pos());

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
            // ???????????????????Enter ???????????
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

// ??????????????????
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

// Esc ?????Enter ???????/?????
void HsImageWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Control) {
        d->updateCtrlPixelPopup(d->cursorViewPos);
        e->accept();
        return;
    }
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

// ??? Ctrl ????????С??
void HsImageWindow::keyReleaseEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Control) {
        d->hideCtrlPixelPopup();
        e->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(e);
}

// ???????? HUD ???
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

// ????????????δ?????????????????????
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
    HsImageWindowMenu::popup(this, e->globalPos(), !d->frame.isNull(), d->vp.isValid(),
                             d->rubberActive, measureId);
}

}

#ifdef HSIMAGE_MOC_FROM_INTDIR
// ??????????? Linux AUTOMOC ?????е??? moc ???? src/display ??????
#define HSIMAGE_MOC_HSIMAGEWINDOW "moc_HsImageWindow.cpp"
#include HSIMAGE_MOC_HSIMAGEWINDOW
#endif

