/**
 * @file HalMerge.cpp
 * @brief HalMerge实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalMerge.h"
#include "TextEncoding.h"
#include "ToolGeom.h"
#include "HalAlgoHelpers.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"

using HalconCpp::HObject;
using HalconCpp::HTuple;
using HalconCpp::HException;
using HalconCpp::GenRegionRuns;
using HalconCpp::Union2;
using HalconCpp::AreaCenter;

namespace Vision {

namespace {

// 把区域对象并成一只 HObject；没有则返回空
bool unionRegionObjects(const std::vector<HsCommData::HsObject>& objs, HObject* out)
{
    if (!out) {
        return false;
    }
    bool first = true;
    HObject acc;
    for (size_t i = 0; i < objs.size(); ++i) {
        const HsCommData::HsObject& obj = objs[i];
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
        return false;
    }
    *out = acc;
    return true;
}

}

// 合并上游区域、轮廓与 ROI
namespace {
const char kVer_HalMerge[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalMerge[] = {
    { "1.0.0.0", "首版" },
};
}

std::string HalMerge::version() const
{
    return kVer_HalMerge;
}

// 版本变更，只追加
std::vector<VerNote> HalMerge::versionNotes() const
{
    return verNotesFrom(kNotes_HalMerge, static_cast<int>(sizeof(kNotes_HalMerge) / sizeof(kNotes_HalMerge[0])));
}
AlgToolOut HalMerge::run(const AlgToolIn& in)
{
    try {
        AlgToolOut out;
        if (!in.image.image.isNull()) {
            out.image.image = in.image.image;
        }
        std::vector<HsCommData::HsObject> merged;
        { const std::vector<HsCommData::HsObject> regs = plainRegions(in.regions); merged.insert(merged.end(), regs.begin(), regs.end()); }
        { const std::vector<HsCommData::HsObject> cons = plainContours(in.contours); merged.insert(merged.end(), cons.begin(), cons.end()); }
        for (size_t i = 0; i < in.rois.size(); ++i) {
            if (in.rois[i].roi.isInitialized() && !in.rois[i].roi.isEmpty()) {
                merged.push_back(in.rois[i].roi);
            }
        }

        double area = 0.0;
        HObject uni;
        const bool haveUni = unionRegionObjects(merged, &uni);
        if (haveUni) {
            HTuple a;
            HTuple r;
            HTuple c;
            AreaCenter(uni, &a, &r, &c);
            if (a.Length() > 0) {
                area += a[0].D();
            }
        }
        for (size_t i = 0; i < merged.size(); ++i) {
            const HsCommData::HsObject& obj = merged[i];
            if (obj.kind() == HsCommData::ObjRegion) {
                if (!haveUni && obj.regionAt(0)) {
                    area += static_cast<double>(obj.regionAt(0)->area());
                }
                continue;
            }
            const HsCommData::Graphic g = objectToGraphic(obj);
            const double w = g.boundsImage.w;
            const double h = g.boundsImage.h;
            if (w > 0.0 && h > 0.0) {
                area += w * h;
            }
        }

        const int count = static_cast<int>(merged.size());
        const double minArea = in.run.value("minArea").toDouble();
        const int maxCount = in.run.value("maxCount").toInt();
        bool ok = (area + 1e-9 >= minArea);
        if (maxCount > 0 && count > maxCount) {
            ok = false;
        }
        out.objects = merged;
        out.values.insert("count", count);
        out.values.insert("area", area);
        out.ok = ok;
        if (count == 0) {
            out.message = Vision::acpToUtf8("无上游缺陷区域");
        } else if (!ok) {
            out.message = Vision::acpToUtf8("面积或数量超限");
        } else {
            out.message = Vision::acpToUtf8("已合并上游缺陷区域");
        }
        return out;
    } catch (const HException& e) {
        return algorToolFail(hExceptionText(e));
    }
}

}
