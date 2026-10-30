/**
 * @file HalGeom.cpp
 * @brief HalGeom实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalGeom.h"
#include <HsCommData/GeomOps.h>
#include <HsCommData/Graphic.h>
#include <HsCommData/Style.h>
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
using HalconCpp::GenEmptyObj;
using HalconCpp::DilationCircle;
using HalconCpp::Union2;
using HalconCpp::AreaCenter;
using HalconCpp::CountObj;
using HalconCpp::GetRegionRuns;

namespace Vision {

// 无效或未知 kind 时用整幅图
static HObject fullImageRegion(int imgW, int imgH)
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

// 区域是否无面积（AreaCenter，与旧工程一致）
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

// HsCommData::HsObject 到 HObject。Halcon (row,col) = (y,x)
HObject roiToRegion(const HsCommData::HsObject& roi, int imgW, int imgH)
{
    using namespace HsCommData;
    if (!roi.isInitialized() || roi.isEmpty() || imgW <= 0 || imgH <= 0) {
        return fullImageRegion(imgW, imgH);
    }
    HObject r;
    const ObjectKind k = roi.kind();
    // 只看第一格几何；轴对齐矩形的 w、h 是像素数，Halcon 要闭区间右下角
    if (k == ObjRect1) {
        const Rect1P* rc = roi.rect1At(0);
        const double c1 = rc->r.x;
        const double r1 = rc->r.y;
        const double c2 = rc->r.x + (rc->r.w > 1.0 ? rc->r.w - 1.0 : 0.0);
        const double r2 = rc->r.y + (rc->r.h > 1.0 ? rc->r.h - 1.0 : 0.0);
        GenRectangle1(&r, r1, c1, r2, c2);
        return r;
    }
    if (k == ObjRect2) {
        const Rect2P* rc = roi.rect2At(0);
        GenRectangle2(&r, rc->c.y, rc->c.x, rc->phi, rc->l1, rc->l2);
        return r;
    }
    // 椭圆当前用同中心、同角度、半边长取 ra/rb 的旋转矩形生成
    if (k == ObjEllipse) {
        const EllipseP* e = roi.ellipseAt(0);
        GenRectangle2(&r, e->c.y, e->c.x, e->phi, e->ra, e->rb);
        return r;
    }
    if (k == ObjCircle) {
        const CircleP* c = roi.circleAt(0);
        GenCircle(&r, c->c.y, c->c.x, c->r);
        return r;
    }
    // 线和两点轮廓按半径 8 膨胀成条带；多点轮廓填充，结果为空时退回整幅图
    if (k == ObjLine) {
        const LineP* ln = roi.lineAt(0);
        return lineStripRegion(ln->a.x, ln->a.y, ln->b.x, ln->b.y, 8.0);
    }
    if (k == ObjContour) {
        const HsContour* c = roi.contourAt(0);
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
            if (!hoIsEmpty(r)) {
                return r;
            }
        }
        return fullImageRegion(imgW, imgH);
    }
    // 行程区域按原样还原，为空时退回整幅图
    if (k == ObjRegion) {
        const HsRegion* reg = roi.regionAt(0);
        if (reg && !reg->runsEmpty()) {
            HTuple row;
            HTuple cb;
            HTuple ce;
            for (size_t i = 0; i < reg->runCount(); ++i) {
                row.Append(static_cast<Hlong>(reg->runAt(i).row));
                cb.Append(static_cast<Hlong>(reg->runAt(i).cb));
                ce.Append(static_cast<Hlong>(reg->runAt(i).ce));
            }
            GenRegionRuns(&r, row, cb, ce);
            if (!hoIsEmpty(r)) {
                return r;
            }
        }
        return fullImageRegion(imgW, imgH);
    }
    return fullImageRegion(imgW, imgH);
}


// 多框并成一只区域
HObject roisToRegion(const std::vector<HsCommData::HsObject>& rois, int imgW, int imgH)
{
    if (rois.empty()) {
        return fullImageRegion(imgW, imgH);
    }
    HObject acc;
    bool first = true;
    for (int i = 0; i < static_cast<int>(rois.size()); ++i) {
        const HObject one = roiToRegion(rois[static_cast<size_t>(i)], imgW, imgH);
        if (first) {
            acc = one;
            first = false;
        } else {
            HObject uni;
            Union2(acc, one, &uni);
            acc = uni;
        }
    }
    if (first) {
        return fullImageRegion(imgW, imgH);
    }
    return acc;
}

// 上游区域并成一只 HObject；空则整图（不额外约束）
HObject rleRegionsToHObject(const std::vector<HsCommData::HsObject>& regs, int imgW, int imgH)
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
            HTuple row;
            HTuple cb;
            HTuple ce;
            for (size_t k = 0; k < reg->runCount(); ++k) {
                const HsCommData::Chord& c = reg->runAt(k);
                row.Append(static_cast<Hlong>(c.row));
                cb.Append(static_cast<Hlong>(c.cb));
                ce.Append(static_cast<Hlong>(c.ce));
            }
            HObject one;
            GenRegionRuns(&one, row, cb, ce);
            if (hoIsEmpty(one)) {
                continue;
            }
            if (first) {
                acc = one;
                first = false;
            } else {
                HObject uni;
                Union2(acc, one, &uni);
                acc = uni;
            }
        }
    }
    if (first) {
        return fullImageRegion(imgW, imgH);
    }
    return acc;
}

// 本地 ROI ∪ 上游区域（空 ROI 列表表示无本地约束，勿与整幅图并集）
HObject searchDomainFromAlgIn(const std::vector<HsCommData::HsObject>& rois,
                                const std::vector<HsCommData::HsObject>& regions,
                                int imgW, int imgH)
{
    if (regions.empty()) {
        return roisToRegion(rois, imgW, imgH);
    }
    const HObject mask = rleRegionsToHObject(regions, imgW, imgH);
    if (rois.empty()) {
        return mask;
    }
    const HObject local = roisToRegion(rois, imgW, imgH);
    HObject uni;
    Union2(local, mask, &uni);
    if (hoIsEmpty(uni)) {
        return local;
    }
    return uni;
}

// GetRegionRuns 写成区域对象
HsCommData::HsObject regionObject(const HObject& r)
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

// 结果圆
HsCommData::HsObject circleObject(double cx, double cy, double r)
{
    HsCommData::CircleP c;
    c.c = HsCommData::Point2d(cx, cy);
    c.r = r;
    return HsCommData::HsObject(c);
}

// 结果线段
HsCommData::HsObject lineObject(double x1, double y1, double x2, double y2)
{
    HsCommData::LineP ln;
    ln.a = HsCommData::Point2d(x1, y1);
    ln.b = HsCommData::Point2d(x2, y2);
    return HsCommData::HsObject(ln);
}

// 匹配点：水平竖直线段组成十字
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

// Halcon 异常文本
std::string hExceptionText(const HalconCpp::HException& e)
{
    const char* t = e.ErrorMessage().Text();
    return t ? std::string(t) : std::string("Halcon error");
}

// 统一失败包
AlgToolOut algorToolFail(const std::string& msg)
{
    AlgToolOut o;
    o.ok = false;
    o.message = msg;
    return o;
}

}
