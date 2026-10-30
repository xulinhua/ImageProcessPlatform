/**
 * @file HalStitch.cpp
 * @brief 把多张图拼成一张（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalStitch.h"
#include "HalImageConvert.h"
#include "TolShot.h"
#include "ToolParaStitch.hpp"
#include <HsCommData/HsError.h>

namespace Vision {

namespace {
const char kVer_HalStitch[] = "1.0.0.0";
const VerNoteRaw kNotes_HalStitch[] = {
    { "1.0.0.0", "Initial version" },
};
}

std::string HalStitch::version() const
{
    return kVer_HalStitch;
}

// 版本变更，只追加
std::vector<VerNote> HalStitch::versionNotes() const
{
    return verNotesFrom(kNotes_HalStitch, static_cast<int>(sizeof(kNotes_HalStitch) / sizeof(kNotes_HalStitch[0])));
}

// 缺省图加其它图，转成 Halcon 图
bool HalStitch::collect(const AlgToolIn& in, std::vector<HalconCpp::HObject>* images, std::string* err) const
{
    const HsCommData::HsImage& first = inputImage(in);
    if (in.imageNative.ptr != nullptr || !first.isNull()) {
        HalconCpp::HObject one;
        HalconCpp::HObject* view = &one;
        if (!inputToHObject(in.imageNative.ptr, static_cast<int>(in.imageNative.algLib), first, view, err)) {
            return false;
        }
        images->push_back(one);
    }
    for (size_t i = 0; i < in.images.size(); ++i) {
        const PortImage& item = in.images[i];
        if (item.native.ptr == nullptr && item.image.isNull()) {
            continue;
        }
        HalconCpp::HObject one;
        HalconCpp::HObject* view = &one;
        if (!inputToHObject(item.native.ptr, static_cast<int>(item.native.algLib), item.image, view, err)) {
            return false;
        }
        images->push_back(one);
    }
    return true;
}

// 通道数不一致时把多通道图转灰度
void HalStitch::unifyChannels(std::vector<HalconCpp::HObject>* images) const
{
    std::vector<Hlong> channels;
    bool mixed = false;
    for (size_t i = 0; i < images->size(); ++i) {
        HalconCpp::HTuple ch;
        HalconCpp::CountChannels((*images)[i], &ch);
        channels.push_back(ch.Length() > 0 ? ch[0].L() : 1);
        if (channels.back() != channels.front()) {
            mixed = true;
        }
    }
    if (!mixed) {
        return;
    }
    for (size_t i = 0; i < images->size(); ++i) {
        if (channels[i] >= 3) {
            HalconCpp::HObject gray;
            HalconCpp::Rgb1ToGray((*images)[i], &gray);
            (*images)[i] = gray;
        }
    }
}

// 按列数把多张图拼成一张
int HalStitch::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = AlgToolOut();
    try {
        SectionTimer timer;
        timer.start();
        std::vector<HalconCpp::HObject> images;
        std::string err;
        if (!collect(in, &images, &err)) {
            return fail(HS_ERR_STITCH_OPERATOR, err, out);
        }
        if (images.size() < 2) {
            return fail(HS_ERR_STITCH_FEW, "need at least two images", out);
        }
        ToolParaStitch local;
        const ToolParaStitch* para = paraOf(in, &local);
        const int count = static_cast<int>(images.size());
        int columns = para->run.columns;
        if (columns < 1 || columns > count) {
            columns = count;
        }
        unifyChannels(&images);
        timer.mark("proc_input");

        // 每行取最高、每列取最宽，各图贴在格子左上角
        const int rows = (count + columns - 1) / columns;
        std::vector<int> widths;
        std::vector<int> heights;
        std::vector<int> rowHeight(static_cast<size_t>(rows), 0);
        std::vector<int> colWidth(static_cast<size_t>(columns), 0);
        HalconCpp::HObject all;
        HalconCpp::GenEmptyObj(&all);
        for (int i = 0; i < count; ++i) {
            HalconCpp::HTuple tw;
            HalconCpp::HTuple th;
            HalconCpp::GetImageSize(images[static_cast<size_t>(i)], &tw, &th);
            const int iw = static_cast<int>(tw[0].L());
            const int ih = static_cast<int>(th[0].L());
            widths.push_back(iw);
            heights.push_back(ih);
            int& rh = rowHeight[static_cast<size_t>(i / columns)];
            int& cw = colWidth[static_cast<size_t>(i % columns)];
            rh = ih > rh ? ih : rh;
            cw = iw > cw ? iw : cw;
            HalconCpp::HObject next;
            HalconCpp::ConcatObj(all, images[static_cast<size_t>(i)], &next);
            all = next;
        }
        std::vector<int> rowAt(static_cast<size_t>(rows), 0);
        std::vector<int> colAt(static_cast<size_t>(columns), 0);
        int totalH = 0;
        int totalW = 0;
        for (int r = 0; r < rows; ++r) {
            rowAt[static_cast<size_t>(r)] = totalH;
            totalH += rowHeight[static_cast<size_t>(r)];
        }
        for (int c = 0; c < columns; ++c) {
            colAt[static_cast<size_t>(c)] = totalW;
            totalW += colWidth[static_cast<size_t>(c)];
        }
        HalconCpp::HTuple offsetRow;
        HalconCpp::HTuple offsetCol;
        HalconCpp::HTuple row1;
        HalconCpp::HTuple col1;
        HalconCpp::HTuple row2;
        HalconCpp::HTuple col2;
        for (int i = 0; i < count; ++i) {
            offsetRow.Append(rowAt[static_cast<size_t>(i / columns)]);
            offsetCol.Append(colAt[static_cast<size_t>(i % columns)]);
            row1.Append(0);
            col1.Append(0);
            row2.Append(heights[static_cast<size_t>(i)] - 1);
            col2.Append(widths[static_cast<size_t>(i)] - 1);
        }
        HalconCpp::HObject tiled;
        HalconCpp::TileImagesOffset(all, &tiled, offsetRow, offsetCol, row1, col1, row2, col2, totalW, totalH);
        timer.mark("alg_run");

        HsCommData::HsImage* result = &out->image.image;
        if (!hObjectToHsImage(tiled, inputImage(in).frameId, result, &err)) {
            return fail(HS_ERR_STITCH_OPERATOR, err, out);
        }
        TolShot tol;
        tol.put_int(StitchKey::count, count);
        tol.put_int(StitchKey::columns, columns);
        tol.judge(para->judge, nullptr);
        tol.apply(out);
        out->ok = tol.ok;
        out->errCode = HS_OK;
        out->message = tol.ok ? "Stitch done" : "Stitch out of tolerance";
        timer.stop("alg_rst_proc");
        write_alg_steps(out, timer.sections(), timer.totalMs());
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        return fail(HS_ERR_STITCH_OPERATOR, hExceptionText(e), out);
    }
}

}
