/**
 * @file HalMerge.cpp
 * @brief 合并区域并统计数量和面积（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalMerge.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "ToolGeom.h"
#include "ToolParaRstMerge.hpp"
#include <HsCommData/Graphic.h>
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "result.merge";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "首版" },
};
}

HalMerge::HalMerge()
    : area_(0.0)
    , count_(0)
    , min_area_(0.0)
    , max_count_(0)
    , param_ok_(true)
    , shot_err_(0)
{
}

AlgToolBase* HalMerge::clone() const
{
    return new HalMerge();
}

std::string HalMerge::tool_name() const
{
    return kName;
}

AlgLibKind HalMerge::alg_lib() const
{
    return AlgLibHal;
}

std::string HalMerge::version() const
{
    return kVer;
}

std::vector<VerNote> HalMerge::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

void HalMerge::init(const std::string&)
{
}

void HalMerge::save(const std::string&)
{
}

int HalMerge::tool_reset()
{
    merged_.clear();
    area_ = 0.0;
    count_ = 0;
    min_area_ = 0.0;
    max_count_ = 0;
    param_ok_ = true;
    tol_.reset();
    shot_err_ = 0;
    shot_msg_.clear();
    return HS_OK;
}

void HalMerge::collect_upstream(const AlgToolIn& in)
{
    merged_.clear();
    const std::vector<HsCommData::HsObject> regs = plainRegions(in.regions);
    merged_.insert(merged_.end(), regs.begin(), regs.end());
    const std::vector<HsCommData::HsObject> cons = plainContours(in.contours);
    merged_.insert(merged_.end(), cons.begin(), cons.end());
    for (size_t i = 0; i < in.rois.size(); ++i) {
        if (in.rois[i].roi.isInitialized() && !in.rois[i].roi.isEmpty()) {
            merged_.push_back(in.rois[i].roi);
        }
    }
    count_ = static_cast<int>(merged_.size());
}

bool HalMerge::union_regions(HalconCpp::HObject* out) const
{
    if (out == nullptr) {
        return false;
    }
    HalconCpp::GenEmptyObj(out);
    bool first = true;
    HalconCpp::HObject acc;
    for (size_t i = 0; i < merged_.size(); ++i) {
        const HsCommData::HsObject& obj = merged_[i];
        if (!obj.isInitialized()) {
            continue;
        }
        const int n = obj.count();
        for (int ei = 0; ei < n; ++ei) {
            if (obj.kindAt(ei) != HsCommData::ObjRegion) {
                continue;
            }
            const HsCommData::HsRegion* reg = obj.regionAt(ei);
            if (reg == nullptr || reg->runsEmpty()) {
                continue;
            }
            HalconCpp::HTuple row;
            HalconCpp::HTuple cb;
            HalconCpp::HTuple ce;
            for (size_t k = 0; k < reg->runCount(); ++k) {
                const HsCommData::Chord& c = reg->runAt(k);
                row.Append(static_cast<Hlong>(c.row));
                cb.Append(static_cast<Hlong>(c.cb));
                ce.Append(static_cast<Hlong>(c.ce));
            }
            HalconCpp::HObject one;
            HalconCpp::GenRegionRuns(&one, row, cb, ce);
            if (hoIsEmpty(one)) {
                continue;
            }
            if (first) {
                acc = one;
                first = false;
            } else {
                HalconCpp::HObject uni;
                HalconCpp::Union2(acc, one, &uni);
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

void HalMerge::measure_area()
{
    area_ = 0.0;
    HalconCpp::HObject uni;
    const bool haveUni = union_regions(&uni);
    if (haveUni) {
        HalconCpp::HTuple a;
        HalconCpp::HTuple r;
        HalconCpp::HTuple c;
        HalconCpp::AreaCenter(uni, &a, &r, &c);
        if (a.Length() > 0) {
            area_ += a[0].D();
        }
    }
    for (size_t i = 0; i < merged_.size(); ++i) {
        const HsCommData::HsObject& obj = merged_[i];
        if (obj.kind() == HsCommData::ObjRegion) {
            if (!haveUni && obj.regionAt(0) != nullptr) {
                area_ += static_cast<double>(obj.regionAt(0)->area());
            }
            continue;
        }
        const HsCommData::Graphic g = objectToGraphic(obj);
        const double w = g.boundsImage.w;
        const double h = g.boundsImage.h;
        if (w > 0.0 && h > 0.0) {
            area_ += w * h;
        }
    }
}

int HalMerge::proc_input(const AlgToolIn& in)
{
    ToolParaRstMerge local;
    const ToolParaRstMerge* para = paraOf(in, &local);
    if (in.param != nullptr && toolParaAs<ToolParaRstMerge>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    min_area_ = para->run.minArea;
    max_count_ = para->run.maxCount;
    collect_upstream(in);
    return HS_OK;
}

int HalMerge::alg_run(const AlgToolIn& in)
{
    (void)in;
    try {
        measure_area();
        param_ok_ = (area_ + 1e-9 >= min_area_);
        if (max_count_ > 0 && count_ > max_count_) {
            param_ok_ = false;
        }
        shot_err_ = HS_OK;
        if (count_ == 0) {
            shot_msg_ = "无上游缺陷区域";
        } else if (!param_ok_) {
            shot_msg_ = "面积或数量超限";
        } else {
            shot_msg_ = "已合并上游缺陷区域";
        }
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = hExceptionText(e);
        return shot_err_;
    }
}

int HalMerge::judge_tol(const AlgToolIn& in)
{
    ToolParaRstMerge local;
    const ToolParaRstMerge* para = paraOf(in, &local);
    tol_.reset();
    tol_.put_int(RstMergeKey::count, count_);
    tol_.put(RstMergeKey::area, area_);
    tol_.judge(para->judge, nullptr);
    if (!param_ok_) {
        tol_.ok = false;
    }
    return HS_OK;
}

int HalMerge::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
{
    (void)in;
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.runOk = shot_err_ == HS_OK;
    result.objects = merged_;
    if (!inputImage(in).isNull()) {
        result.image.image = inputImage(in);
    }
    result.message = shot_msg_;
    if (shot_err_ == HS_OK) {
        tol_.apply(&result);
        result.ok = param_ok_ && tol_.ok;
        result.judgeOk = tol_.ok;
    } else {
        result.ok = false;
    }
    *out = result;
    return shot_err_;
}

int HalMerge::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    try {
        run_timer_.start();
        tool_reset();
        run_timer_.mark("tool_reset");
        int step = proc_input(in);
        run_timer_.mark("proc_input");
        if (step == HS_OK) {
            step = alg_run(in);
            run_timer_.mark("alg_run");
        }
        if (step == HS_OK) {
            step = judge_tol(in);
            run_timer_.mark("judge_tol");
        }
        alg_rst_proc(in, out);
        run_timer_.stop("alg_rst_proc");
        write_alg_steps(out, run_timer_.sections(), run_timer_.totalMs());
        return step != HS_OK ? step : shot_err_;
    } catch (const HalconCpp::HException& e) {
        return fail(HS_ERR_OPERATOR, hExceptionText(e), out);
    }
}

}
