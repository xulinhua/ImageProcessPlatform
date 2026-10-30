/**
 * @file HalQr.cpp
 * @brief 二维码识别（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalQr.h"
#include "HalImageConvert.h"
#include "HalObjectConvert.h"
#include "HalOps.h"
#include "HalTupleConvert.h"
#include "ParamKeys.h"
#include "TolShot.h"
#include "ToolParaQr.hpp"
#include <HsCommData/HsError.h>

namespace Vision {

namespace {
const char kVer_HalQr[] = "1.0.0.0";
const VerNoteRaw kNotes_HalQr[] = {
    { "1.0.0.0", "Initial version" },
};
}

std::string HalQr::version() const
{
    return kVer_HalQr;
}

// 版本变更，只追加
std::vector<VerNote> HalQr::versionNotes() const
{
    return verNotesFrom(kNotes_HalQr, static_cast<int>(sizeof(kNotes_HalQr) / sizeof(kNotes_HalQr[0])));
}

// 释放模型句柄
HalQr::Model::~Model()
{
    if (!open) {
        return;
    }
    try {
        HalconCpp::ClearDataCode2dModel(id);
    } catch (const HalconCpp::HException&) {
    }
}

// 在搜索区里读码，写出文本、码区和位姿
int HalQr::execute(const AlgToolIn& in, AlgToolOut* out)
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
        ToolParaQr local;
        const ToolParaQr* para = paraOf(in, &local);
        HalconCpp::HObject reduced;
        HalconCpp::ReduceDomain(gray, toolRoisToHObject(plainRois(in.rois), w, h), &reduced);
        timer.mark("proc_input");

        Model model;
        HalconCpp::CreateDataCode2dModel("QR Code", HalconCpp::HTuple(), HalconCpp::HTuple(), &model.id);
        model.open = model.id.Length() > 0;
        HalconCpp::SetDataCode2dParam(model.id, "default_parameters", "enhanced_recognition");
        HalconCpp::SetDataCode2dParam(model.id, "polarity", para->run.polarity.c_str());
        HalconCpp::SetDataCode2dParam(model.id, "timeout", para->run.timeout);
        if (para->run.moduleMin > 0) {
            HalconCpp::SetDataCode2dParam(model.id, "module_size_min", para->run.moduleMin);
        }
        HalconCpp::HObject symbols;
        HalconCpp::HTuple handles;
        HalconCpp::HTuple decoded;
        HalconCpp::FindDataCode2d(reduced, &symbols, model.id, "stop_after_result_num", para->run.num, &handles, &decoded);
        timer.mark("alg_run");
        const int found = static_cast<int>(decoded.Length());
        if (found < 1) {
            return fail(HS_ERR_QR_NONE, "no QR code found", out);
        }

        // 位姿优先用码的方向，没有再用外接矩形的角
        HalconCpp::HTuple orient;
        try {
            HalconCpp::GetDataCode2dResults(model.id, handles, "orientation", &orient);
        } catch (const HalconCpp::HException&) {
            orient = HalconCpp::HTuple();
        }
        HalconCpp::HTuple row;
        HalconCpp::HTuple col;
        HalconCpp::HTuple phi;
        HalconCpp::HTuple len1;
        HalconCpp::HTuple len2;
        HalconCpp::SmallestRectangle2Xld(symbols, &row, &col, &phi, &len1, &len2);
        HalconCpp::HObject filled;
        HalconCpp::HObject region;
        HalconCpp::GenRegionContourXld(symbols, &filled, "filled");
        HalconCpp::Union1(filled, &region);

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
        tol.put_text(QrKey::code, joined);
        tol.put_int(QrKey::count, found);
        paramSet(&tol.values, ValueKey::X, out->poses[0].tx);
        paramSet(&tol.values, ValueKey::Y, out->poses[0].ty);
        paramSet(&tol.values, ValueKey::Angle, out->poses[0].angRad());
        tol.judge(para->judge, nullptr);
        tol.apply(out);
        if (regionHasArea(region)) {
            NamedObject symbol;
            symbol.port = QrKey::resultRoi;
            symbol.object = hObjectToRegionObject(region);
            out->portObjects.push_back(symbol);
            out->objects.push_back(symbol.object);
        }
        out->ok = tol.ok;
        out->errCode = HS_OK;
        out->message = tol.ok ? "QR done" : "QR out of tolerance";
        timer.stop("alg_rst_proc");
        write_alg_steps(out, timer.sections(), timer.totalMs());
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        return fail(HS_ERR_QR_OPERATOR, hExceptionText(e), out);
    }
}

}
