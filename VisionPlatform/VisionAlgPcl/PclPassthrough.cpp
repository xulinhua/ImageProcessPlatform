/**
 * @file PclPassthrough.cpp
 * @brief 点云单轴直通滤波（PCL PassThrough）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclPassthrough.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>
#if defined(VISION_HAS_PCL)
#  include <pcl/point_cloud.h>
#  include <pcl/point_types.h>
#  include <pcl/filters/passthrough.h>
#endif

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云单轴直通滤波" } };
}

std::vector<VerNote> PclPassthrough::version_notes() const
{
    return verNotesFrom(kVerNotes, 1);
}

PclPassthrough::PclPassthrough() : shot_err_(0) {}
PclPassthrough::~PclPassthrough() {}

int PclPassthrough::tool_reset()
{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}

int PclPassthrough::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaPassthrough local;
    const ToolParaPassthrough* p = paraOf(in, &local);
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

int PclPassthrough::alg_run(const AlgToolIn& in)
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
    std::string axis = para_.run.axis;
    if (axis != "x" && axis != "X" && axis != "y" && axis != "Y" && axis != "z" && axis != "Z") {
        axis = "z";
    }
    if (axis.size() == 1 && axis[0] >= 'A' && axis[0] <= 'Z') {
        axis[0] = static_cast<char>(axis[0] - 'A' + 'a');
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts = ops_.to_xyz(cloud_);
    pcl::PassThrough<pcl::PointXYZ> pass;
    pass.setInputCloud(pts);
    pass.setFilterFieldName(axis);
    pass.setFilterLimits(static_cast<float>(para_.run.min), static_cast<float>(para_.run.max));
    pcl::PointCloud<pcl::PointXYZ> filtered;
    pass.filter(filtered);
    HsCommData::Cloud3 outCloud = ops_.from_xyz(filtered);
    PortCloud pc;
    pc.cloud = outCloud;
    result_.clouds.push_back(pc);
    result_.ok = true;
    result_.errCode = HS_OK;
    result_.values.insert("points", static_cast<int>(outCloud.xyz.size()));
    char msg[64];
    sprintf_s(msg, "直通(%s)后 %zu 点", axis.c_str(), outCloud.xyz.size());
    result_.message = msg;
    return HS_OK;
#endif
}

int PclPassthrough::judge_tol(const AlgToolIn& in)
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

int PclPassthrough::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
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

int PclPassthrough::execute(const AlgToolIn& in, AlgToolOut* out)
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
