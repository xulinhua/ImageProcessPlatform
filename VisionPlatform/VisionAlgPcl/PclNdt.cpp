/**
 * @file PclNdt.cpp
 * @brief NDT 配准定位（PCL NormalDistributionsTransform）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclNdt.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>
#if defined(VISION_HAS_PCL)
#  include <pcl/point_cloud.h>
#  include <pcl/point_types.h>
#  include <pcl/registration/ndt.h>
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "NDT 配准定位" } };
}

std::vector<VerNote> PclNdt::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclNdt::PclNdt() : shot_err_(0) {}
PclNdt::~PclNdt() {}

int PclNdt::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclNdt::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaNdt local;
    const ToolParaNdt* p = paraOf(in, &local);
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
        } else if ((in.clouds[i].port == "scene" || in.clouds[i].port == "source" || in.clouds[i].port.empty())
            && !in.clouds[i].cloud.isEmpty()) {
            cloud_ = in.clouds[i].cloud;
        }
    }
    return HS_OK;
#endif
}

int PclNdt::alg_run(const AlgToolIn& in)
{
    (void)in;
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    if (cloud_.isEmpty() || cloud2_.isEmpty()) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "需要源点云与目标点云";
        return shot_err_;
    }
    double resolution = para_.run.resolution;
    if (resolution <= 0.0) {
        resolution = 1.0;
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr src = ops_.to_xyz(cloud_);
    pcl::PointCloud<pcl::PointXYZ>::Ptr tgt = ops_.to_xyz(cloud2_);
    pcl::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ> ndt;
    ndt.setInputSource(src);
    ndt.setInputTarget(tgt);
    ndt.setResolution(static_cast<float>(resolution));
    ndt.setMaximumIterations(para_.run.maxIter < 1 ? 1 : para_.run.maxIter);
    if (para_.run.maxCorr > 0.0) {
        ndt.setTransformationEpsilon(1e-6);
        ndt.setStepSize(0.1);
    }
    pcl::PointCloud<pcl::PointXYZ> aligned;
    ndt.align(aligned);
    if (!ndt.hasConverged()) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "NDT 未收敛";
        return shot_err_;
    }
    const HsCommData::Pose3 pose = ops_.matrix_to_pose(ndt.getFinalTransformation());
    const double fitness = ndt.getFitnessScore();
    PortCloud pc;
    pc.cloud = ops_.from_xyz(aligned);
    result_.clouds.push_back(pc);
    PortPose3 pp;
    pp.pose = pose;
    result_.poses3.push_back(pp);
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("tx", pose.tx);
    result_.values.insert("ty", pose.ty);
    result_.values.insert("tz", pose.tz);
    result_.values.insert("qx", pose.qx);
    result_.values.insert("qy", pose.qy);
    result_.values.insert("qz", pose.qz);
    result_.values.insert("qw", pose.qw);
    result_.values.insert("fitness", fitness);
    char msg[64];
    sprintf_s(msg, "NDT fitness %.4f", fitness);
    result_.message = msg;
    return HS_OK;
#endif
}

int PclNdt::judge_tol(const AlgToolIn& in)
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
    if (result_.values.contains("fitness")) {
        const double fitness = result_.values.value("fitness").toDouble(0.0);
        if (fitness > para_.run.minFitness) {
            tol_.ok = false;
            tol_.add_miss("fitness", fitness, 0.0, para_.run.minFitness);
        }
    }
    tol_.judge(para_.judge.empty() ? in.judge : para_.judge, nullptr);
    return HS_OK;
}

int PclNdt::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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

int PclNdt::execute(const AlgToolIn& in, AlgToolOut* out)
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
