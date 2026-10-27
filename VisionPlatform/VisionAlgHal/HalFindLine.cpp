/**
 * @file HalFindLine.cpp
 * @brief HalFindLine实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalFindLine.h"
#include "TextEncoding.h"
#include "HalAlgoHelpers.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "ParamKeys.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Vision {

struct FittedLine {
    double x1;      ///< 起点列
    double y1;      ///< 起点行
    double x2;      ///< 终点列
    double y2;      ///< 终点行
    double length;  ///< 长度
};

// 按长度从大到小
static bool longerLine(const FittedLine& a, const FittedLine& b)
{
    return a.length > b.length;
}

// 每只 ROI 内拟合直线
namespace {
const char kVer_HalFindLine[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalFindLine[] = {
    { "1.0.0.0", "首版" },
};
}

std::string HalFindLine::version() const
{
    return kVer_HalFindLine;
}

// 版本变更，只追加
std::vector<VerNote> HalFindLine::versionNotes() const
{
    return verNotesFrom(kNotes_HalFindLine, static_cast<int>(sizeof(kNotes_HalFindLine) / sizeof(kNotes_HalFindLine[0])));
}
AlgToolOut HalFindLine::run(const AlgToolIn& in)
{
    try {
        HalconCpp::HImage gray;
        int w = 0;
        int h = 0;
        std::string err;
        HalconCpp::HImage* pGray = &gray;
        if (!loadGrayFromAlgIn(in, pGray, &w, &h, &err)) {
            return algorToolFail(err);
        }
        const double sigma = in.run.value("sigma").toDouble(1.0);
        const double low = in.run.value("low").toDouble(20.0);
        const double high = in.run.value("high").toDouble(40.0);
        const double minLen = in.run.value("minLength").toDouble(20.0);
        const int toolCount = in.run.value("searchCount").toInt(1);
        const std::string select = in.run.value("select").toString();
        std::vector<HsCommData::HsObject> rois = plainRois(in.rois);
        if (rois.empty()) {
            HsCommData::HsObject whole;
            HsCommData::Rect1P rc;
            rc.r = HsCommData::Box2(0, 0, w, h);
            whole = rc;
            rois.push_back(whole);
        }
        AlgToolOut out;
        out.image.image = in.image.image;
        std::vector<FittedLine> all;
        for (int ri = 0; ri < static_cast<int>(rois.size()); ++ri) {
            const HsCommData::HsObject& roi = rois[static_cast<size_t>(ri)];
            const int want = toolCount > 0 ? toolCount : 1;
            HalconCpp::HObject local;
            HalconCpp::ReduceDomain(gray, toolRoiToHObject(roi, w, h), &local);
            HalconCpp::HObject edges;
            HalconCpp::EdgesSubPix(local, &edges, "canny", sigma < 0.4 ? 0.4 : sigma,
                                   static_cast<Hlong>(low), static_cast<Hlong>(high));
            HalconCpp::HObject segs;
            HalconCpp::SegmentContoursXld(edges, &segs, "lines", 5.0, 4.0, 2.0);
            HalconCpp::HTuple n;
            HalconCpp::CountObj(segs, &n);
            std::vector<FittedLine> found;
            const Hlong nObj = n.Length() > 0 ? n[0].L() : 0;
            for (Hlong i = 1; i <= nObj; ++i) {
                HalconCpp::HObject one;
                HalconCpp::SelectObj(segs, &one, i);
                HalconCpp::HTuple len;
                HalconCpp::LengthXld(one, &len);
                if (len.Length() < 1 || len[0].D() < minLen) {
                    continue;
                }
                HalconCpp::HTuple rowBegin;
                HalconCpp::HTuple colBegin;
                HalconCpp::HTuple rowEnd;
                HalconCpp::HTuple colEnd;
                HalconCpp::HTuple nr;
                HalconCpp::HTuple nc;
                HalconCpp::HTuple dist;
                HalconCpp::FitLineContourXld(one, "tukey", -1, 0, 5, 2, &rowBegin, &colBegin, &rowEnd, &colEnd,
                                             &nr, &nc, &dist);
                FittedLine fl;
                fl.x1 = colBegin[0].D();
                fl.y1 = rowBegin[0].D();
                fl.x2 = colEnd[0].D();
                fl.y2 = rowEnd[0].D();
                const double dx = fl.x2 - fl.x1;
                const double dy = fl.y2 - fl.y1;
                fl.length = std::sqrt(dx * dx + dy * dy);
                found.push_back(fl);
            }
            std::sort(found.begin(), found.end(), longerLine);
            if (select == "first" && !found.empty()) {
                FittedLine keep = found[0];
                found.clear();
                found.push_back(keep);
            } else if (select == "last" && !found.empty()) {
                FittedLine keep = found[found.size() - 1];
                found.clear();
                found.push_back(keep);
            }
            const int take = static_cast<int>(found.size()) < want ? static_cast<int>(found.size()) : want;
            for (int k = 0; k < take; ++k) {
                all.push_back(found[static_cast<size_t>(k)]);
            }
        }
        if (all.empty()) {
            out.message = Vision::acpToUtf8("未找到直线");
            return out;
        }
        out.ok = true;
        out.values.insert("count", static_cast<int>(all.size()));
        paramSet(&out.values, ValueKey::X, (all[0].x1 + all[0].x2) * 0.5);
        paramSet(&out.values, ValueKey::Y, (all[0].y1 + all[0].y2) * 0.5);
        paramSet(&out.values, ValueKey::Angle, std::atan2(all[0].y2 - all[0].y1, all[0].x2 - all[0].x1));
        paramSet(&out.values, ValueKey::Score, 1.0);
        HalconCpp::HObject lineMask;
        bool maskFirst = true;
        for (int i = 0; i < static_cast<int>(all.size()); ++i) {
            const FittedLine& fl = all[static_cast<size_t>(i)];
            out.objects.push_back(lineObject(fl.x1, fl.y1, fl.x2, fl.y2));
            HalconCpp::HObject strip;
            HalconCpp::GenRegionLine(&strip, fl.y1, fl.x1, fl.y2, fl.x2);
            HalconCpp::HObject dil;
            HalconCpp::DilationCircle(strip, &dil, 8.0);
            strip = dil;
            if (maskFirst) {
                lineMask = strip;
                maskFirst = false;
            } else {
                HalconCpp::HObject uni;
                HalconCpp::Union2(lineMask, strip, &uni);
                lineMask = uni;
            }
        }
        // 供下游 Blob「区域」端口使用（KindLine 只进 rois）
        if (!maskFirst && !hoIsEmpty(lineMask)) {
            out.objects.push_back(hObjectToRegionObject(lineMask));
        }
        out.message = Vision::acpToUtf8("找线完成");
        return out;
    } catch (const HalconCpp::HException& e) {
        return algorToolFail(hExceptionText(e));
    }
}

}
