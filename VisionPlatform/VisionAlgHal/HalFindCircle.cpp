/**
 * @file HalFindCircle.cpp
 * @brief HalFindCircle实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalFindCircle.h"
#include "TextEncoding.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "ParamKeys.h"

namespace Vision {

// 亚像素圆
namespace {
const char kVer_HalFindCircle[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalFindCircle[] = {
    { "1.0.0.0", "首版" },
};
}

std::string HalFindCircle::version() const
{
    return kVer_HalFindCircle;
}

// 版本变更，只追加
std::vector<VerNote> HalFindCircle::versionNotes() const
{
    return verNotesFrom(kNotes_HalFindCircle, static_cast<int>(sizeof(kNotes_HalFindCircle) / sizeof(kNotes_HalFindCircle[0])));
}
AlgToolOut HalFindCircle::run(const AlgToolIn& in)
{
    try {
        HalconCpp::HImage gray;
        int w = 0;
        int h = 0;
        std::string err;
        HalconCpp::HImage* pGray = &gray;
        if (!hsImageToGray(inputImage(in), pGray, &w, &h, &err)) {
            return AlgToolOut::fail(err);
        }
        const double sigma = in.run.value("sigma").toDouble(1.0);
        const double low = in.run.value("low").toDouble(20.0);
        const double high = in.run.value("high").toDouble(40.0);
        const double minR = in.run.value("minR").toDouble(8.0);
        const double maxR = in.run.value("maxR").toDouble(400.0);
        // 搜索区内提亚像素边缘，再切成直线段和圆弧段
        HalconCpp::HObject local;
        HalconCpp::ReduceDomain(gray, toolRoisToHObject(plainRois(in.rois), w, h), &local);
        HalconCpp::HObject edges;
        HalconCpp::EdgesSubPix(local, &edges, "canny", sigma < 0.4 ? 0.4 : sigma,
                               static_cast<Hlong>(low), static_cast<Hlong>(high));
        HalconCpp::HObject segs;
        HalconCpp::SegmentContoursXld(edges, &segs, "lines_circles", 5.0, 4.0, 2.0);
        HalconCpp::HTuple n;
        HalconCpp::CountObj(segs, &n);
        const Hlong nObj = n.Length() > 0 ? n[0].L() : 0;
        // 每段都拟合圆（不区分线段与圆弧），拟合失败跳过，保留半径在范围内最大的
        double bestR = 0;
        double bestCx = 0;
        double bestCy = 0;
        bool found = false;
        for (Hlong i = 1; i <= nObj; ++i) {
            HalconCpp::HObject one;
            HalconCpp::SelectObj(segs, &one, i);
            HalconCpp::HTuple row;
            HalconCpp::HTuple col;
            HalconCpp::HTuple radius;
            HalconCpp::HTuple startPhi;
            HalconCpp::HTuple endPhi;
            HalconCpp::HTuple pointOrder;
            try {
                HalconCpp::FitCircleContourXld(one, "algebraic", -1, 0.0, 0, 3, 2.0, &row, &col, &radius,
                                               &startPhi, &endPhi, &pointOrder);
            } catch (const HalconCpp::HException&) {
                continue;
            }
            if (radius.Length() < 1 || col.Length() < 1 || row.Length() < 1) {
                continue;
            }
            const double rVal = radius[0].D();
            if (rVal < minR || rVal > maxR) {
                continue;
            }
            if (!found || rVal > bestR) {
                found = true;
                bestR = rVal;
                bestCx = col[0].D();
                bestCy = row[0].D();
            }
        }
        AlgToolOut out;
        out.image.image = inputImage(in);
        if (!found) {
            out.message = "未找到圆";
            return out;
        }
        // 位姿角度固定 0，分数固定 1
        out.ok = true;
        out.values.insert("cx", bestCx);
        out.values.insert("cy", bestCy);
        out.values.insert("r", bestR);
        paramSet(&out.values, ValueKey::X, bestCx);
        paramSet(&out.values, ValueKey::Y, bestCy);
        paramSet(&out.values, ValueKey::Angle, 0.0);
        paramSet(&out.values, ValueKey::Score, 1.0);
        out.objects.push_back(circleObject(bestCx, bestCy, bestR));
        out.message = "找圆完成";
        return out;
    } catch (const HalconCpp::HException& e) {
        return AlgToolOut::fail(hExceptionText(e));
    }
}

}
