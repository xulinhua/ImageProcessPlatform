/**
 * @file HalStitch.cpp
 * @brief 把多张图拼成一张（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalStitch.h"
#include "HalImageConvert.h"
#include "HalOps.h"
#include "ToolParaStitch.hpp"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "preprocess.stitch";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "Initial version" },
};
}

HalStitch::HalStitch()
    : count_(0)
    , columns_(0)
    , frame_id_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&tiled_);
}

AlgToolBase* HalStitch::clone() const
{
    return new HalStitch();
}

std::string HalStitch::tool_name() const
{
    return kName;
}

AlgLibKind HalStitch::alg_lib() const
{
    return AlgLibHal;
}

std::string HalStitch::version() const
{
    return kVer;
}

std::vector<VerNote> HalStitch::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

int HalStitch::tool_reset()
{
    tiles_.clear();
    widths_.clear();
    heights_.clear();
    HalconCpp::GenEmptyObj(&tiled_);
    count_ = 0;
    columns_ = 0;
    frame_id_ = 0;
    tol_.reset();
    shot_err_ = 0;
    shot_msg_.clear();
    return HS_OK;
}

int HalStitch::collect_images(const AlgToolIn& in)
{
    tiles_.clear();
    widths_.clear();
    heights_.clear();
    frame_id_ = inputImage(in).frameId;
    std::string err;
    const HsCommData::HsImage& first = inputImage(in);
    if (in.imageNative.ptr != nullptr || !first.isNull()) {
        HalconCpp::HObject one;
        HalconCpp::HObject* view = &one;
        if (!inputToHObject(in.imageNative.ptr, static_cast<int>(in.imageNative.algLib), first, view, &err)) {
            shot_err_ = HS_ERR_STITCH_OPERATOR;
            shot_msg_ = err;
            return shot_err_;
        }
        tiles_.push_back(one);
    }
    for (size_t i = 0; i < in.images.size(); ++i) {
        const PortImage& item = in.images[i];
        if (item.native.ptr == nullptr && item.image.isNull()) {
            continue;
        }
        HalconCpp::HObject one;
        HalconCpp::HObject* view = &one;
        if (!inputToHObject(item.native.ptr, static_cast<int>(item.native.algLib), item.image, view, &err)) {
            shot_err_ = HS_ERR_STITCH_OPERATOR;
            shot_msg_ = err;
            return shot_err_;
        }
        tiles_.push_back(one);
    }
    if (tiles_.size() < 2) {
        shot_err_ = HS_ERR_STITCH_FEW;
        shot_msg_ = HsCommData::HsException(HS_ERR_STITCH_FEW).errorMessage();
        return shot_err_;
    }
    return HS_OK;
}

void HalStitch::unify_channels()
{
    if (tiles_.empty()) {
        return;
    }
    std::vector<Hlong> channels;
    bool mixed = false;
    for (size_t i = 0; i < tiles_.size(); ++i) {
        HalconCpp::HTuple ch;
        HalconCpp::CountChannels(tiles_[i], &ch);
        channels.push_back(ch.Length() > 0 ? ch[0].L() : 1);
        if (channels.back() != channels.front()) {
            mixed = true;
        }
    }
    if (!mixed) {
        return;
    }
    for (size_t i = 0; i < tiles_.size(); ++i) {
        if (channels[i] >= 3) {
            HalconCpp::HObject gray;
            HalconCpp::Rgb1ToGray(tiles_[i], &gray);
            tiles_[i] = gray;
        }
    }
}

int HalStitch::proc_input(const AlgToolIn& in)
{
    ToolParaStitch local;
    const ToolParaStitch* para = paraOf(in, &local);
    if (in.param != nullptr && toolParaAs<ToolParaStitch>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    const int step = collect_images(in);
    if (step != HS_OK) {
        return step;
    }
    count_ = static_cast<int>(tiles_.size());
    columns_ = para->run.columns;
    if (columns_ < 1 || columns_ > count_) {
        columns_ = count_;
    }
    unify_channels();
    return HS_OK;
}

int HalStitch::alg_run(const AlgToolIn& in)
{
    (void)in;
    try {
        const int rows = (count_ + columns_ - 1) / columns_;
        widths_.resize(static_cast<size_t>(count_));
        heights_.resize(static_cast<size_t>(count_));
        std::vector<int> rowHeight(static_cast<size_t>(rows), 0);
        std::vector<int> colWidth(static_cast<size_t>(columns_), 0);
        HalconCpp::HObject all;
        HalconCpp::GenEmptyObj(&all);
        for (int i = 0; i < count_; ++i) {
            HalconCpp::HTuple tw;
            HalconCpp::HTuple th;
            HalconCpp::GetImageSize(tiles_[static_cast<size_t>(i)], &tw, &th);
            const int iw = static_cast<int>(tw[0].L());
            const int ih = static_cast<int>(th[0].L());
            widths_[static_cast<size_t>(i)] = iw;
            heights_[static_cast<size_t>(i)] = ih;
            int& rh = rowHeight[static_cast<size_t>(i / columns_)];
            int& cw = colWidth[static_cast<size_t>(i % columns_)];
            rh = ih > rh ? ih : rh;
            cw = iw > cw ? iw : cw;
            HalconCpp::HObject next;
            HalconCpp::ConcatObj(all, tiles_[static_cast<size_t>(i)], &next);
            all = next;
        }
        std::vector<int> rowAt(static_cast<size_t>(rows), 0);
        std::vector<int> colAt(static_cast<size_t>(columns_), 0);
        int totalH = 0;
        int totalW = 0;
        for (int r = 0; r < rows; ++r) {
            rowAt[static_cast<size_t>(r)] = totalH;
            totalH += rowHeight[static_cast<size_t>(r)];
        }
        for (int c = 0; c < columns_; ++c) {
            colAt[static_cast<size_t>(c)] = totalW;
            totalW += colWidth[static_cast<size_t>(c)];
        }
        HalconCpp::HTuple offsetRow;
        HalconCpp::HTuple offsetCol;
        HalconCpp::HTuple row1;
        HalconCpp::HTuple col1;
        HalconCpp::HTuple row2;
        HalconCpp::HTuple col2;
        for (int i = 0; i < count_; ++i) {
            offsetRow.Append(rowAt[static_cast<size_t>(i / columns_)]);
            offsetCol.Append(colAt[static_cast<size_t>(i % columns_)]);
            row1.Append(0);
            col1.Append(0);
            row2.Append(heights_[static_cast<size_t>(i)] - 1);
            col2.Append(widths_[static_cast<size_t>(i)] - 1);
        }
        HalconCpp::TileImagesOffset(all, &tiled_, offsetRow, offsetCol, row1, col1, row2, col2, totalW, totalH);
        shot_err_ = HS_OK;
        shot_msg_ = "拼图完成";
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        shot_err_ = HS_ERR_STITCH_OPERATOR;
        shot_msg_ = hExceptionText(e);
        return shot_err_;
    }
}

int HalStitch::judge_tol(const AlgToolIn& in)
{
    ToolParaStitch local;
    const ToolParaStitch* para = paraOf(in, &local);
    tol_.reset();
    tol_.put_int(StitchKey::count, count_);
    tol_.put_int(StitchKey::columns, columns_);
    tol_.judge(para->judge, nullptr);
    return HS_OK;
}

int HalStitch::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
{
    (void)in;
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.runOk = shot_err_ == HS_OK;
    result.ok = shot_err_ == HS_OK && tol_.ok;
    result.message = shot_msg_;
    if (shot_err_ == HS_OK) {
        std::string err;
        HsCommData::HsImage* image = &result.image.image;
        if (!hObjectToHsImage(tiled_, frame_id_, image, &err)) {
            result.ok = false;
            result.runOk = false;
            result.errCode = HS_ERR_STITCH_OPERATOR;
            result.message = err;
            *out = result;
            return HS_ERR_STITCH_OPERATOR;
        }
        if (!tol_.ok) {
            result.message = "拼图测值超公差";
        }
        tol_.apply(&result);
        result.ok = shot_err_ == HS_OK && tol_.ok;
    }
    *out = result;
    return shot_err_;
}

int HalStitch::execute(const AlgToolIn& in, AlgToolOut* out)
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
        return fail(HS_ERR_STITCH_OPERATOR, hExceptionText(e), out);
    }
}

}
