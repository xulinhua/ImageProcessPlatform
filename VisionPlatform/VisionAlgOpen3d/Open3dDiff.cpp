/**
 * @file Open3dDiff.cpp
 * @brief 点云差分：相对参考云超距的点（Open3D）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Open3dDiff.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>
#if defined(VISION_HAS_OPEN3D)
#  include <open3d/geometry/PointCloud.h>
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云差分（超距点）" } };
}

std::vector<VerNote> Open3dDiff::version_notes() const { return verNotesFrom(kVerNotes, 1); }
Open3dDiff::Open3dDiff() : shot_err_(0) {}
Open3dDiff::~Open3dDiff() {}

int Open3dDiff::tool_reset()
{
    tol_.reset(); result_ = AlgToolOut(); shot_err_ = 0; shot_msg_.clear();
    cloud_ = HsCommData::Cloud3(); cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int Open3dDiff::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3D"; return shot_err_;
#else
    ToolParaDiff local; const ToolParaDiff* p = paraOf(in, &local); para_ = *p;
    if (para_.loadCode() != HS_OK) {
        shot_err_ = para_.loadCode();
        shot_msg_ = HsCommData::HsException(shot_err_).errorMessage();
        return shot_err_;
    }
    if (!in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) cloud_ = in.clouds[0].cloud;
    if (in.clouds.size() >= 2 && !in.clouds[1].cloud.isEmpty()) cloud2_ = in.clouds[1].cloud;
    for (size_t i = 0; i < in.clouds.size(); ++i) {
        if (in.clouds[i].port == "template" || in.clouds[i].port == "target" || in.clouds[i].port == "ref") {
            cloud2_ = in.clouds[i].cloud;
        }
    }
    return HS_OK;
#endif
}

int Open3dDiff::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3D"; return shot_err_;
#else
    if (cloud_.isEmpty() || cloud2_.isEmpty()) {
        shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "需要场景点云与参考点云"; return shot_err_;
    }
    double thr = para_.run.threshold; if (thr < 0.0) thr = 0.0;
    open3d::geometry::PointCloud scene = ops_.to_cloud(cloud_);
    open3d::geometry::PointCloud ref = ops_.to_cloud(cloud2_);
    // 用点到点云距离，避免拉入 KDTree/Feature 头
    const std::vector<double> dists = scene.ComputePointCloudDistance(ref);
    std::vector<size_t> keep;
    keep.reserve(dists.size());
    for (size_t i = 0; i < dists.size(); ++i) {
        if (dists[i] > thr) {
            keep.push_back(i);
        }
    }
    auto diffPc = scene.SelectByIndex(keep);
    HsCommData::Cloud3 diff = diffPc ? ops_.from_cloud(*diffPc) : HsCommData::Cloud3();
    PortCloud out; out.cloud = diff; result_.clouds.push_back(out);
    result_.ok = true; result_.errCode = HS_OK;
    result_.values.insert("points", static_cast<int>(diff.xyz.size()));
    char msg[64]; sprintf_s(msg, "差分 %zu 点", diff.xyz.size()); result_.message = msg;
    return HS_OK;
#endif
}

int Open3dDiff::judge_tol(const AlgToolIn& in)
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

int Open3dDiff::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    result_.errCode = shot_err_;
    if (shot_err_ != HS_OK) { result_.ok = false; result_.runOk = false; if (!shot_msg_.empty()) result_.message = shot_msg_; }
    else { result_.runOk = true; result_.ok = tol_.ok; tol_.apply(&result_); }
    *out = result_; return shot_err_;
}

int Open3dDiff::execute(const AlgToolIn& in, AlgToolOut* out)
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
