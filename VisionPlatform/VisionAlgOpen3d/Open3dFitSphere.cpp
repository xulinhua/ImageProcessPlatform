#include "Open3dFitSphere.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>
#if defined(VISION_HAS_OPEN3D)
#include <open3d/geometry/PointCloud.h>

#endif
namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "measure3d.fit_sphere pipeline" } };
}
std::vector<VerNote> Open3dFitSphere::version_notes() const { return verNotesFrom(kVerNotes, 1); }
Open3dFitSphere::Open3dFitSphere() : shot_err_(0) {}
Open3dFitSphere::~Open3dFitSphere() {}
int Open3dFitSphere::tool_reset() { tol_.reset(); result_=AlgToolOut(); shot_err_=0; shot_msg_.clear(); cloud_=HsCommData::Cloud3(); cloud2_=HsCommData::Cloud3(); return HS_OK; }
int Open3dFitSphere::proc_input(const AlgToolIn& in) {
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3d"; return shot_err_;
#else
    ToolParaFitSphere local; const ToolParaFitSphere* p = paraOf(in, &local); para_ = *p;
    if (!in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) cloud_ = in.clouds[0].cloud;
    if (in.clouds.size() >= 2 && !in.clouds[1].cloud.isEmpty()) cloud2_ = in.clouds[1].cloud;
    for (size_t i = 0; i < in.clouds.size(); ++i) {
        if (in.clouds[i].port == "template" || in.clouds[i].port == "target" || in.clouds[i].port == "ref") cloud2_ = in.clouds[i].cloud;
    }
    return HS_OK;
#endif
}
int Open3dFitSphere::alg_run(const AlgToolIn& in) {
    (void)in;
#if !defined(VISION_HAS_OPEN3D)
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未安装 Open3d"; return shot_err_;
#else
    if (cloud_.xyz.size() < 4) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    size_t n = cloud_.xyz.size(); Eigen::MatrixXd A(n,4); Eigen::VectorXd b(n);
    for (size_t i=0;i<n;++i){ double x=cloud_.xyz[i].x,y=cloud_.xyz[i].y,z=cloud_.xyz[i].z; A(i,0)=2*x;A(i,1)=2*y;A(i,2)=2*z;A(i,3)=1; b(i)=x*x+y*y+z*z; }
    Eigen::VectorXd sol = A.colPivHouseholderQr().solve(b);
    double cx=sol(0),cy=sol(1),cz=sol(2); double r=std::sqrt(std::max(0.0, sol(3)+cx*cx+cy*cy+cz*cz));
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("cx",cx); result_.values.insert("cy",cy); result_.values.insert("cz",cz); result_.values.insert("r",r);
    return HS_OK;
#endif
}
int Open3dFitSphere::judge_tol(const AlgToolIn& in) {
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
int Open3dFitSphere::alg_rst_proc(const AlgToolIn&, AlgToolOut* out) {
    result_.errCode = shot_err_;
    if (shot_err_ != HS_OK) { result_.ok=false; result_.runOk=false; if(!shot_msg_.empty()) result_.message=shot_msg_; }
    else { result_.runOk=true; if(!result_.ok && result_.errCode==HS_OK) result_.ok = tol_.ok; if(result_.ok) result_.ok = tol_.ok; tol_.apply(&result_); }
    *out = result_; return shot_err_;
}
int Open3dFitSphere::execute(const AlgToolIn& in, AlgToolOut* out) {
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
