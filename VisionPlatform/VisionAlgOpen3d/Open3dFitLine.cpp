/**
 * @file Open3dFitLine.cpp
 * @brief 三维直线拟合（Open3D：协方差主方向）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Open3dFitLine.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>
#if defined(VISION_HAS_OPEN3D)
#  include <open3d/geometry/PointCloud.h>
#  include <Eigen/Eigenvalues>
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "三维直线主成分拟合" } };
}

std::vector<VerNote> Open3dFitLine::version_notes() const { return verNotesFrom(kVerNotes, 1); }
Open3dFitLine::Open3dFitLine() : shot_err_(0) {}
Open3dFitLine::~Open3dFitLine() {}

int Open3dFitLine::tool_reset()
{
    tol_.reset(); result_ = AlgToolOut(); shot_err_ = 0; shot_msg_.clear();
    cloud_ = HsCommData::Cloud3(); cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int Open3dFitLine::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3D"; return shot_err_;
#else
    ToolParaFitLine local; const ToolParaFitLine* p = paraOf(in, &local); para_ = *p;
    if (para_.loadCode() != HS_OK) {
        shot_err_ = para_.loadCode();
        shot_msg_ = HsCommData::HsException(shot_err_).errorMessage();
        return shot_err_;
    }
    if (!in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) cloud_ = in.clouds[0].cloud;
    return HS_OK;
#endif
}

int Open3dFitLine::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3D"; return shot_err_;
#else
    if (cloud_.xyz.size() < 2) {
        shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少，无法拟合直线"; return shot_err_;
    }
    Eigen::Vector3d mean(0, 0, 0);
    for (size_t i = 0; i < cloud_.xyz.size(); ++i) {
        mean += Eigen::Vector3d(cloud_.xyz[i].x, cloud_.xyz[i].y, cloud_.xyz[i].z);
    }
    mean /= static_cast<double>(cloud_.xyz.size());
    Eigen::Matrix3d cov = Eigen::Matrix3d::Zero();
    int inliers = 0;
    for (size_t i = 0; i < cloud_.xyz.size(); ++i) {
        Eigen::Vector3d d(cloud_.xyz[i].x - mean.x(), cloud_.xyz[i].y - mean.y(), cloud_.xyz[i].z - mean.z());
        cov += d * d.transpose();
    }
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(cov);
    if (es.info() != Eigen::Success) {
        shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "直线特征分解失败"; return shot_err_;
    }
    // 最大特征值对应主方向
    Eigen::Vector3d dir = es.eigenvectors().col(2);
    const double nn = dir.norm();
    if (nn < 1e-12) {
        shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "直线方向退化"; return shot_err_;
    }
    dir /= nn;
    // distance 参数：统计距直线小于阈值的点数
    const double distTol = para_.run.distance > 0.0 ? para_.run.distance : 0.5;
    for (size_t i = 0; i < cloud_.xyz.size(); ++i) {
        Eigen::Vector3d p(cloud_.xyz[i].x, cloud_.xyz[i].y, cloud_.xyz[i].z);
        const double dist = (p - mean).cross(dir).norm();
        if (dist <= distTol) {
            ++inliers;
        }
    }
    result_.ok = true; result_.errCode = HS_OK;
    result_.values.insert("ox", mean.x());
    result_.values.insert("oy", mean.y());
    result_.values.insert("oz", mean.z());
    result_.values.insert("dx", dir.x());
    result_.values.insert("dy", dir.y());
    result_.values.insert("dz", dir.z());
    result_.values.insert("inliers", inliers);
    char msg[64]; sprintf_s(msg, "直线内点 %d", inliers); result_.message = msg;
    return HS_OK;
#endif
}

int Open3dFitLine::judge_tol(const AlgToolIn& in)
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

int Open3dFitLine::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    result_.errCode = shot_err_;
    if (shot_err_ != HS_OK) { result_.ok = false; result_.runOk = false; if (!shot_msg_.empty()) result_.message = shot_msg_; }
    else { result_.runOk = true; result_.ok = tol_.ok; tol_.apply(&result_); }
    *out = result_; return shot_err_;
}

int Open3dFitLine::execute(const AlgToolIn& in, AlgToolOut* out)
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
