/**
 * @file PclFitCylinder.cpp
 * @brief 圆柱拟合（PCL SACMODEL_CYLINDER，需法向）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclFitCylinder.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>
#if defined(VISION_HAS_PCL)
#  include <pcl/point_cloud.h>
#  include <pcl/point_types.h>
#  include <pcl/features/normal_3d.h>
#  include <pcl/search/kdtree.h>
#  include <pcl/segmentation/sac_segmentation.h>
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "圆柱 RANSAC 拟合" } };
}

std::vector<VerNote> PclFitCylinder::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclFitCylinder::PclFitCylinder() : shot_err_(0) {}
PclFitCylinder::~PclFitCylinder() {}

int PclFitCylinder::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclFitCylinder::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaFitCylinder local;
    const ToolParaFitCylinder* p = paraOf(in, &local);
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

int PclFitCylinder::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    if (cloud_.xyz.size() < 10) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "点太少，无法拟合圆柱";
        return shot_err_;
    }
    double dist = para_.run.distance;
    if (dist <= 0.0) {
        dist = 0.5;
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts = ops_.to_xyz(cloud_);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    ne.setInputCloud(pts);
    ne.setSearchMethod(tree);
    ne.setKSearch(30);
    pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
    ne.compute(*normals);

    pcl::SACSegmentationFromNormals<pcl::PointXYZ, pcl::Normal> seg;
    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_CYLINDER);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setNormalDistanceWeight(0.1);
    seg.setMaxIterations(1000);
    seg.setDistanceThreshold(dist);
    seg.setRadiusLimits(0.0, 1.0e6);
    seg.setInputCloud(pts);
    seg.setInputNormals(normals);
    pcl::PointIndices idx;
    pcl::ModelCoefficients coeff;
    seg.segment(idx, coeff);
    // 轴上一点 + 轴向 + 半径（7）
    if (coeff.values.size() < 7 || idx.indices.empty()) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "圆柱拟合失败";
        return shot_err_;
    }
    double dx = coeff.values[3], dy = coeff.values[4], dz = coeff.values[5];
    const double nn = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (nn > 1e-12) {
        dx /= nn; dy /= nn; dz /= nn;
    }
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("ox", coeff.values[0]);
    result_.values.insert("oy", coeff.values[1]);
    result_.values.insert("oz", coeff.values[2]);
    result_.values.insert("dx", dx);
    result_.values.insert("dy", dy);
    result_.values.insert("dz", dz);
    result_.values.insert("r", coeff.values[6]);
    result_.values.insert("inliers", static_cast<int>(idx.indices.size()));
    char msg[80];
    sprintf_s(msg, "圆柱 r=%.3f 内点 %zu", coeff.values[6], idx.indices.size());
    result_.message = msg;
    return HS_OK;
#endif
}

int PclFitCylinder::judge_tol(const AlgToolIn& in)
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

int PclFitCylinder::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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

int PclFitCylinder::execute(const AlgToolIn& in, AlgToolOut* out)
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
