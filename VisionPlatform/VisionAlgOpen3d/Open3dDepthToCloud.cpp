/**
 * @file Open3dDepthToCloud.cpp
 * @brief 深度图转点云（Open3D / 针孔模型）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Open3dDepthToCloud.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cstdio>

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "深度图按内参转点云" } };
}

std::vector<VerNote> Open3dDepthToCloud::version_notes() const { return verNotesFrom(kVerNotes, 1); }
Open3dDepthToCloud::Open3dDepthToCloud() : has_image_(false), shot_err_(0) {}
Open3dDepthToCloud::~Open3dDepthToCloud() {}

int Open3dDepthToCloud::tool_reset()
{
    tol_.reset(); result_ = AlgToolOut(); shot_err_ = 0; shot_msg_.clear();
    cloud_ = HsCommData::Cloud3(); image_ = HsCommData::HsImage(); has_image_ = false;
    return HS_OK;
}

int Open3dDepthToCloud::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3D"; return shot_err_;
#else
    ToolParaDepthToCloud local; const ToolParaDepthToCloud* p = paraOf(in, &local); para_ = *p;
    if (para_.loadCode() != HS_OK) {
        shot_err_ = para_.loadCode();
        shot_msg_ = HsCommData::HsException(shot_err_).errorMessage();
        return shot_err_;
    }
    const HsCommData::HsImage& img = inputImage(in);
    if (!img.isNull()) { image_ = img; has_image_ = true; }
    if (!in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) cloud_ = in.clouds[0].cloud;
    return HS_OK;
#endif
}

int Open3dDepthToCloud::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3D"; return shot_err_;
#else
    HsCommData::Cloud3 outCloud;
    if (has_image_ && !image_.isNull()) {
        outCloud = ops_.depth_to_cloud(image_, para_.run.fx, para_.run.fy, para_.run.cx, para_.run.cy, para_.run.scale);
    } else if (cloud_.isOrganized()) {
        outCloud = ops_.organized_depth_to_cloud(cloud_, para_.run.fx, para_.run.fy, para_.run.cx, para_.run.cy, para_.run.scale);
    } else {
        shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "需要深度图或有序深度点云"; return shot_err_;
    }
    if (outCloud.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "深度转点云结果为空"; return shot_err_; }
    PortCloud pc; pc.cloud = outCloud; result_.clouds.push_back(pc);
    result_.ok = true; result_.errCode = HS_OK;
    result_.values.insert("points", static_cast<int>(outCloud.xyz.size()));
    char msg[64]; sprintf_s(msg, "转点云 %zu", outCloud.xyz.size()); result_.message = msg;
    return HS_OK;
#endif
}

int Open3dDepthToCloud::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    const std::vector<std::string> keys = result_.values.keys();
    for (size_t i = 0; i < keys.size(); ++i) {
        const ParamValue v = result_.values.value(keys[i]);
        if (v.kind() == ParamValue::KindInt) tol_.put_int(keys[i], v.toInt(0));
        else if (v.kind() == ParamValue::KindDouble) tol_.put(keys[i], v.toDouble(0));
    }
    tol_.judge(para_.judge.empty() ? in.judge : para_.judge, nullptr);
    return HS_OK;
}

int Open3dDepthToCloud::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    result_.errCode = shot_err_;
    if (shot_err_ != HS_OK) { result_.ok = false; result_.runOk = false; if (!shot_msg_.empty()) result_.message = shot_msg_; }
    else { result_.runOk = true; result_.ok = tol_.ok; tol_.apply(&result_); }
    *out = result_; return shot_err_;
}

int Open3dDepthToCloud::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) return HS_ERR_NULL;
    run_timer_.start(); tool_reset(); run_timer_.mark("tool_reset");
    int step = proc_input(in); run_timer_.mark("proc_input");
    if (step == HS_OK) { step = alg_run(in); run_timer_.mark("alg_run"); }
    if (step == HS_OK) { step = judge_tol(in); run_timer_.mark("judge_tol"); }
    alg_rst_proc(in, out); run_timer_.stop("alg_rst_proc");
    write_alg_steps(out, run_timer_.sections(), run_timer_.totalMs());
    return step != HS_OK ? step : shot_err_;
}

}
