/**
 * @file HalUndistort.cpp
 * @brief 按 Halcon 相机参数做径向畸变校正
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalUndistort.h"
#include "HalImageConvert.h"
#include "HalOps.h"
#include "ToolParaUndistort.hpp"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <cstdlib>
#include <sstream>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "preprocess.undistort";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "首版" },
};
}

HalUndistort::HalUndistort()
    : frame_id_(0)
    , shot_err_(0)
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&fixed_);
}

AlgToolBase* HalUndistort::clone() const
{
    return new HalUndistort();
}

std::string HalUndistort::tool_name() const
{
    return kName;
}

AlgLibKind HalUndistort::alg_lib() const
{
    return AlgLibHal;
}

std::string HalUndistort::version() const
{
    return kVer;
}

std::vector<VerNote> HalUndistort::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

void HalUndistort::init(const std::string&)
{
}

void HalUndistort::save(const std::string&)
{
}

int HalUndistort::tool_reset()
{
    HalconCpp::GenEmptyObj(&image_);
    HalconCpp::GenEmptyObj(&fixed_);
    frame_id_ = 0;
    cam_text_.clear();
    tol_.reset();
    shot_err_ = 0;
    shot_msg_.clear();
    return HS_OK;
}

HalconCpp::HTuple HalUndistort::parse_cam_par(const std::string& text) const
{
    HalconCpp::HTuple t;
    std::istringstream in(text);
    std::string tok;
    bool first = true;
    while (in >> tok) {
        if (first) {
            t.Append(tok.c_str());
            first = false;
            continue;
        }
        t.Append(atof(tok.c_str()));
    }
    return t;
}

int HalUndistort::proc_input(const AlgToolIn& in)
{
    if (inputImage(in).isNull()) {
        shot_err_ = HS_ERR_IMAGE_EMPTY;
        shot_msg_ = HsCommData::HsException(HS_ERR_IMAGE_EMPTY).errorMessage();
        return shot_err_;
    }
    ToolParaUndistort local;
    const ToolParaUndistort* para = paraOf(in, &local);
    if (in.param != nullptr && toolParaAs<ToolParaUndistort>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    cam_text_ = para->run.halconCam;
    if (cam_text_.empty()) {
        shot_err_ = HS_ERR_PARAM;
        shot_msg_ = "缺少 Halcon 相机参数";
        return shot_err_;
    }
    std::string err;
    HalconCpp::HObject gray;
    HalconCpp::HObject* pGray = &gray;
    int w = 0;
    int h = 0;
    if (!inputToGray(in.imageNative.ptr, static_cast<int>(in.imageNative.algLib), inputImage(in), pGray, &w, &h, &err)
        || w < 1 || h < 1) {
        shot_err_ = HS_ERR_IMAGE_EMPTY;
        shot_msg_ = err.empty() ? HsCommData::HsException(HS_ERR_IMAGE_EMPTY).errorMessage() : err;
        return shot_err_;
    }
    image_ = gray;
    frame_id_ = inputImage(in).frameId;
    return HS_OK;
}

int HalUndistort::alg_run(const AlgToolIn& in)
{
    (void)in;
    try {
        HalconCpp::HTuple camIn = parse_cam_par(cam_text_);
        HalconCpp::HTuple camOut;
        HalconCpp::ChangeRadialDistortionCamPar(camIn, "adaptive", 0.0, &camOut);
        HalconCpp::HObject domain;
        HalconCpp::GetDomain(image_, &domain);
        HalconCpp::ChangeRadialDistortionImage(image_, domain, &fixed_, camIn, camOut);
        shot_err_ = HS_OK;
        shot_msg_ = "已校正";
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = hExceptionText(e);
        return shot_err_;
    }
}

int HalUndistort::judge_tol(const AlgToolIn& in)
{
    // 图像预处理工具无专用公差域；仍按 judge 表比较已写入测值
    ToolParaUndistort local;
    const ToolParaUndistort* para = paraOf(in, &local);
    tol_.reset();
    tol_.judge(para->judge, nullptr);
    return HS_OK;
}

int HalUndistort::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
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
        if (!hObjectToHsImage(fixed_, frame_id_, image, &err)) {
            result.ok = false;
            result.runOk = false;
            result.errCode = HS_ERR_OPERATOR;
            result.message = err;
            *out = result;
            return HS_ERR_OPERATOR;
        }
        tol_.apply(&result);
        result.ok = shot_err_ == HS_OK && tol_.ok;
    }
    *out = result;
    return shot_err_;
}

int HalUndistort::execute(const AlgToolIn& in, AlgToolOut* out)
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
