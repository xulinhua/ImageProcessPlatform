/**
 * @file PclDiff.cpp
 * @brief 点云差分：相对参考云超距的点（KdTree）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclDiff.h"
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
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云差分（超距点）" } };
}

std::vector<VerNote> PclDiff::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclDiff::PclDiff() : shot_err_(0) {}
PclDiff::~PclDiff() {}

int PclDiff::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclDiff::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaDiff local;
    const ToolParaDiff* p = paraOf(in, &local);
    para_ = *p;
    if (para_.loadCode() != HS_OK) {
        shot_err_ = para_.loadCode();
        shot_msg_ = HsCommData::HsException(shot_err_).errorMessage();
        return shot_err_;
    }
    if (!in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) {
        cloud_ = in.clouds[0].cloud;
    }
    if (in.clouds.size() >= 2 && !in.clouds[1].cloud.isEmpty()) {
        cloud2_ = in.clouds[1].cloud;
    }
    for (size_t i = 0; i < in.clouds.size(); ++i) {
        if (in.clouds[i].port == "template" || in.clouds[i].port == "target" || in.clouds[i].port == "ref") {
            cloud2_ = in.clouds[i].cloud;
        }
    }
    return HS_OK;
#endif
}

int PclDiff::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    if (cloud_.isEmpty() || cloud2_.isEmpty()) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "需要场景点云与参考点云";
        return shot_err_;
    }
    double thr = para_.run.threshold;
    if (thr < 0.0) {
        thr = 0.0;
    }
    const double thr2 = thr * thr;
    pcl::PointCloud<pcl::PointXYZ>::Ptr scene = ops_.to_xyz(cloud_);
    pcl::PointCloud<pcl::PointXYZ>::Ptr ref = ops_.to_xyz(cloud2_);
    pcl::search::KdTree<pcl::PointXYZ> tree;
    tree.setInputCloud(ref);
    HsCommData::Cloud3 diff;
    diff.xyz.reserve(scene->size());
    for (size_t i = 0; i < scene->size(); ++i) {
        std::vector<int> idx(1);
        std::vector<float> dist2(1);
        if (tree.nearestKSearch((*scene)[i], 1, idx, dist2) <= 0) {
            continue;
        }
        if (dist2[0] > thr2) {
            const pcl::PointXYZ& pt = (*scene)[i];
            diff.xyz.push_back(HsCommData::Point3d(pt.x, pt.y, pt.z));
        }
    }
    PortCloud pc;
    pc.cloud = diff;
    result_.clouds.push_back(pc);
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("points", static_cast<int>(diff.xyz.size()));
    char msg[64];
    sprintf_s(msg, "差分 %zu 点", diff.xyz.size());
    result_.message = msg;
    return HS_OK;
#endif
}

int PclDiff::judge_tol(const AlgToolIn& in)
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

int PclDiff::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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

int PclDiff::execute(const AlgToolIn& in, AlgToolOut* out)
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
