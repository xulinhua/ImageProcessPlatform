/**
 * @file HalFindRect.cpp
 * @brief 找矩形（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalFindRect.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "ParamKeys.h"
#include "TolShot.h"
#include "ToolParaFindRect.hpp"
#include <HsCommData/HsError.h>
#include <cmath>

namespace Vision {

namespace {
const char kVer_HalFindRect[] = "1.0.0.0";
const VerNoteRaw kNotes_HalFindRect[] = {
    { "1.0.0.0", "Initial version" },
};
}

std::string HalFindRect::version() const
{
    return kVer_HalFindRect;
}

// 版本变更，只追加
std::vector<VerNote> HalFindRect::versionNotes() const
{
    return verNotesFrom(kNotes_HalFindRect, static_cast<int>(sizeof(kNotes_HalFindRect) / sizeof(kNotes_HalFindRect[0])));
}

// 对一条轮廓拟合矩形
bool HalFindRect::fitOne(const HalconCpp::HObject& contour, Fit* fit) const
{
    HalconCpp::HTuple row;
    HalconCpp::HTuple col;
    HalconCpp::HTuple phi;
    HalconCpp::HTuple len1;
    HalconCpp::HTuple len2;
    HalconCpp::HTuple order;
    HalconCpp::HTuple dist;
    try {
        HalconCpp::FitRectangle2ContourXld(contour, "tukey", -1, 0, 0, 3, 2, &row, &col, &phi, &len1, &len2, &order);
        if (row.Length() < 1) {
            return false;
        }
        HalconCpp::DistRectangle2ContourPointsXld(contour, 0, row[0], col[0], phi[0], len1[0], len2[0], &dist);
    } catch (const HalconCpp::HException&) {
        return false;
    }
    double sum = 0;
    const Hlong n = dist.Length();
    for (Hlong i = 0; i < n; ++i) {
        sum += std::fabs(dist[i].D());
    }
    fit->row = row[0].D();
    fit->col = col[0].D();
    fit->phi = phi[0].D();
    fit->len1 = len1[0].D();
    fit->len2 = len2[0].D();
    fit->dist = n > 0 ? sum / static_cast<double>(n) : 0.0;
    return true;
}

// 找矩形
int HalFindRect::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = AlgToolOut();
    try {
        SectionTimer timer;
        timer.start();
        HalconCpp::HObject gray;
        HalconCpp::HObject* pGray = &gray;
        int w = 0;
        int h = 0;
        std::string err;
        if (!inputToGray(in.imageNative.ptr, static_cast<int>(in.imageNative.algLib), inputImage(in), pGray, &w, &h, &err)
            || w < 1 || h < 1) {
            return fail(HS_ERR_IMAGE_EMPTY, err.empty() ? std::string("no input image") : err, out);
        }
        ToolParaFindRect local;
        const ToolParaFindRect* para = paraOf(in, &local);
        HalconCpp::HObject reduced;
        HalconCpp::ReduceDomain(gray, toolRoisToHObject(plainRois(in.rois), w, h), &reduced);
        timer.mark("proc_input");

        // 边缘先连成长轮廓，太短的不拟合
        HalconCpp::HObject edges;
        HalconCpp::EdgesSubPix(reduced, &edges, "canny", para->run.sigma,
                               static_cast<Hlong>(para->run.low), static_cast<Hlong>(para->run.high));
        HalconCpp::HObject joined;
        HalconCpp::UnionAdjacentContoursXld(edges, &joined, 10, 1, "attr_keep");
        HalconCpp::HObject candidates;
        HalconCpp::SelectContoursXld(joined, &candidates, "contour_length", 4.0 * para->run.minSide, 1e9, -0.5, 0.5);
        HalconCpp::HTuple n;
        HalconCpp::CountObj(candidates, &n);
        const Hlong count = n.Length() > 0 ? n[0].L() : 0;
        Fit best;
        bool found = false;
        for (Hlong i = 1; i <= count; ++i) {
            HalconCpp::HObject one;
            HalconCpp::SelectObj(candidates, &one, i);
            Fit fit;
            if (!fitOne(one, &fit)) {
                continue;
            }
            const double shortSide = 2.0 * (fit.len1 < fit.len2 ? fit.len1 : fit.len2);
            const double longSide = 2.0 * (fit.len1 > fit.len2 ? fit.len1 : fit.len2);
            if (shortSide < para->run.minSide || fit.dist > para->run.maxDist) {
                continue;
            }
            if (para->run.maxSide > 0.0 && longSide > para->run.maxSide) {
                continue;
            }
            if (!found || fit.len1 * fit.len2 > best.len1 * best.len2) {
                best = fit;
                found = true;
            }
        }
        timer.mark("alg_run");
        if (!found) {
            return fail(HS_ERR_RECT_NONE, "no rectangle found", out);
        }

        HsCommData::Pose2 pose;
        pose.tx = best.col;
        pose.ty = best.row;
        pose.setAngRad(best.phi);
        out->poses.push_back(pose);
        HsCommData::Rect2P rect;
        rect.c = HsCommData::Point2d(best.col, best.row);
        rect.l1 = best.len1;
        rect.l2 = best.len2;
        rect.phi = best.phi;
        out->objects.push_back(HsCommData::HsObject(rect));
        out->objects.push_back(crossObject(best.col, best.row));

        TolShot tol;
        paramSet(&tol.values, ValueKey::X, best.col);
        paramSet(&tol.values, ValueKey::Y, best.row);
        paramSet(&tol.values, ValueKey::Angle, best.phi);
        paramSet(&tol.values, ValueKey::Score, 1.0 / (1.0 + best.dist));
        tol.put(FindRectKey::width, 2.0 * best.len1);
        tol.put(FindRectKey::height, 2.0 * best.len2);
        tol.put(FindRectKey::dist, best.dist);
        tol.judge(para->judge, nullptr);
        tol.apply(out);
        out->ok = tol.ok;
        out->errCode = HS_OK;
        out->message = tol.ok ? "Find rect done" : "Find rect out of tolerance";
        timer.stop("alg_rst_proc");
        write_alg_steps(out, timer.sections(), timer.totalMs());
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        return fail(HS_ERR_RECT_OPERATOR, hExceptionText(e), out);
    }
}

}
