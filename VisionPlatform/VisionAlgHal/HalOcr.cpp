/**
 * @file HalOcr.cpp
 * @brief 字符识别（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalOcr.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "HalTupleConvert.h"
#include "ParamKeys.h"
#include "TolShot.h"
#include "ToolParaOcr.hpp"
#include <HsCommData/HsError.h>

namespace Vision {

namespace {
const char kVer_HalOcr[] = "1.0.0.0";
const VerNoteRaw kNotes_HalOcr[] = {
    { "1.0.0.0", "Initial version" },
};
}

std::string HalOcr::version() const
{
    return kVer_HalOcr;
}

// 版本变更，只追加
std::vector<VerNote> HalOcr::versionNotes() const
{
    return verNotesFrom(kNotes_HalOcr, static_cast<int>(sizeof(kNotes_HalOcr) / sizeof(kNotes_HalOcr[0])));
}

// 释放读字模型
HalOcr::Model::~Model()
{
    if (!open) {
        return;
    }
    try {
        HalconCpp::ClearTextModel(id);
    } catch (const HalconCpp::HException&) {
    }
}

// 释放读字结果
HalOcr::Result::~Result()
{
    if (!open) {
        return;
    }
    try {
        HalconCpp::ClearTextResult(id);
    } catch (const HalconCpp::HException&) {
    }
}

// 读字符，写出文本、字符区和位姿
int HalOcr::execute(const AlgToolIn& in, AlgToolOut* out)
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
        ToolParaOcr local;
        const ToolParaOcr* para = paraOf(in, &local);
        HalconCpp::HObject reduced;
        HalconCpp::ReduceDomain(gray, toolRoisToHObject(plainRois(in.rois), w, h), &reduced);
        timer.mark("proc_input");

        // 每拍按字体新建读字模型，设极性和字高后在搜索区里读字
        Model model;
        try {
            HalconCpp::CreateTextModelReader("auto", para->run.font.c_str(), &model.id);
        } catch (const HalconCpp::HException& e) {
            return fail(HS_ERR_OCR_NO_MODEL, para->run.font + ": " + hExceptionText(e), out);
        }
        model.open = true;
        HalconCpp::SetTextModelParam(model.id, "polarity", para->run.polarity.c_str());
        if (para->run.minCharHeight > 0) {
            HalconCpp::SetTextModelParam(model.id, "min_char_height", para->run.minCharHeight);
        }
        if (para->run.maxCharHeight > 0) {
            HalconCpp::SetTextModelParam(model.id, "max_char_height", para->run.maxCharHeight);
        }
        Result result;
        HalconCpp::FindText(reduced, model.id, &result.id);
        result.open = true;
        HalconCpp::HTuple classes;
        HalconCpp::GetTextResult(result.id, "class", &classes);
        timer.mark("alg_run");
        const int found = static_cast<int>(classes.Length());
        if (found < 1) {
            return fail(HS_ERR_OCR_NONE, "no characters found", out);
        }

        // 文本按字符顺序拼接，分数取各字符置信度均值
        std::string text;
        for (int i = 0; i < found; ++i) {
            text += hTupleText(classes, i);
        }
        HalconCpp::HTuple confidence;
        try {
            HalconCpp::GetTextResult(result.id, "confidence", &confidence);
        } catch (const HalconCpp::HException&) {
            confidence = HalconCpp::HTuple();
        }
        double score = 0;
        for (Hlong i = 0; i < confidence.Length(); ++i) {
            score += confidence[i].D();
        }
        score = confidence.Length() > 0 ? score / static_cast<double>(confidence.Length()) : 0.0;

        // 位姿取全部字符并集的最小外接旋转矩形；无字符区时位姿为零
        HalconCpp::HObject chars;
        HalconCpp::HObject region;
        HalconCpp::GetTextObject(&chars, result.id, "all_lines");
        HalconCpp::Union1(chars, &region);
        HsCommData::Pose2 pose;
        if (regionHasArea(region)) {
            HalconCpp::HTuple row;
            HalconCpp::HTuple col;
            HalconCpp::HTuple phi;
            HalconCpp::HTuple len1;
            HalconCpp::HTuple len2;
            HalconCpp::SmallestRectangle2(region, &row, &col, &phi, &len1, &len2);
            pose.tx = col[0].D();
            pose.ty = row[0].D();
            pose.setAngRad(phi[0].D());
            NamedObject item;
            item.port = OcrKey::resultRoi;
            item.object = hObjectToRegionObject(region);
            out->portObjects.push_back(item);
            out->objects.push_back(item.object);
        }
        out->poses.push_back(pose);
        ResultLabel label;
        label.x = pose.tx;
        label.y = pose.ty;
        label.text = text;
        out->labels.push_back(label);

        // 公差判定后再比对期望文本（期望为空不比对），两者都过才 OK
        TolShot tol;
        tol.put_text(OcrKey::text, text);
        tol.put_int(OcrKey::count, found);
        paramSet(&tol.values, ValueKey::X, pose.tx);
        paramSet(&tol.values, ValueKey::Y, pose.ty);
        paramSet(&tol.values, ValueKey::Angle, pose.angRad());
        paramSet(&tol.values, ValueKey::Score, score);
        tol.judge(para->judge, nullptr);
        tol.apply(out);
        const bool match = para->run.expected.empty() || para->run.expected == text;
        out->ok = tol.ok && match;
        out->errCode = HS_OK;
        if (!match) {
            out->message = "text mismatch: " + text;
        } else {
            out->message = tol.ok ? "OCR done" : "OCR out of tolerance";
        }
        timer.stop("alg_rst_proc");
        write_alg_steps(out, timer.sections(), timer.totalMs());
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        return fail(HS_ERR_OCR_OPERATOR, hExceptionText(e), out);
    }
}

}
