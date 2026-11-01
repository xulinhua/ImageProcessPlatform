/**
 * @file PclFitLine.cpp
 * @brief 三维直线拟合（PCL SACMODEL_LINE）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclFitLine.h"
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
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "三维直线 RANSAC 拟合" } };
}

std::vector<VerNote> PclFitLine::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclFitLine::PclFitLine() : shot_err_(0) {}
PclFitLine::~PclFitLine() {}

int PclFitLine::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclFitLine::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaFitLine local;
    const ToolParaFitLine* p = paraOf(in, &local);
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

int PclFitLine::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    if (cloud_.xyz.size() < 2) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "点太少，无法拟合直线";
        return shot_err_;
    }
    double dist = para_.run.distance;
    if (dist <= 0.0) {
        dist = 0.5;
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts = ops_.to_xyz(cloud_);
    pcl::SACSegmentation<pcl::PointXYZ> seg;
    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_LINE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(dist);
    seg.setInputCloud(pts);
    pcl::PointIndices idx;
    pcl::ModelCoefficients coeff;
    seg.segment(idx, coeff);
    // 线模型：point_on_line + direction（6 个系数）
    if (coeff.values.size() < 6 || idx.indices.empty()) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "直线拟合失败";
        return shot_err_;
    }
    double dx = coeff.values[3], dy = coeff.values[4], dz = coeff.values[5];
    const double nn = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (nn < 1e-12) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "直线方向退化";
        return shot_err_;
    }
    dx /= nn; dy /= nn; dz /= nn;
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("ox", coeff.values[0]);
    result_.values.insert("oy", coeff.values[1]);
    result_.values.insert("oz", coeff.values[2]);
    result_.values.insert("dx", dx);
    result_.values.insert("dy", dy);
    result_.values.insert("dz", dz);
    result_.values.insert("inliers", static_cast<int>(idx.indices.size()));
    char msg[80];
    sprintf_s(msg, "直线内点 %zu", idx.indices.size());
    result_.message = msg;
    return HS_OK;
#endif
}

int PclFitLine::judge_tol(const AlgToolIn& in)
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

int PclFitLine::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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

int PclFitLine::execute(const AlgToolIn& in, AlgToolOut* out)
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
