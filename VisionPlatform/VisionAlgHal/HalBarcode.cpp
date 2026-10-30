/**
 * @file HalBarcode.cpp
 * @brief 一维码识别（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalBarcode.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "HalTupleConvert.h"
#include "ParamKeys.h"
#include "TolShot.h"
#include "ToolParaBarcode.hpp"
#include <HsCommData/HsError.h>

namespace Vision {

namespace {
const char kVer_HalBarcode[] = "1.0.0.0";
const VerNoteRaw kNotes_HalBarcode[] = {
    { "1.0.0.0", "Initial version" },
};
}

std::string HalBarcode::version() const
{
    return kVer_HalBarcode;
}

// 版本变更，只追加
std::vector<VerNote> HalBarcode::versionNotes() const
{
    return verNotesFrom(kNotes_HalBarcode, static_cast<int>(sizeof(kNotes_HalBarcode) / sizeof(kNotes_HalBarcode[0])));
}

// 释放模型句柄
HalBarcode::Model::~Model()
{
    if (!open) {
        return;
    }
    try {
        HalconCpp::ClearBarCodeModel(id);
    } catch (const HalconCpp::HException&) {
    }
}

// 在搜索区里读码，写出文本、码区和位姿
int HalBarcode::execute(const AlgToolIn& in, AlgToolOut* out)
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
        ToolParaBarcode local;
        const ToolParaBarcode* para = paraOf(in, &local);
        HalconCpp::HObject reduced;
        HalconCpp::ReduceDomain(gray, toolRoisToHObject(plainRois(in.rois), w, h), &reduced);
        timer.mark("proc_input");

        Model model;
        HalconCpp::CreateBarCodeModel(HalconCpp::HTuple(), HalconCpp::HTuple(), &model.id);
        model.open = model.id.Length() > 0;
        HalconCpp::SetBarCodeParam(model.id, "polarity", para->run.polarity.c_str());
        HalconCpp::SetBarCodeParam(model.id, "timeout", para->run.timeout);
        HalconCpp::SetBarCodeParam(model.id, "stop_after_result_num", para->run.num);
        HalconCpp::HObject symbols;
        HalconCpp::HTuple decoded;
        HalconCpp::FindBarCode(reduced, &symbols, model.id, para->run.codeType.c_str(), &decoded);
        timer.mark("alg_run");
        const int found = static_cast<int>(decoded.Length());
        if (found < 1) {
            return fail(HS_ERR_BAR_NONE, "no barcode found", out);
        }

        // 位姿优先用码的方向，没有再用外接矩形的角
        HalconCpp::HTuple orient;
        try {
            HalconCpp::GetBarCodeResult(model.id, "all", "orientation", &orient);
        } catch (const HalconCpp::HException&) {
            orient = HalconCpp::HTuple();
        }
        HalconCpp::HTuple row;
        HalconCpp::HTuple col;
        HalconCpp::HTuple phi;
        HalconCpp::HTuple len1;
        HalconCpp::HTuple len2;
        HalconCpp::SmallestRectangle2(symbols, &row, &col, &phi, &len1, &len2);
        HalconCpp::HObject region;
        HalconCpp::Union1(symbols, &region);

        std::string joined;
        for (int i = 0; i < found; ++i) {
            const std::string text = hTupleText(decoded, i);
            HsCommData::Pose2 pose;
            if (col.Length() > i && row.Length() > i) {
                pose.tx = col[i].D();
                pose.ty = row[i].D();
            }
            if (orient.Length() > i) {
                pose.setAngRad(orient[i].D());
            } else if (phi.Length() > i) {
                pose.setAngRad(phi[i].D());
            }
            out->poses.push_back(pose);
            ResultLabel label;
            label.x = pose.tx;
            label.y = pose.ty;
            label.text = text;
            out->labels.push_back(label);
            if (!joined.empty()) {
                joined += "; ";
            }
            joined += text;
        }

        TolShot tol;
        tol.put_text(BarcodeKey::code, joined);
        tol.put_int(BarcodeKey::count, found);
        paramSet(&tol.values, ValueKey::X, out->poses[0].tx);
        paramSet(&tol.values, ValueKey::Y, out->poses[0].ty);
        paramSet(&tol.values, ValueKey::Angle, out->poses[0].angRad());
        tol.judge(para->judge, nullptr);
        tol.apply(out);
        if (regionHasArea(region)) {
            NamedObject symbol;
            symbol.port = BarcodeKey::resultRoi;
            symbol.object = hObjectToRegionObject(region);
            out->portObjects.push_back(symbol);
            out->objects.push_back(symbol.object);
        }
        out->ok = tol.ok;
        out->errCode = HS_OK;
        out->message = tol.ok ? "Barcode done" : "Barcode out of tolerance";
        timer.stop("alg_rst_proc");
        write_alg_steps(out, timer.sections(), timer.totalMs());
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        return fail(HS_ERR_BAR_OPERATOR, hExceptionText(e), out);
    }
}

}
