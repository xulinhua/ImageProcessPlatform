/**
 * @file ToolGeom.cpp
 * @brief ToolGeom实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolGeom.h"
#include "ParamKeys.h"
#include <HsCommData/GeomOps.h>
#include <HsCommData/Style.h>
#include <cmath>

namespace Vision {

namespace {

// 绕示教点旋转再平移到当前位姿
void mapPoint(double* x, double* y, double refX, double refY, double poseX, double poseY, double c, double s)
{
    if (!x || !y) {
        return;
    }
    const double dx = *x - refX;
    const double dy = *y - refY;
    *x = poseX + c * dx - s * dy;
    *y = poseY + s * dx + c * dy;
}

// 旧游程还原区域
HsCommData::HsRegion regionFromRuns(const std::vector<std::int32_t>& runRow,
                                    const std::vector<std::int32_t>& runCb,
                                    const std::vector<std::int32_t>& runCe)
{
    HsCommData::HsRegion reg;
    const size_t n = runRow.size();
    if (n == 0 || runCb.size() != n || runCe.size() != n) {
        return reg;
    }
    reg.reserveRuns(n);
    for (size_t i = 0; i < n; ++i) {
        reg.pushRun(HsCommData::Chord(runRow[i], runCb[i], runCe[i]));
    }
    int32_t minR = reg.runAt(0).row;
    int32_t maxR = reg.runAt(0).row;
    int32_t minC = reg.runAt(0).cb;
    int32_t maxC = reg.runAt(0).ce;
    for (size_t i = 1; i < reg.runCount(); ++i) {
        const HsCommData::Chord& ch = reg.runAt(i);
        if (ch.row < minR) {
            minR = ch.row;
        }
        if (ch.row > maxR) {
            maxR = ch.row;
        }
        if (ch.cb < minC) {
            minC = ch.cb;
        }
        if (ch.ce > maxC) {
            maxC = ch.ce;
        }
    }
    reg.bounds = HsCommData::fromInclusive(minC, minR, maxC, maxR);
    return reg;
}

} // namespace

// 几何是区域
bool graphicIsRegion(const HsCommData::Graphic& g)
{
    return g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjRegion;
}

// 几何是轮廓
bool graphicIsContour(const HsCommData::Graphic& g)
{
    return g.kind != HsCommData::KindKeypoints
        && g.kind != HsCommData::KindSkeleton
        && g.geom.isInitialized()
        && g.geom.kind() == HsCommData::ObjContour;
}

// 搜索区画成可拖图元
HsCommData::Graphic roiToDrawing(const HsCommData::HsObject& roi)
{
    HsCommData::Graphic g;
    g.role = HsCommData::RoleDrawing;
    g.visible = roi.isInitialized() && !roi.isEmpty();
    g.geomStyle = HsCommData::Style::drawingStyle();
    g.kind = HsCommData::KindObject;
    g.geom = roi;
    g.id = "roi";
    if (g.visible) {
        g.updateBounds();
    }
    return g;
}

// 绘制图元收成一块搜索区
HsCommData::HsObject drawingToRoi(const HsCommData::Graphic& g)
{
    HsCommData::HsObject r;
    if (g.kind == HsCommData::KindCross || g.kind == HsCommData::KindPoint
        || g.kind == HsCommData::KindText || g.kind == HsCommData::KindTextBadge
        || g.kind == HsCommData::KindArrow) {
        return r;
    }
    if (!g.geom.isInitialized() || g.geom.isEmpty()) {
        return r;
    }
    const HsCommData::ObjectKind k = g.geom.kind();
    if (k != HsCommData::ObjCircle && k != HsCommData::ObjEllipse
        && k != HsCommData::ObjRect1 && k != HsCommData::ObjRect2
        && k != HsCommData::ObjLine && k != HsCommData::ObjPoly
        && k != HsCommData::ObjContour && k != HsCommData::ObjRegion) {
        return r;
    }
    r = g.geom.select(0);
    return r;
}

// 旧 x/y/a/b/phi 转 HsObject
HsCommData::HsObject toolRoiFromLegacy(int kind, double x, double y, double a, double b, double phi,
                          const std::vector<double>& polyX, const std::vector<double>& polyY,
                          bool polyClosed,
                          const std::vector<std::int32_t>& runRow,
                          const std::vector<std::int32_t>& runCb,
                          const std::vector<std::int32_t>& runCe)
{
    using namespace HsCommData;
    HsCommData::HsObject r;
    if (kind == 0) {
        CircleP c;
        c.c = Point2d(x, y);
        c.r = a;
        r = c;
    } else if (kind == 1) {
        EllipseP e;
        e.c = Point2d(x, y);
        e.ra = a;
        e.rb = b;
        e.phi = phi;
        r = e;
    } else if (kind == 2) {
        Rect1P rc;
        rc.r = Box2(x, y, a, b);
        r = rc;
    } else if (kind == 3) {
        Rect2P rc;
        rc.c = Point2d(x, y);
        rc.l1 = a;
        rc.l2 = b;
        rc.phi = phi;
        r = rc;
    } else if (kind == 4) {
        LineP ln;
        ln.a = Point2d(x, y);
        ln.b = Point2d(x + a, y + b);
        r = ln;
    } else if (kind == 9) {
        HsContour c;
        c.closed = polyClosed;
        const size_t n = polyX.size() < polyY.size() ? polyX.size() : polyY.size();
        for (size_t i = 0; i < n; ++i) {
            c.pushPt(Point2d(polyX[i], polyY[i]));
        }
        if (c.ptsEmpty()) {
            c.pushPt(Point2d(x, y));
            c.pushPt(Point2d(x + a, y + b));
        }
        r = c;
    } else if (kind == 10) {
        HsRegion reg = regionFromRuns(runRow, runCb, runCe);
        if (reg.runsEmpty() && polyX.size() >= 3 && polyX.size() == polyY.size()) {
            std::vector<Point2d> pts;
            for (size_t i = 0; i < polyX.size(); ++i) {
                pts.push_back(Point2d(polyX[i], polyY[i]));
            }
            reg = HsRegion::genRegion(pts);
        }
        r = reg;
    }
    return r;
}

// 从数值结果读位姿
bool poseFromValues(const ParamMap& v, HsCommData::Pose2* pose)
{
    if (!pose) {
        return false;
    }
    if (!paramHas(v, ValueKey::X) && !paramHas(v, ValueKey::Cx)) {
        return false;
    }
    pose->tx = paramHas(v, ValueKey::X) ? paramGet(v, ValueKey::X).toDouble()
                                        : paramGet(v, ValueKey::Cx).toDouble();
    pose->ty = paramHas(v, ValueKey::Y) ? paramGet(v, ValueKey::Y).toDouble()
                                        : paramGet(v, ValueKey::Cy).toDouble();
    pose->setAng(paramGet(v, ValueKey::Angle).toDouble());
    return true;
}

// 位姿写入数值结果
void poseToValues(ParamMap* v, const HsCommData::Pose2& p)
{
    if (!v) {
        return;
    }
    paramSet(v, ValueKey::X, p.tx);
    paramSet(v, ValueKey::Y, p.ty);
    paramSet(v, ValueKey::Angle, p.ang());
}

// 位姿跟随：旋转和平移搜索区
void applyPoseToRois(std::vector<HsCommData::HsObject>* rois, const ParamMap& params, const HsCommData::Pose2& pose)
{
    using namespace HsCommData;
    if (!rois || !teachHas(params, TeachKey::RefX)) {
        return;
    }
    const double refX = teachGet(params, TeachKey::RefX).toDouble();
    const double refY = teachGet(params, TeachKey::RefY).toDouble();
    HsCommData::Pose2 taught(refX, refY, 0.0);
    taught.setAng(teachGet(params, TeachKey::RefA).toDouble());
    const double da = pose.angle - taught.angle;
    const double c = std::cos(da);
    const double s = std::sin(da);
    for (size_t i = 0; i < rois->size(); ++i) {
        HsCommData::HsObject& roi = (*rois)[i];
        if (!roi.isInitialized() || roi.isEmpty()) {
            continue;
        }
        HsObject g = roi.clone();
        const ObjectKind k = g.kind();
        if (k == ObjCircle) {
            CircleP* o = g.circleAt(0);
            mapPoint(&o->c.x, &o->c.y, refX, refY, pose.tx, pose.ty, c, s);
        } else if (k == ObjEllipse) {
            EllipseP* o = g.ellipseAt(0);
            mapPoint(&o->c.x, &o->c.y, refX, refY, pose.tx, pose.ty, c, s);
            o->phi += da;
        } else if (k == ObjRect2) {
            Rect2P* o = g.rect2At(0);
            mapPoint(&o->c.x, &o->c.y, refX, refY, pose.tx, pose.ty, c, s);
            o->phi += da;
        } else if (k == ObjRect1) {
            Rect1P* o = g.rect1At(0);
            if (std::fabs(da) < 1e-9) {
                double x2 = o->r.x + o->r.w;
                double y2 = o->r.y + o->r.h;
                mapPoint(&o->r.x, &o->r.y, refX, refY, pose.tx, pose.ty, c, s);
                mapPoint(&x2, &y2, refX, refY, pose.tx, pose.ty, c, s);
                o->r.w = x2 - o->r.x;
                o->r.h = y2 - o->r.y;
            } else {
                Rect2P rc;
                rc.c = Point2d(o->r.x + o->r.w * 0.5, o->r.y + o->r.h * 0.5);
                rc.l1 = o->r.w * 0.5;
                rc.l2 = o->r.h * 0.5;
                rc.phi = da;
                mapPoint(&rc.c.x, &rc.c.y, refX, refY, pose.tx, pose.ty, c, s);
                g = rc;
            }
        } else if (k == ObjLine) {
            LineP* o = g.lineAt(0);
            mapPoint(&o->a.x, &o->a.y, refX, refY, pose.tx, pose.ty, c, s);
            mapPoint(&o->b.x, &o->b.y, refX, refY, pose.tx, pose.ty, c, s);
        } else if (k == ObjContour) {
            HsContour* o = g.contourAt(0);
            for (size_t p = 0; p < o->ptCount(); ++p) {
                mapPoint(&o->ptAt(p).x, &o->ptAt(p).y, refX, refY, pose.tx, pose.ty, c, s);
            }
        }
        roi = g;
    }
}

// 图标对象转成窗口叠加
HsCommData::Graphic objectToGraphic(const HsCommData::HsObject& obj)
{
    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.role = HsCommData::RoleStack;
    g.visible = obj.isInitialized() && !obj.isEmpty();
    g.geomStyle = HsCommData::Style::stackStyle();
    g.geom = obj;
    if (g.visible) {
        g.updateBounds();
    }
    return g;
}

// 一批图标对象转成窗口叠加
std::vector<HsCommData::Graphic> objectsToGraphics(const std::vector<HsCommData::HsObject>& objs)
{
    std::vector<HsCommData::Graphic> out;
    out.reserve(objs.size());
    for (size_t i = 0; i < objs.size(); ++i) {
        out.push_back(objectToGraphic(objs[i]));
    }
    return out;
}


// 工具画布能力。由工具 DLL 填写，平台不按名字猜测
bool ToolCaps::usesMultiRoi() const
{
        for (size_t i = 0; i < slotRules.size(); ++i) {
            if (slotRules[i].maxCount != 1) {
                return true;
            }
        }
        return false;
    }

// 工具画布能力。由工具 DLL 填写，平台不按名字猜测
int ToolCaps::preferredRoiKind() const
{
        int found = -1;
        bool any = false;
        for (size_t si = 0; si < slotRules.size(); ++si) {
            for (size_t i = 0; i < slotRules[si].shapes.size(); ++i) {
                const int k = static_cast<int>(slotRules[si].shapes[i].kind);
                if (!any) {
                    found = k;
                    any = true;
                } else if (found != k) {
                    return -1;
                }
            }
        }
        return found;
    }

// 工具画布能力。由工具 DLL 填写，平台不按名字猜测
bool ToolCaps::kindMatches(int kind) const
{
        if (kind < 0) {
            return false;
        }
        bool listed = false;
        for (size_t si = 0; si < slotRules.size(); ++si) {
            for (size_t i = 0; i < slotRules[si].shapes.size(); ++i) {
                listed = true;
                const int one = static_cast<int>(slotRules[si].shapes[i].kind);
                if (kind == one) {
                    return true;
                }
                if (one == static_cast<int>(HsCommData::ObjLine) && kind == static_cast<int>(HsCommData::ObjContour)) {
                    return true;
                }
            }
        }
        return !listed;
    }

}
