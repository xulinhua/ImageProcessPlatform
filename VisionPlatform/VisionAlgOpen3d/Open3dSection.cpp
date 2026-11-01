/**
 * @file Open3dSection.cpp
 * @brief 点云截面：相对最佳拟合平面带状切片（Open3D）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Open3dSection.h"
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
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "拟合平面带状截面" } };
}

std::vector<VerNote> Open3dSection::version_notes() const { return verNotesFrom(kVerNotes, 1); }
Open3dSection::Open3dSection() : shot_err_(0) {}
Open3dSection::~Open3dSection() {}

int Open3dSection::tool_reset()
{
    tol_.reset(); result_ = AlgToolOut(); shot_err_ = 0; shot_msg_.clear();
    cloud_ = HsCommData::Cloud3(); cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int Open3dSection::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3D"; return shot_err_;
#else
    ToolParaSection local; const ToolParaSection* p = paraOf(in, &local); para_ = *p;
    if (para_.loadCode() != HS_OK) {
        shot_err_ = para_.loadCode();
        shot_msg_ = HsCommData::HsException(shot_err_).errorMessage();
        return shot_err_;
    }
    if (!in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) cloud_ = in.clouds[0].cloud;
    return HS_OK;
#endif
}

int Open3dSection::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3D"; return shot_err_;
#else
    if (cloud_.xyz.size() < 3) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少，无法取截面"; return shot_err_; }
    double band = para_.run.band; if (band < 0.0) band = 0.0;
    open3d::geometry::PointCloud pc = ops_.to_cloud(cloud_);
    auto plane = pc.SegmentPlane(band > 0.0 ? band : 0.5, 3, 1000);
    Eigen::Vector4d model = std::get<0>(plane);
    double a = model(0), b = model(1), c = model(2), d = model(3);
    const double nn = std::sqrt(a * a + b * b + c * c);
    if (nn < 1e-12) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "截面平面退化"; return shot_err_; }
    a /= nn; b /= nn; c /= nn; d /= nn;
    const double half = band * 0.5;
    std::vector<size_t> keep;
    keep.reserve(pc.points_.size());
    for (size_t i = 0; i < pc.points_.size(); ++i) {
        const Eigen::Vector3d& p = pc.points_[i];
        const double dist = std::fabs(a * p.x() + b * p.y() + c * p.z() + d);
        if (dist <= half) keep.push_back(i);
    }
    auto sectionPc = pc.SelectByIndex(keep);
    HsCommData::Cloud3 section = sectionPc ? ops_.from_cloud(*sectionPc) : HsCommData::Cloud3();
    PortCloud out; out.cloud = section; result_.clouds.push_back(out);
    result_.ok = true; result_.errCode = HS_OK;
    result_.values.insert("points", static_cast<int>(section.xyz.size()));
    result_.values.insert("nx", a); result_.values.insert("ny", b); result_.values.insert("nz", c);
    char msg[64]; sprintf_s(msg, "截面 %zu 点", section.xyz.size()); result_.message = msg;
    return HS_OK;
#endif
}

int Open3dSection::judge_tol(const AlgToolIn& in)
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

int Open3dSection::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    result_.errCode = shot_err_;
    if (shot_err_ != HS_OK) { result_.ok = false; result_.runOk = false; if (!shot_msg_.empty()) result_.message = shot_msg_; }
    else { result_.runOk = true; result_.ok = tol_.ok; tol_.apply(&result_); }
    *out = result_; return shot_err_;
}

int Open3dSection::execute(const AlgToolIn& in, AlgToolOut* out)
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
