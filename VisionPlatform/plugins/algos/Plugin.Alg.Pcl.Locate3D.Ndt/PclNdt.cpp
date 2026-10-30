#include "PclNdt.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/registration/ndt.h>) && __has_include(<pcl/point_types.h>)
#    define VISION_PCL_NDT_USE 1
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#    include <pcl/registration/ndt.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "NormalDistributionsTransform" } };
}
std::vector<VerNote> PclNdt::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclNdt::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_NDT_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "没有源点云", out);
    }
    const HsCommData::Cloud3* tmpl = nullptr;
    for (size_t i = 0; i < in.clouds.size(); ++i) {
        if (in.clouds[i].port == "template") { tmpl = &in.clouds[i].cloud; break; }
    }
    if (!tmpl || tmpl->isEmpty()) {
        if (in.clouds.size() >= 2 && !in.clouds[1].cloud.isEmpty()) { tmpl = &in.clouds[1].cloud; }
    }
    if (!tmpl || tmpl->isEmpty()) {
        return fail(HS_ERR_OPERATOR, "没有模板点云", out);
    }
    double resolution = in.run.value("resolution").toDouble(1.0);
    if (resolution < 0.01) { resolution = 0.01; }
    int maxIter = in.run.value("maxIter").toInt(35);
    if (maxIter < 1) { maxIter = 1; }
    pcl::PointCloud<pcl::PointXYZ>::Ptr src(new pcl::PointCloud<pcl::PointXYZ>());
    pcl::PointCloud<pcl::PointXYZ>::Ptr tgt(new pcl::PointCloud<pcl::PointXYZ>());
    src->resize(in.clouds[0].cloud.xyz.size());
    tgt->resize(tmpl->xyz.size());
    for (size_t i = 0; i < in.clouds[0].cloud.xyz.size(); ++i) {
        (*src)[i].x = static_cast<float>(in.clouds[0].cloud.xyz[i].x);
        (*src)[i].y = static_cast<float>(in.clouds[0].cloud.xyz[i].y);
        (*src)[i].z = static_cast<float>(in.clouds[0].cloud.xyz[i].z);
    }
    for (size_t i = 0; i < tmpl->xyz.size(); ++i) {
        (*tgt)[i].x = static_cast<float>(tmpl->xyz[i].x);
        (*tgt)[i].y = static_cast<float>(tmpl->xyz[i].y);
        (*tgt)[i].z = static_cast<float>(tmpl->xyz[i].z);
    }
    pcl::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ> ndt;
    ndt.setTransformationEpsilon(0.01);
    ndt.setStepSize(0.1);
    ndt.setResolution(static_cast<float>(resolution));
    ndt.setMaximumIterations(maxIter);
    ndt.setInputSource(src);
    ndt.setInputTarget(tgt);
    pcl::PointCloud<pcl::PointXYZ> aligned;
    ndt.align(aligned);
    if (!ndt.hasConverged()) {
        return fail(HS_ERR_OPERATOR, "NDT 失败", out);
    }
    const Eigen::Matrix4f t = ndt.getFinalTransformation();
    const double m00 = t(0, 0), m01 = t(0, 1), m02 = t(0, 2);
    const double m10 = t(1, 0), m11 = t(1, 1), m12 = t(1, 2);
    const double m20 = t(2, 0), m21 = t(2, 1), m22 = t(2, 2);
    HsCommData::Pose3 pose;
    pose.tx = t(0, 3); pose.ty = t(1, 3); pose.tz = t(2, 3);
    const double tr = m00 + m11 + m22;
    if (tr > 0) {
        const double s = std::sqrt(tr + 1.0) * 2.0;
        pose.qw = 0.25 * s; pose.qx = (m21 - m12) / s; pose.qy = (m02 - m20) / s; pose.qz = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const double s = std::sqrt(1.0 + m00 - m11 - m22) * 2.0;
        pose.qw = (m21 - m12) / s; pose.qx = 0.25 * s; pose.qy = (m01 + m10) / s; pose.qz = (m02 + m20) / s;
    } else if (m11 > m22) {
        const double s = std::sqrt(1.0 + m11 - m00 - m22) * 2.0;
        pose.qw = (m02 - m20) / s; pose.qx = (m01 + m10) / s; pose.qy = 0.25 * s; pose.qz = (m12 + m21) / s;
    } else {
        const double s = std::sqrt(1.0 + m22 - m00 - m11) * 2.0;
        pose.qw = (m10 - m01) / s; pose.qx = (m02 + m20) / s; pose.qy = (m12 + m21) / s; pose.qz = 0.25 * s;
    }
    pose = pose.normalized();
    const double fitness = ndt.getFitnessScore();
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("tx", pose.tx); out->values.insert("ty", pose.ty); out->values.insert("tz", pose.tz);
    out->values.insert("qx", pose.qx); out->values.insert("qy", pose.qy); out->values.insert("qz", pose.qz); out->values.insert("qw", pose.qw);
    out->values.insert("fitness", fitness);
    PortPose3 pp; pp.pose = pose; out->poses3.push_back(pp);
    PortTuple tuple; HsCommData::HsTuple values;
    values.append(pose.tx); values.append(pose.ty); values.append(pose.tz);
    values.append(pose.qx); values.append(pose.qy); values.append(pose.qz); values.append(pose.qw);
    values.append(fitness); tuple.value = values; out->tuples.push_back(tuple);
    char msg[128]; sprintf_s(msg, "fitness %.4f", fitness); out->message = msg;
    return HS_OK;
#endif
}
}