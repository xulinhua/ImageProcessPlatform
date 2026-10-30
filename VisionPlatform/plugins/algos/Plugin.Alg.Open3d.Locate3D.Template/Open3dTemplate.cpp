#include "Open3dTemplate.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<open3d/pipelines/registration/Registration.h>) && __has_include(<open3d/geometry/PointCloud.h>)
#    define VISION_ICP_USE_OPEN3D 1
#    include <open3d/geometry/PointCloud.h>
#    include <open3d/pipelines/registration/Registration.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D 模板 ICP" } };
}
std::vector<VerNote> Open3dTemplate::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int Open3dTemplate::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) {
        return HS_ERR_NULL;
    }
    *out = AlgToolOut();
#if !defined(VISION_ICP_USE_OPEN3D)
    return fail(HS_ERR_OPERATOR, "未安装 Open3D", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "没有源点云", out);
    }
    const HsCommData::Cloud3* tmpl = nullptr;
    for (size_t i = 0; i < in.clouds.size(); ++i) {
        if (in.clouds[i].port == "template") {
            tmpl = &in.clouds[i].cloud;
            break;
        }
    }
    if (!tmpl || tmpl->isEmpty()) {
        if (in.clouds.size() >= 2 && !in.clouds[1].cloud.isEmpty()) {
            tmpl = &in.clouds[1].cloud;
        }
    }
    if (!tmpl || tmpl->isEmpty()) {
        return fail(HS_ERR_OPERATOR, "没有模板点云", out);
    }
    double maxCorr = in.run.value("maxCorr").toDouble(5.0);
    int maxIter = in.run.value("maxIter").toInt(50);
    open3d::geometry::PointCloud source;
    open3d::geometry::PointCloud target;
    source.points_.resize(in.clouds[0].cloud.xyz.size());
    target.points_.resize(tmpl->xyz.size());
    for (size_t i = 0; i < in.clouds[0].cloud.xyz.size(); ++i) {
        source.points_[i] = Eigen::Vector3d(in.clouds[0].cloud.xyz[i].x,
                                            in.clouds[0].cloud.xyz[i].y,
                                            in.clouds[0].cloud.xyz[i].z);
    }
    for (size_t i = 0; i < tmpl->xyz.size(); ++i) {
        target.points_[i] = Eigen::Vector3d(tmpl->xyz[i].x, tmpl->xyz[i].y, tmpl->xyz[i].z);
    }
    open3d::pipelines::registration::ICPConvergenceCriteria crit;
    crit.max_iteration_ = maxIter;
    const auto result = open3d::pipelines::registration::RegistrationICP(
        source, target, maxCorr, Eigen::Matrix4d::Identity(),
        open3d::pipelines::registration::TransformationEstimationPointToPoint(), crit);
    const Eigen::Matrix4d& t = result.transformation_;
    const double m00 = t(0, 0), m01 = t(0, 1), m02 = t(0, 2);
    const double m10 = t(1, 0), m11 = t(1, 1), m12 = t(1, 2);
    const double m20 = t(2, 0), m21 = t(2, 1), m22 = t(2, 2);
    HsCommData::Pose3 pose;
    pose.tx = t(0, 3);
    pose.ty = t(1, 3);
    pose.tz = t(2, 3);
    const double tr = m00 + m11 + m22;
    if (tr > 0) {
        const double s = std::sqrt(tr + 1.0) * 2.0;
        pose.qw = 0.25 * s;
        pose.qx = (m21 - m12) / s;
        pose.qy = (m02 - m20) / s;
        pose.qz = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const double s = std::sqrt(1.0 + m00 - m11 - m22) * 2.0;
        pose.qw = (m21 - m12) / s;
        pose.qx = 0.25 * s;
        pose.qy = (m01 + m10) / s;
        pose.qz = (m02 + m20) / s;
    } else if (m11 > m22) {
        const double s = std::sqrt(1.0 + m11 - m00 - m22) * 2.0;
        pose.qw = (m02 - m20) / s;
        pose.qx = (m01 + m10) / s;
        pose.qy = 0.25 * s;
        pose.qz = (m12 + m21) / s;
    } else {
        const double s = std::sqrt(1.0 + m22 - m00 - m11) * 2.0;
        pose.qw = (m10 - m01) / s;
        pose.qx = (m02 + m20) / s;
        pose.qy = (m12 + m21) / s;
        pose.qz = 0.25 * s;
    }
    pose = pose.normalized();
    const double fitness = result.inlier_rmse_;
    out->ok = true;
    out->errCode = HS_OK;
    out->values.insert("tx", pose.tx);
    out->values.insert("ty", pose.ty);
    out->values.insert("tz", pose.tz);
    out->values.insert("qx", pose.qx);
    out->values.insert("qy", pose.qy);
    out->values.insert("qz", pose.qz);
    out->values.insert("qw", pose.qw);
    out->values.insert("fitness", fitness);
    PortPose3 pp;
    pp.pose = pose;
    out->poses3.push_back(pp);
    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(pose.tx);
    values.append(pose.ty);
    values.append(pose.tz);
    values.append(pose.qx);
    values.append(pose.qy);
    values.append(pose.qz);
    values.append(pose.qw);
    values.append(fitness);
    tuple.value = values;
    out->tuples.push_back(tuple);
    char msg[128];
    sprintf_s(msg, "fitness %.4f", fitness);
    out->message = msg;
    for (size_t i = 0; i < in.judge.size(); ++i) {
        const JudgeParam& row = in.judge[i];
        if (!row.enable || !out->values.contains(row.name)) {
            continue;
        }
        const ParamValue got = out->values.value(row.name);
        if (got.kind() != ParamValue::KindInt && got.kind() != ParamValue::KindDouble) {
            continue;
        }
        const double value = got.toDouble(0);
        if (value < row.minVal || value > row.maxVal) {
            out->ok = false;
            out->message = row.name + " 超出范围";
        }
    }
    return HS_OK;
#endif
}
}
