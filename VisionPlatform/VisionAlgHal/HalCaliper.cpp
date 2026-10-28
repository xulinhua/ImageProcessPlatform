/**
 * @file HalCaliper.cpp
 * @brief HalCaliper实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalCaliper.h"
#include "TextEncoding.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "ParamKeys.h"
#include <cmath>

namespace Vision {

// 卡尺 ROI 转测量矩形参数
static bool caliperRect(const HsCommData::HsObject& roi, double* row, double* col, double* phi,
                        double* l1, double* l2)
{
    if (!roi.isInitialized() || roi.isEmpty()) {
        return false;
    }
    const HsCommData::ObjectKind k = roi.kind();
    if (k == HsCommData::ObjRect2) {
        const HsCommData::Rect2P* rc = roi.rect2At(0);
        *row = rc->c.y;
        *col = rc->c.x;
        *phi = rc->phi;
        *l1 = rc->l1;
        *l2 = rc->l2;
        return *l1 > 1.0 && *l2 > 0.5;
    }
    if (k == HsCommData::ObjEllipse) {
        const HsCommData::EllipseP* e = roi.ellipseAt(0);
        *row = e->c.y;
        *col = e->c.x;
        *phi = e->phi;
        *l1 = e->ra;
        *l2 = e->rb;
        return *l1 > 1.0 && *l2 > 0.5;
    }
    if (k == HsCommData::ObjRect1) {
        const HsCommData::Rect1P* rc = roi.rect1At(0);
        *col = rc->r.x + rc->r.w * 0.5;
        *row = rc->r.y + rc->r.h * 0.5;
        *phi = 0.0;
        *l1 = rc->r.w * 0.5;
        *l2 = rc->r.h * 0.5;
        return *l1 > 1.0 && *l2 > 0.5;
    }
    if (k == HsCommData::ObjLine) {
        const HsCommData::LineP* ln = roi.lineAt(0);
        *col = (ln->a.x + ln->b.x) * 0.5;
        *row = (ln->a.y + ln->b.y) * 0.5;
        *phi = std::atan2(ln->b.y - ln->a.y, ln->b.x - ln->a.x);
        const double dx = ln->b.x - ln->a.x;
        const double dy = ln->b.y - ln->a.y;
        *l1 = 0.5 * std::sqrt(dx * dx + dy * dy);
        *l2 = 8.0;
        return *l1 > 1.0;
    }
    return false;
}

// 测宽或找边
namespace {
const char kVer_HalCaliper[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalCaliper[] = {
    { "1.0.0.0", "首版" },
};
}

std::string HalCaliper::version() const
{
    return kVer_HalCaliper;
}

// 版本变更，只追加
std::vector<VerNote> HalCaliper::versionNotes() const
{
    return verNotesFrom(kNotes_HalCaliper, static_cast<int>(sizeof(kNotes_HalCaliper) / sizeof(kNotes_HalCaliper[0])));
}
AlgToolOut HalCaliper::run(const AlgToolIn& in)
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
        const std::string mode = in.run.value("mode").toString();
        const double sigma = in.run.value("sigma").toDouble(1.0);
        const double thresh = in.run.value("threshold").toDouble(20.0);
        std::string trans = in.run.value("transition").toString();
        if (trans.empty()) {
            trans = "all";
        }
        std::string sel = in.run.value("select").toString();
        if (sel.empty()) {
            sel = "all";
        }
        const std::vector<HsCommData::HsObject> rois = plainRois(in.rois);
        const int toolCount = in.run.value("searchCount").toInt(1);
        AlgToolOut out;
        out.image.image = inputImage(in);
        int found = 0;
        double lastWidth = 0;
        double lastX = 0;
        double lastY = 0;
        for (int i = 0; i < static_cast<int>(rois.size()); ++i) {
            double row = 0;
            double col = 0;
            double phi = 0;
            double l1 = 0;
            double l2 = 0;
            if (!caliperRect(rois[static_cast<size_t>(i)], &row, &col, &phi, &l1, &l2)) {
                continue;
            }
            HalconCpp::HMeasure m;
            m.GenMeasureRectangle2(row, col, phi, l1, l2, w, h, "nearest_neighbor");
            if (mode == "edge") {
                HalconCpp::HTuple rowE;
                HalconCpp::HTuple colE;
                HalconCpp::HTuple amp;
                HalconCpp::HTuple dist;
                gray.MeasurePos(m, sigma, thresh, trans.c_str(), sel.c_str(),
                                &rowE, &colE, &amp, &dist);
                const int want = toolCount > 0 ? toolCount : 1;
                const int n = static_cast<int>(rowE.Length());
                const int take = n < want ? n : want;
                for (int k = 0; k < take; ++k) {
                    const double cx = colE[k].D();
                    const double cy = rowE[k].D();
                    out.objects.push_back(crossObject(cx, cy));
                    lastX = cx;
                    lastY = cy;
                    ++found;
                }
            } else {
                HalconCpp::HTuple rowA;
                HalconCpp::HTuple colA;
                HalconCpp::HTuple ampA;
                HalconCpp::HTuple rowB;
                HalconCpp::HTuple colB;
                HalconCpp::HTuple ampB;
                HalconCpp::HTuple width;
                HalconCpp::HTuple intra;
                gray.MeasurePairs(m, sigma, thresh, trans.c_str(), sel.c_str(),
                                  &rowA, &colA, &ampA, &rowB, &colB, &ampB, &width, &intra);
                const int want = toolCount > 0 ? toolCount : 1;
                const int n = static_cast<int>(width.Length());
                const int take = n < want ? n : want;
                for (int k = 0; k < take; ++k) {
                    out.objects.push_back(lineObject(colA[k].D(), rowA[k].D(),
                                                       colB[k].D(), rowB[k].D()));
                    lastWidth = width[k].D();
                    lastX = 0.5 * (colA[k].D() + colB[k].D());
                    lastY = 0.5 * (rowA[k].D() + rowB[k].D());
                    ++found;
                }
            }
        }
        if (found <= 0) {
            out.message = "卡尺未测到边缘";
            return out;
        }
        out.ok = true;
        out.values.insert("count", found);
        out.values.insert("width", lastWidth);
        paramSet(&out.values, ValueKey::X, lastX);
        paramSet(&out.values, ValueKey::Y, lastY);
        paramSet(&out.values, ValueKey::Angle, 0.0);
        paramSet(&out.values, ValueKey::Score, 1.0);
        out.message = "卡尺完成";
        return out;
    } catch (const HalconCpp::HException& e) {
        return AlgToolOut::fail(hExceptionText(e));
    }
}

}
