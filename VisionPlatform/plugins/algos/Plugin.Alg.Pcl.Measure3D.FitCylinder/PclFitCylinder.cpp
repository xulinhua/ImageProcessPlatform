#include "PclFitCylinder.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <cmath>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/segmentation/sac_segmentation.h>) && __has_include(<pcl/features/normal_3d.h>)
#    define VISION_PCL_CYL_USE 1
#    include <pcl/features/normal_3d.h>
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#    include <pcl/search/kdtree.h>
#    include <pcl/segmentation/sac_segmentation.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "SACMODEL_CYLINDER" } };
}
std::vector<VerNote> PclFitCylinder::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclFitCylinder::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_CYL_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.xyz.size() < 10) {
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
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>());
    pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>());
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    ne.setSearchMethod(tree);
    ne.setInputCloud(pts);
    ne.setKSearch(20);
    ne.compute(*normals);
    pcl::SACSegmentationFromNormals<pcl::PointXYZ, pcl::Normal> seg;
    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_CYLINDER);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setNormalDistanceWeight(0.1);
    seg.setMaxIterations(2000);
    seg.setDistanceThreshold(distance);
    seg.setRadiusLimits(0.0, 1e6);
    seg.setInputCloud(pts);
    seg.setInputNormals(normals);
    pcl::PointIndices idx;
    pcl::ModelCoefficients coeff;
    seg.segment(idx, coeff);
    // point_on_axis(3) + axis_direction(3) + radius(1)
    if (coeff.values.size() < 7 || idx.indices.size() < 10) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    HsCommData::Cylinder3 cyl;
    cyl.center = HsCommData::Point3d(coeff.values[0], coeff.values[1], coeff.values[2]);
    cyl.axis = HsCommData::Point3d(coeff.values[3], coeff.values[4], coeff.values[5]);
    cyl.radius = coeff.values[6];
    cyl.halfHeight = -1.0;
    HsCommData::Cloud3 kept;
    double sumSq = 0;
    for (size_t i = 0; i < idx.indices.size(); ++i) {
        const HsCommData::Point3d& p = cloud.xyz[static_cast<size_t>(idx.indices[i])];
        kept.xyz.push_back(p);
        const HsCommData::Point3d v(p.x - cyl.center.x, p.y - cyl.center.y, p.z - cyl.center.z);
        const double t = v.x * cyl.axis.x + v.y * cyl.axis.y + v.z * cyl.axis.z;
        const HsCommData::Point3d radial(v.x - cyl.axis.x * t, v.y - cyl.axis.y * t, v.z - cyl.axis.z * t);
        const double r = std::sqrt(radial.x * radial.x + radial.y * radial.y + radial.z * radial.z);
        const double e = r - cyl.radius;
        sumSq += e * e;
    }
    const double rms = idx.indices.empty() ? 0.0 : std::sqrt(sumSq / static_cast<double>(idx.indices.size()));
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("cx", cyl.center.x); out->values.insert("cy", cyl.center.y); out->values.insert("cz", cyl.center.z);
    out->values.insert("ax", cyl.axis.x); out->values.insert("ay", cyl.axis.y); out->values.insert("az", cyl.axis.z);
    out->values.insert("radius", cyl.radius); out->values.insert("rms", rms);
    out->values.insert("inliers", static_cast<int>(idx.indices.size()));
    PortShape3 shape; shape.kind = Shape3Kind::Cylinder; shape.cylinder = cyl; out->shapes3.push_back(shape);
    PortCloud pc; pc.port = "inlier"; pc.cloud = kept; out->clouds.push_back(pc);
    PortTuple tuple; HsCommData::HsTuple values;
    values.append(cyl.center.x); values.append(cyl.center.y); values.append(cyl.center.z);
    values.append(cyl.axis.x); values.append(cyl.axis.y); values.append(cyl.axis.z);
    values.append(cyl.radius); values.append(rms); values.append(static_cast<double>(idx.indices.size()));
    tuple.value = values; out->tuples.push_back(tuple);
    char msg[128]; sprintf_s(msg, "r %.4f  rms %.4f", cyl.radius, rms); out->message = msg;
    return HS_OK;
#endif
}
}