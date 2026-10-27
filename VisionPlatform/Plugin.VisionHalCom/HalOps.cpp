/**
 * @file HalOps.cpp
 * @brief HalOps实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalOps.h"

#include "HalObjectConvert.h"

#include <cmath>

using HalconCpp::HObject;
using HalconCpp::HImage;
using HalconCpp::HTuple;
using HalconCpp::OpeningCircle;
using HalconCpp::ClosingCircle;
using HalconCpp::ReduceDomain;
using HalconCpp::GenRectangle1;
using HalconCpp::EdgesSubPix;
using HalconCpp::SegmentContoursXld;
using HalconCpp::CountObj;
using HalconCpp::SelectObj;
using HalconCpp::LengthXld;
using HalconCpp::FitLineContourXld;

namespace Vision {

// 区域开闭运算。空域直接返回；openR/closeR ≤0.4 跳过对应步
void morphOpenClose(const HObject& bin, double openR, double closeR, HObject*& out)
{
    if (!out) {
        return;
    }
    *out = bin;
    if (hoIsEmpty(*out)) {
        return;
    }
    if (openR > 0.4) {
        HObject tmp;
        OpeningCircle(*out, &tmp, openR);
        *out = tmp;
    }
    if (closeR > 0.4 && !hoIsEmpty(*out)) {
        HObject tmp;
        ClosingCircle(*out, &tmp, closeR);
        *out = tmp;
    }
}

// 在矩形测量区域内拟合直线（edges_sub_pix + fit_line_contour_xld）
bool measureLineRect(const HImage& image,
                     double row1, double col1, double row2, double col2,
                     double sigma, double low, double high,
                     double*& outRow1, double*& outCol1, double*& outRow2, double*& outCol2)
{
    if (!outRow1 || !outCol1 || !outRow2 || !outCol2) {
        return false;
    }
    try {
        HObject rect;
        GenRectangle1(&rect, row1, col1, row2, col2);
        HObject local;
        ReduceDomain(image, rect, &local);
        HObject edges;
        const double sig = sigma < 0.4 ? 0.4 : sigma;
        EdgesSubPix(local, &edges, "canny", sig, static_cast<Hlong>(low), static_cast<Hlong>(high));
        HObject segs;
        SegmentContoursXld(edges, &segs, "lines", 5.0, 4.0, 2.0);
        HTuple n;
        CountObj(segs, &n);
        const Hlong nObj = n.Length() > 0 ? n[0].L() : 0;
        if (nObj < 1) {
            return false;
        }
        double bestLen = -1.0;
        double br1 = 0;
        double bc1 = 0;
        double br2 = 0;
        double bc2 = 0;
        for (Hlong i = 1; i <= nObj; ++i) {
            HObject one;
            SelectObj(segs, &one, i);
            HTuple len;
            LengthXld(one, &len);
            if (len.Length() < 1) {
                continue;
            }
            HTuple rowBegin;
            HTuple colBegin;
            HTuple rowEnd;
            HTuple colEnd;
            HTuple nr;
            HTuple nc;
            HTuple dist;
            FitLineContourXld(one, "tukey", -1, 0, 5, 2, &rowBegin, &colBegin, &rowEnd, &colEnd,
                              &nr, &nc, &dist);
            if (rowBegin.Length() < 1 || colBegin.Length() < 1 || rowEnd.Length() < 1 || colEnd.Length() < 1) {
                continue;
            }
            const double dx = colEnd[0].D() - colBegin[0].D();
            const double dy = rowEnd[0].D() - rowBegin[0].D();
            const double L = std::sqrt(dx * dx + dy * dy);
            if (L > bestLen) {
                bestLen = L;
                br1 = rowBegin[0].D();
                bc1 = colBegin[0].D();
                br2 = rowEnd[0].D();
                bc2 = colEnd[0].D();
            }
        }
        if (bestLen < 0.0) {
            return false;
        }
        *outRow1 = br1;
        *outCol1 = bc1;
        *outRow2 = br2;
        *outCol2 = bc2;
        return true;
    } catch (const HalconCpp::HException&) {
        return false;
    }
}

}
