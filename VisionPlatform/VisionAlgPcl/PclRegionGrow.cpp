/**
 * @file PclRegionGrow.cpp
 * @brief 区域生长分割（PCL RegionGrowing）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclRegionGrow.h"
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
#  include <pcl/segmentation/region_growing.h>
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "区域生长分割" } };
}

std::vector<VerNote> PclRegionGrow::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclRegionGrow::PclRegionGrow() : shot_err_(0) {}
PclRegionGrow::~PclRegionGrow() {}

int PclRegionGrow::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclRegionGrow::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaRegionGrow local;
    const ToolParaRegionGrow* p = paraOf(in, &local);
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

int PclRegionGrow::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    if (cloud_.xyz.size() < 10) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "点太少，无法区域生长";
        return shot_err_;
    }
    double smoothDeg = para_.run.smoothness;
    if (smoothDeg <= 0.0) {
        smoothDeg = 3.0;
    }
    int minSize = para_.run.minSize;
    if (minSize < 1) {
        minSize = 1;
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts = ops_.to_xyz(cloud_);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    ne.setInputCloud(pts);
    ne.setSearchMethod(tree);
    ne.setKSearch(30);
    pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
    ne.compute(*normals);

    pcl::RegionGrowing<pcl::PointXYZ, pcl::Normal> reg;
    reg.setMinClusterSize(minSize);
    reg.setMaxClusterSize(static_cast<int>(pts->size()));
    reg.setSearchMethod(tree);
    reg.setNumberOfNeighbours(30);
    reg.setInputCloud(pts);
    reg.setInputNormals(normals);
    reg.setSmoothnessThreshold(static_cast<float>(smoothDeg / 180.0 * 3.14159265358979323846));
    reg.setCurvatureThreshold(1.0f);
    std::vector<pcl::PointIndices> clusters;
    reg.extract(clusters);

    HsCommData::Cloud3 biggest;
    size_t biggestN = 0;
    for (size_t c = 0; c < clusters.size(); ++c) {
        if (clusters[c].indices.size() > biggestN) {
            biggestN = clusters[c].indices.size();
            biggest.xyz.clear();
            biggest.xyz.reserve(biggestN);
            for (size_t k = 0; k < clusters[c].indices.size(); ++k) {
                const pcl::PointXYZ& pt = (*pts)[static_cast<size_t>(clusters[c].indices[k])];
                biggest.xyz.push_back(HsCommData::Point3d(pt.x, pt.y, pt.z));
            }
        }
    }
    if (!biggest.isEmpty()) {
        PortCloud pc;
        pc.cloud = biggest;
        result_.clouds.push_back(pc);
    }
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("count", static_cast<int>(clusters.size()));
    result_.values.insert("points", static_cast<int>(biggestN));
    char msg[64];
    sprintf_s(msg, "区域 %zu 个", clusters.size());
    result_.message = msg;
    return HS_OK;
#endif
}

int PclRegionGrow::judge_tol(const AlgToolIn& in)
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

int PclRegionGrow::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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

int PclRegionGrow::execute(const AlgToolIn& in, AlgToolOut* out)
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
