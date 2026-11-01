#include "PclDownsample.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>
#if defined(VISION_HAS_PCL)
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>

#endif
namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "prepare3d.downsample pipeline" } };
}
std::vector<VerNote> PclDownsample::version_notes() const { return verNotesFrom(kVerNotes, 1); }
PclDownsample::PclDownsample() : shot_err_(0) {}
PclDownsample::~PclDownsample() {}
int PclDownsample::tool_reset() { tol_.reset(); result_=AlgToolOut(); shot_err_=0; shot_msg_.clear(); cloud_=HsCommData::Cloud3(); cloud2_=HsCommData::Cloud3(); return HS_OK; }
int PclDownsample::proc_input(const AlgToolIn& in) {
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Pcl"; return shot_err_;
#else
    ToolParaDownsample local; const ToolParaDownsample* p = paraOf(in, &local); para_ = *p;
    if (!in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) cloud_ = in.clouds[0].cloud;
    if (in.clouds.size() >= 2 && !in.clouds[1].cloud.isEmpty()) cloud2_ = in.clouds[1].cloud;
    for (size_t i = 0; i < in.clouds.size(); ++i) {
        if (in.clouds[i].port == "template" || in.clouds[i].port == "target" || in.clouds[i].port == "ref") cloud2_ = in.clouds[i].cloud;
    }
    return HS_OK;
#endif
}
int PclDownsample::alg_run(const AlgToolIn& in) {
    (void)in;
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Pcl"; return shot_err_;
#else
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    HsCommData::Cloud3 result = cloud_;
    if (para_.run.leaf > 0.0) {
        auto pts = ops_.to_xyz(cloud_);
        pcl::VoxelGrid<pcl::PointXYZ> vg; vg.setInputCloud(pts);
        float l = static_cast<float>(para_.run.leaf); vg.setLeafSize(l,l,l);
        pcl::PointCloud<pcl::PointXYZ> filtered; vg.filter(filtered);
        result = ops_.from_xyz(filtered);
    }
    PortCloud pc; pc.cloud = result; result_.clouds.push_back(pc);
    result_.ok = true; result_.errCode = HS_OK;
    result_.values.insert("points", static_cast<int>(result.xyz.size()));
    return HS_OK;
#endif
}
int PclDownsample::judge_tol(const AlgToolIn& in) {
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
int PclDownsample::alg_rst_proc(const AlgToolIn&, AlgToolOut* out) {
    result_.errCode = shot_err_;
    if (shot_err_ != HS_OK) { result_.ok=false; result_.runOk=false; if(!shot_msg_.empty()) result_.message=shot_msg_; }
    else { result_.runOk=true; if(!result_.ok && result_.errCode==HS_OK) result_.ok = tol_.ok; if(result_.ok) result_.ok = tol_.ok; tol_.apply(&result_); }
    *out = result_; return shot_err_;
}
int PclDownsample::execute(const AlgToolIn& in, AlgToolOut* out) {
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
