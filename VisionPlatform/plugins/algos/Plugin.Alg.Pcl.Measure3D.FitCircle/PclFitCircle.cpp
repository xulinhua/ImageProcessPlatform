#include "PclFitCircle.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <cmath>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/segmentation/sac_segmentation.h>) && __has_include(<pcl/point_types.h>)
#    define VISION_PCL_CIRCLE_USE 1
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#    include <pcl/segmentation/sac_segmentation.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "SACMODEL_CIRCLE3D" } };
}
std::vector<VerNote> PclFitCircle::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclFitCircle::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_CIRCLE_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.xyz.size() < 3) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    double distance = in.run.value("distance").toDouble(0.5);
    if (distance < 0.0) { distance = 0.0; }
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts(new pcl::PointCloud<pcl::PointXYZ>());
    pts->resize(cloud.xyz.size());
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        (*pts)[i].x = static_cast<float>(cloud.xyz[i].x);
        (*pts)[i].y = static_cast<float>(cloud.xyz[i].y);
        (*pts)[i].z = static_cast<float>(cloud.xyz[i].z);
    }
    pcl::SACSegmentation<pcl::PointXYZ> seg;
    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_CIRCLE3D);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(distance);
    seg.setMaxIterations(2000);
    seg.setInputCloud(pts);
    pcl::PointIndices idx;
    pcl::ModelCoefficients coeff;
    seg.segment(idx, coeff);
    // CIRCLE3D: center(3) + radius(1) + normal(3)
    if (coeff.values.size() < 7 || idx.indices.size() < 3) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    HsCommData::Circle3 circle;
    circle.center = HsCommData::Point3d(coeff.values[0], coeff.values[1], coeff.values[2]);
    circle.radius = coeff.values[3];
    circle.normal = HsCommData::Point3d(coeff.values[4], coeff.values[5], coeff.values[6]);
    HsCommData::Cloud3 kept;
    double sumSq = 0;
    for (size_t i = 0; i < idx.indices.size(); ++i) {
        const HsCommData::Point3d& p = cloud.xyz[static_cast<size_t>(idx.indices[i])];
        kept.xyz.push_back(p);
        const double dx = p.x - circle.center.x, dy = p.y - circle.center.y, dz = p.z - circle.center.z;
        const double r = std::sqrt(dx * dx + dy * dy + dz * dz);
        const double e = r - circle.radius;
        sumSq += e * e;
    }
    const double rms = idx.indices.empty() ? 0.0 : std::sqrt(sumSq / static_cast<double>(idx.indices.size()));
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("cx", circle.center.x); out->values.insert("cy", circle.center.y); out->values.insert("cz", circle.center.z);
    out->values.insert("nx", circle.normal.x); out->values.insert("ny", circle.normal.y); out->values.insert("nz", circle.normal.z);
    out->values.insert("radius", circle.radius); out->values.insert("rms", rms);
    out->values.insert("inliers", static_cast<int>(idx.indices.size()));
    PortShape3 shape; shape.kind = Shape3Kind::Circle; shape.circle = circle; out->shapes3.push_back(shape);
    PortCloud pc; pc.port = "inlier"; pc.cloud = kept; out->clouds.push_back(pc);
    PortTuple tuple; HsCommData::HsTuple values;
    values.append(circle.center.x); values.append(circle.center.y); values.append(circle.center.z);
    values.append(circle.radius); values.append(rms); values.append(static_cast<double>(idx.indices.size()));
    tuple.value = values; out->tuples.push_back(tuple);
    char msg[128]; sprintf_s(msg, "r %.4f  rms %.4f", circle.radius, rms); out->message = msg;
    return HS_OK;
#endif
}
}