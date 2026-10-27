/**
 * @file HalObjectConvert.cpp
 * @brief HalObjectConvert实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalObjectConvert.h"

#include <HsCommData/Geom.h>
#include <cmath>

using HalconCpp::HObject;
using HalconCpp::HTuple;
using HalconCpp::HException;
using HalconCpp::GenRectangle1;
using HalconCpp::GenRectangle2;
using HalconCpp::GenCircle;
using HalconCpp::GenRegionLine;
using HalconCpp::GenRegionPolygonFilled;
using HalconCpp::GenRegionRuns;
using HalconCpp::DilationCircle;
using HalconCpp::Union2;
using HalconCpp::AreaCenter;
using HalconCpp::CountObj;
using HalconCpp::GetRegionRuns;

namespace Vision {

// 区域是否无有效面积
bool hoIsEmpty(const HObject& r)
{
    try {
        HTuple num;
        CountObj(r, &num);
        if (num.Length() < 1 || num[0].L() < 1) {
            return true;
        }
        HTuple area;
        HTuple row;
        HTuple col;
        AreaCenter(r, &area, &row, &col);
        if (area.Length() < 1) {
            return true;
        }
        return area[0].D() < 0.5;
    } catch (const HException&) {
        return true;
    }
}

// 整幅矩形区域（Halcon 行/列闭区间）
HObject fullImageRegion(int imgW, int imgH)
{
    HObject r;
    const double w = imgW > 0 ? static_cast<double>(imgW - 1) : 0.0;
    const double h = imgH > 0 ? static_cast<double>(imgH - 1) : 0.0;
    GenRectangle1(&r, 0.0, 0.0, h, w);
    return r;
}

// 线段扩成条带区域
static HObject lineStripRegion(double x1, double y1, double x2, double y2, double dilateR)
{
    HObject r;
    GenRegionLine(&r, y1, x1, y2, x2);
    if (dilateR > 0.1) {
        HObject dil;
        DilationCircle(r, &dil, dilateR);
        return dil;
    }
    return r;
}

// HsRegion 行程 → HObject
HObject hsRegionToHObject(const HsCommData::HsRegion& reg)
{
    HObject r;
    if (reg.runsEmpty()) {
        HalconCpp::GenEmptyObj(&r);
        return r;
    }
    HTuple row;
    HTuple cb;
    HTuple ce;
    for (size_t i = 0; i < reg.runCount(); ++i) {
        row.Append(static_cast<Hlong>(reg.runAt(i).row));
        cb.Append(static_cast<Hlong>(reg.runAt(i).cb));
        ce.Append(static_cast<Hlong>(reg.runAt(i).ce));
    }
    GenRegionRuns(&r, row, cb, ce);
    return r;
}

// 单格几何 → 区域；失败返回空对象（调用方决定是否整幅图）
static HObject geomCellToRegion(const HsCommData::HsObject& obj, int index)
{
    using namespace HsCommData;
    HObject r;
    HalconCpp::GenEmptyObj(&r);
    if (!obj.isInitialized() || index < 0 || index >= obj.count()) {
        return r;
    }
    const ObjectKind k = obj.kindAt(index);
    if (k == ObjRect1) {
        const Rect1P* rc = obj.rect1At(index);
        if (!rc) {
            return r;
        }
        const double c1 = rc->r.x;
        const double r1 = rc->r.y;
        const double c2 = rc->r.x + (rc->r.w > 1.0 ? rc->r.w - 1.0 : 0.0);
        const double r2 = rc->r.y + (rc->r.h > 1.0 ? rc->r.h - 1.0 : 0.0);
        GenRectangle1(&r, r1, c1, r2, c2);
        return r;
    }
    if (k == ObjRect2) {
        const Rect2P* rc = obj.rect2At(index);
        if (!rc) {
            return r;
        }
        GenRectangle2(&r, rc->c.y, rc->c.x, rc->phi, rc->l1, rc->l2);
        return r;
    }
    if (k == ObjEllipse) {
        const EllipseP* e = obj.ellipseAt(index);
        if (!e) {
            return r;
        }
        GenRectangle2(&r, e->c.y, e->c.x, e->phi, e->ra, e->rb);
        return r;
    }
    if (k == ObjCircle) {
        const CircleP* c = obj.circleAt(index);
        if (!c) {
            return r;
        }
        GenCircle(&r, c->c.y, c->c.x, c->r);
        return r;
    }
    if (k == ObjLine) {
        const LineP* ln = obj.lineAt(index);
        if (!ln) {
            return r;
        }
        return lineStripRegion(ln->a.x, ln->a.y, ln->b.x, ln->b.y, 8.0);
    }
    if (k == ObjContour) {
        const HsContour* c = obj.contourAt(index);
        if (c && c->ptCount() >= 2) {
            if (c->ptCount() == 2) {
                return lineStripRegion(c->ptAt(0).x, c->ptAt(0).y, c->ptAt(1).x, c->ptAt(1).y, 8.0);
            }
            HTuple rows;
            HTuple cols;
            for (size_t i = 0; i < c->ptCount(); ++i) {
                rows.Append(c->ptAt(i).y);
                cols.Append(c->ptAt(i).x);
            }
            GenRegionPolygonFilled(&r, rows, cols);
        }
        return r;
    }
    if (k == ObjRegion) {
        const HsRegion* reg = obj.regionAt(index);
        if (reg && !reg->runsEmpty()) {
            return hsRegionToHObject(*reg);
        }
        return r;
    }
    return r;
}

// 并入累加区域
static void unionInto(HObject* acc, bool* first, const HObject& one)
{
    if (!acc || !first || hoIsEmpty(one)) {
        return;
    }
    if (*first) {
        *acc = one;
        *first = false;
    } else {
        HObject uni;
        Union2(*acc, one, &uni);
        *acc = uni;
    }
}

// 单格或整袋 HsObject 转 HObject（区域并集；跳过 ObjImage）
HObject hsObjectToHObject(const HsCommData::HsObject& obj, int imgW, int imgH)
{
    if (!obj.isInitialized() || obj.isEmpty()) {
        return fullImageRegion(imgW, imgH);
    }
    HObject acc;
    bool first = true;
    const int n = obj.count();
    for (int i = 0; i < n; ++i) {
        if (obj.kindAt(i) == HsCommData::ObjImage) {
            continue;
        }
        unionInto(&acc, &first, geomCellToRegion(obj, i));
    }
    if (first) {
        return fullImageRegion(imgW, imgH);
    }
    return acc;
}

// 多只上游区域对象并集；空则整幅图
HObject hsRegionsToHObject(const std::vector<HsCommData::HsObject>& regs, int imgW, int imgH)
{
    if (regs.empty()) {
        return fullImageRegion(imgW, imgH);
    }
    HObject acc;
    bool first = true;
    for (size_t i = 0; i < regs.size(); ++i) {
        const HsCommData::HsObject& obj = regs[i];
        if (!obj.isInitialized()) {
            continue;
        }
        const int n = obj.count();
        for (int ei = 0; ei < n; ++ei) {
            if (obj.kindAt(ei) != HsCommData::ObjRegion) {
                continue;
            }
            const HsCommData::HsRegion* reg = obj.regionAt(ei);
            if (!reg || reg->runsEmpty()) {
                continue;
            }
            unionInto(&acc, &first, hsRegionToHObject(*reg));
        }
    }
    if (first) {
        return fullImageRegion(imgW, imgH);
    }
    return acc;
}

// 单只 HsObject → 区域；无效则整幅图
HObject toolRoiToHObject(const HsCommData::HsObject& roi, int imgW, int imgH)
{
    if (!roi.isInitialized() || roi.isEmpty() || imgW <= 0 || imgH <= 0) {
        return fullImageRegion(imgW, imgH);
    }
    const HObject r = hsObjectToHObject(roi, imgW, imgH);
    if (hoIsEmpty(r)) {
        return fullImageRegion(imgW, imgH);
    }
    return r;
}

// 多 ROI 并集；空则整幅图
HObject toolRoisToHObject(const std::vector<HsCommData::HsObject>& rois, int imgW, int imgH)
{
    if (rois.empty()) {
        return fullImageRegion(imgW, imgH);
    }
    HObject acc;
    bool first = true;
    for (int i = 0; i < static_cast<int>(rois.size()); ++i) {
        unionInto(&acc, &first, toolRoiToHObject(rois[static_cast<size_t>(i)], imgW, imgH));
    }
    if (first) {
        return fullImageRegion(imgW, imgH);
    }
    return acc;
}

// HObject(Region) → HsObject(ObjRegion)
HsCommData::HsObject hObjectToRegionObject(const HObject& r)
{
    HsCommData::HsRegion reg;
    if (hoIsEmpty(r)) {
        return HsCommData::HsObject(reg);
    }
    HTuple row;
    HTuple cb;
    HTuple ce;
    GetRegionRuns(r, &row, &cb, &ce);
    Hlong n = row.Length();
    int32_t minR = 0;
    int32_t maxR = 0;
    int32_t minC = 0;
    int32_t maxC = 0;
    for (Hlong i = 0; i < n; ++i) {
        const int y = static_cast<int>(row[i].L());
        const int x0 = static_cast<int>(cb[i].L());
        const int x1 = static_cast<int>(ce[i].L());
        reg.pushRun(HsCommData::Chord(y, x0, x1));
        if (i == 0) {
            minR = maxR = y;
            minC = x0;
            maxC = x1;
        } else {
            if (y < minR) minR = y;
            if (y > maxR) maxR = y;
            if (x0 < minC) minC = x0;
            if (x1 > maxC) maxC = x1;
        }
    }
    if (!reg.runsEmpty()) {
        reg.bounds = HsCommData::fromInclusive(minC, minR, maxC, maxR);
    }
    return HsCommData::HsObject(reg);
}

// 结果圆 → HsObject
HsCommData::HsObject circleObject(double cx, double cy, double r)
{
    HsCommData::CircleP c;
    c.c = HsCommData::Point2d(cx, cy);
    c.r = r;
    return HsCommData::HsObject(c);
}

// 结果线段 → HsObject
HsCommData::HsObject lineObject(double x1, double y1, double x2, double y2)
{
    HsCommData::LineP ln;
    ln.a = HsCommData::Point2d(x1, y1);
    ln.b = HsCommData::Point2d(x2, y2);
    return HsCommData::HsObject(ln);
}

// 匹配点十字（两段线段）→ HsObject
HsCommData::HsObject crossObject(double cx, double cy)
{
    const double s = 8.0;
    HsCommData::LineP h;
    h.a = HsCommData::Point2d(cx - s, cy);
    h.b = HsCommData::Point2d(cx + s, cy);
    HsCommData::LineP v;
    v.a = HsCommData::Point2d(cx, cy - s);
    v.b = HsCommData::Point2d(cx, cy + s);
    HsCommData::HsObject obj(h);
    obj.append(HsCommData::HsObject(v));
    return obj;
}

}
