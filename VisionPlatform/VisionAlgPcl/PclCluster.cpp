/**
 * @file PclCluster.cpp
 * @brief 欧氏聚类分割（PCL EuclideanClusterExtraction）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclCluster.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>
#if defined(VISION_HAS_PCL)
#  include <pcl/point_cloud.h>
#  include <pcl/point_types.h>
#  include <pcl/search/kdtree.h>
#  include <pcl/segmentation/extract_clusters.h>
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "欧氏聚类分割" } };
}

std::vector<VerNote> PclCluster::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclCluster::PclCluster() : shot_err_(0) {}
PclCluster::~PclCluster() {}

int PclCluster::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclCluster::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaCluster local;
    const ToolParaCluster* p = paraOf(in, &local);
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

int PclCluster::alg_run(const AlgToolIn& in)
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
    double tol = para_.run.tolerance;
    if (tol <= 0.0) {
        tol = 2.0;
    }
    int minSize = para_.run.minSize;
    if (minSize < 1) {
        minSize = 1;
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts = ops_.to_xyz(cloud_);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    tree->setInputCloud(pts);
    std::vector<pcl::PointIndices> clusters;
    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    ec.setClusterTolerance(tol);
    ec.setMinClusterSize(minSize);
    ec.setMaxClusterSize(static_cast<int>(pts->size()));
    ec.setSearchMethod(tree);
    ec.setInputCloud(pts);
    ec.extract(clusters);
    // 输出最大簇点云；其余簇只计数量
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
        pc.port = "cluster0";
        result_.clouds.push_back(pc);
    }
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("count", static_cast<int>(clusters.size()));
    result_.values.insert("points", static_cast<int>(biggestN));
    char msg[64];
    sprintf_s(msg, "聚类 %zu 个", clusters.size());
    result_.message = msg;
    return HS_OK;
#endif
}

int PclCluster::judge_tol(const AlgToolIn& in)
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

int PclCluster::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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

int PclCluster::execute(const AlgToolIn& in, AlgToolOut* out)
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
