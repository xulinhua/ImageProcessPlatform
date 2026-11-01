/**
 * @file PclNormals.cpp
 * @brief 点云法向估计（PCL NormalEstimation）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclNormals.h"
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
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云法向估计" } };
}

std::vector<VerNote> PclNormals::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclNormals::PclNormals() : shot_err_(0) {}
PclNormals::~PclNormals() {}

int PclNormals::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclNormals::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaNormals local;
    const ToolParaNormals* p = paraOf(in, &local);
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

int PclNormals::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    if (cloud_.xyz.size() < 3) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "点太少，无法估法向";
        return shot_err_;
    }
    double radius = para_.run.radius;
    if (radius <= 0.0) {
        radius = 5.0;
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts = ops_.to_xyz(cloud_);
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    ne.setInputCloud(pts);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>());
    ne.setSearchMethod(tree);
    ne.setRadiusSearch(radius);
    pcl::PointCloud<pcl::Normal> normals;
    ne.compute(normals);
    HsCommData::Cloud3 outCloud = cloud_;
    outCloud.normals.resize(outCloud.xyz.size());
    size_t okCount = 0;
    for (size_t i = 0; i < outCloud.xyz.size() && i < normals.size(); ++i) {
        if (!std::isfinite(normals[i].normal_x) || !std::isfinite(normals[i].normal_y) || !std::isfinite(normals[i].normal_z)) {
            outCloud.normals[i] = HsCommData::Point3d(0, 0, 1);
            continue;
        }
        outCloud.normals[i] = HsCommData::Point3d(normals[i].normal_x, normals[i].normal_y, normals[i].normal_z);
        ++okCount;
    }
    PortCloud pc;
    pc.cloud = outCloud;
    result_.clouds.push_back(pc);
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("points", static_cast<int>(outCloud.xyz.size()));
    result_.values.insert("normals", static_cast<int>(okCount));
    char msg[64];
    sprintf_s(msg, "法向 %zu/%zu", okCount, outCloud.xyz.size());
    result_.message = msg;
    return HS_OK;
#endif
}

int PclNormals::judge_tol(const AlgToolIn& in)
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

int PclNormals::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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

int PclNormals::execute(const AlgToolIn& in, AlgToolOut* out)
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
