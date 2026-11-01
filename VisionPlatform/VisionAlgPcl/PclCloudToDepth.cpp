/**
 * @file PclCloudToDepth.cpp
 * @brief 点云投影成深度图
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclCloudToDepth.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云按内参投影成深度图" } };
}

std::vector<VerNote> PclCloudToDepth::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclCloudToDepth::PclCloudToDepth() : shot_err_(0) {}
PclCloudToDepth::~PclCloudToDepth() {}

int PclCloudToDepth::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclCloudToDepth::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaCloudToDepth local;
    const ToolParaCloudToDepth* p = paraOf(in, &local);
    para_ = *p;
    if (para_.loadCode() != HS_OK) {
        shot_err_ = para_.loadCode();
        shot_msg_ = HsCommData::HsException(shot_err_).errorMessage();
        return shot_err_;
    }
    if (!in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) {
        cloud_ = in.clouds[0].cloud;
    }
    return HS_OK;
#endif
}

int PclCloudToDepth::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    if (cloud_.isEmpty()) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "没有输入点云";
        return shot_err_;
    }
    int w = para_.run.width;
    int h = para_.run.height;
    if (w <= 0) {
        w = 640;
    }
    if (h <= 0) {
        h = 480;
    }
    HsCommData::HsImage depth = ops_.cloud_to_depth(cloud_, para_.run.fx, para_.run.fy, para_.run.cx, para_.run.cy, w, h);
    if (depth.isNull()) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "投影深度图失败";
        return shot_err_;
    }
    int valid = 0;
    for (int y = 0; y < depth.height; ++y) {
        const float* row = reinterpret_cast<const float*>(depth.scanLine(y));
        for (int x = 0; x < depth.width; ++x) {
            if (row[x] > 0.0f) {
                ++valid;
            }
        }
    }
    PortImage pi;
    pi.image = depth;
    result_.image = pi;
    result_.images.push_back(pi);
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("points", static_cast<int>(cloud_.xyz.size()));
    result_.values.insert("valid", valid);
    result_.values.insert("width", w);
    result_.values.insert("height", h);
    char msg[64];
    sprintf_s(msg, "深度有效像素 %d", valid);
    result_.message = msg;
    return HS_OK;
#endif
}

int PclCloudToDepth::judge_tol(const AlgToolIn& in)
{
    tol_.reset();
    const std::vector<std::string> keys = result_.values.keys();
    for (size_t i = 0; i < keys.size(); ++i) {
        const ParamValue v = result_.values.value(keys[i]);
        if (v.kind() == ParamValue::KindInt) {
            tol_.put_int(keys[i], v.toInt(0));
        } else if (v.kind() == ParamValue::KindDouble) {
            tol_.put(keys[i], v.toDouble(0));
        }
    }
    tol_.judge(para_.judge.empty() ? in.judge : para_.judge, nullptr);
    return HS_OK;
}

int PclCloudToDepth::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    result_.errCode = shot_err_;
    if (shot_err_ != HS_OK) {
        result_.ok = false;
        result_.runOk = false;
        if (!shot_msg_.empty()) {
            result_.message = shot_msg_;
        }
    } else {
        result_.runOk = true;
        result_.ok = tol_.ok;
        tol_.apply(&result_);
    }
    *out = result_;
    return shot_err_;
}

int PclCloudToDepth::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
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
}

}
