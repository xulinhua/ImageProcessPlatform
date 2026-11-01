/**
 * @file PclFitCircle.cpp
 * @brief 三维圆拟合（PCL SACMODEL_CIRCLE3D）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclFitCircle.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>
#if defined(VISION_HAS_PCL)
#  include <pcl/point_cloud.h>
#  include <pcl/point_types.h>
#  include <pcl/segmentation/sac_segmentation.h>
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "三维圆 RANSAC 拟合" } };
}

std::vector<VerNote> PclFitCircle::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclFitCircle::PclFitCircle() : shot_err_(0) {}
PclFitCircle::~PclFitCircle() {}

int PclFitCircle::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclFitCircle::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaFitCircle local;
    const ToolParaFitCircle* p = paraOf(in, &local);
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

int PclFitCircle::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    if (cloud_.xyz.size() < 3) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "点太少，无法拟合圆";
        return shot_err_;
    }
    double dist = para_.run.distance;
    if (dist <= 0.0) {
        dist = 0.5;
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts = ops_.to_xyz(cloud_);
    pcl::SACSegmentation<pcl::PointXYZ> seg;
    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_CIRCLE3D);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(dist);
    seg.setInputCloud(pts);
    pcl::PointIndices idx;
    pcl::ModelCoefficients coeff;
    seg.segment(idx, coeff);
    // 圆心 xyz + 半径 + 法向（7）
    if (coeff.values.size() < 7 || idx.indices.empty()) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "圆拟合失败";
        return shot_err_;
    }
    double nx = coeff.values[4], ny = coeff.values[5], nz = coeff.values[6];
    const double nn = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nn > 1e-12) {
        nx /= nn; ny /= nn; nz /= nn;
    }
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("cx", coeff.values[0]);
    result_.values.insert("cy", coeff.values[1]);
    result_.values.insert("cz", coeff.values[2]);
    result_.values.insert("r", coeff.values[3]);
    result_.values.insert("nx", nx);
    result_.values.insert("ny", ny);
    result_.values.insert("nz", nz);
    result_.values.insert("inliers", static_cast<int>(idx.indices.size()));
    char msg[80];
    sprintf_s(msg, "圆 r=%.3f 内点 %zu", coeff.values[3], idx.indices.size());
    result_.message = msg;
    return HS_OK;
#endif
}

int PclFitCircle::judge_tol(const AlgToolIn& in)
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

int PclFitCircle::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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

int PclFitCircle::execute(const AlgToolIn& in, AlgToolOut* out)
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
