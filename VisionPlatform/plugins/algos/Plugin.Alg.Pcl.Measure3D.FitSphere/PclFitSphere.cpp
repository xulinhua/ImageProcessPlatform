#include "PclFitSphere.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <cmath>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/segmentation/sac_segmentation.h>) && __has_include(<pcl/point_types.h>)
#    define VISION_SPHERE_USE_PCL 1
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#    include <pcl/segmentation/sac_segmentation.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "RANSAC 拟合球" } };
}
std::vector<VerNote> PclFitSphere::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclFitSphere::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) {
        return HS_ERR_NULL;
    }
    *out = AlgToolOut();
#if !defined(VISION_SPHERE_USE_PCL)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    double distance = in.run.value("distance").toDouble(0.5);
    if (distance < 0.0) {
        distance = 0.0;
    }
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    const int n = static_cast<int>(cloud.xyz.size());
    if (n < 4) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts(new pcl::PointCloud<pcl::PointXYZ>());
    pts->resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        (*pts)[static_cast<size_t>(i)].x = static_cast<float>(cloud.xyz[static_cast<size_t>(i)].x);
        (*pts)[static_cast<size_t>(i)].y = static_cast<float>(cloud.xyz[static_cast<size_t>(i)].y);
        (*pts)[static_cast<size_t>(i)].z = static_cast<float>(cloud.xyz[static_cast<size_t>(i)].z);
    }
    pcl::SACSegmentation<pcl::PointXYZ> seg;
    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_SPHERE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(distance);
    seg.setMaxIterations(1000);
    seg.setInputCloud(pts);
    pcl::PointIndices idx;
    pcl::ModelCoefficients coeff;
    seg.segment(idx, coeff);
    if (coeff.values.size() < 4 || idx.indices.size() < 4) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    HsCommData::Sphere3 sphere;
    sphere.center = HsCommData::Point3d(coeff.values[0], coeff.values[1], coeff.values[2]);
    sphere.radius = coeff.values[3];
    double sumSq = 0;
    HsCommData::Cloud3 kept;
    for (size_t i = 0; i < idx.indices.size(); ++i) {
        const HsCommData::Point3d& p = cloud.xyz[static_cast<size_t>(idx.indices[i])];
        const double d = HsCommData::dist(sphere.center, p) - sphere.radius;
        sumSq += d * d;
        kept.xyz.push_back(p);
    }
    const double rms = idx.indices.empty() ? 0.0 : std::sqrt(sumSq / static_cast<double>(idx.indices.size()));
    out->ok = true;
    out->errCode = HS_OK;
    out->values.insert("cx", sphere.center.x);
    out->values.insert("cy", sphere.center.y);
    out->values.insert("cz", sphere.center.z);
    out->values.insert("radius", sphere.radius);
    out->values.insert("rms", rms);
    out->values.insert("inliers", static_cast<int>(idx.indices.size()));
    out->values.insert("points", n);
    PortShape3 shape;
    shape.kind = Shape3Kind::Sphere;
    shape.sphere = sphere;
    out->shapes3.push_back(shape);
    PortCloud pc;
    pc.port = "inlier";
    pc.cloud = kept;
    out->clouds.push_back(pc);
    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(sphere.center.x);
    values.append(sphere.center.y);
    values.append(sphere.center.z);
    values.append(sphere.radius);
    values.append(rms);
    values.append(static_cast<double>(idx.indices.size()));
    tuple.value = values;
    out->tuples.push_back(tuple);
    char msg[128];
    sprintf_s(msg, "r %.4f  rms %.4f", sphere.radius, rms);
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
