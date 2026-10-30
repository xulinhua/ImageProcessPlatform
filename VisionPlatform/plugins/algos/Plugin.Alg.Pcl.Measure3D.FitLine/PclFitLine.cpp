#include "PclFitLine.h"
#include "VisionVersion.h"
#include <HsCommData/Geom3.h>
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <cmath>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/segmentation/sac_segmentation.h>) && __has_include(<pcl/point_types.h>)
#    define VISION_PCL_LINE_USE 1
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#    include <pcl/segmentation/sac_segmentation.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "SACMODEL_LINE" } };
}
std::vector<VerNote> PclFitLine::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclFitLine::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_LINE_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.xyz.size() < 2) {
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
    seg.setModelType(pcl::SACMODEL_LINE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(distance);
    seg.setMaxIterations(1000);
    seg.setInputCloud(pts);
    pcl::PointIndices idx;
    pcl::ModelCoefficients coeff;
    seg.segment(idx, coeff);
    if (coeff.values.size() < 6 || idx.indices.size() < 2) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    HsCommData::Point3d origin(coeff.values[0], coeff.values[1], coeff.values[2]);
    HsCommData::Point3d dir(coeff.values[3], coeff.values[4], coeff.values[5]);
    double tMin = 0, tMax = 0;
    bool first = true;
    HsCommData::Cloud3 kept;
    double sumSq = 0;
    for (size_t i = 0; i < idx.indices.size(); ++i) {
        const HsCommData::Point3d& p = cloud.xyz[static_cast<size_t>(idx.indices[i])];
        kept.xyz.push_back(p);
        const HsCommData::Point3d d = p - origin;
        const double t = d.x * dir.x + d.y * dir.y + d.z * dir.z;
        if (first) { tMin = tMax = t; first = false; }
        else { if (t < tMin) tMin = t; if (t > tMax) tMax = t; }
        const HsCommData::Point3d proj(origin.x + dir.x * t, origin.y + dir.y * t, origin.z + dir.z * t);
        const double dx = p.x - proj.x, dy = p.y - proj.y, dz = p.z - proj.z;
        sumSq += dx * dx + dy * dy + dz * dz;
    }
    HsCommData::Line3 line;
    line.a = HsCommData::Point3d(origin.x + dir.x * tMin, origin.y + dir.y * tMin, origin.z + dir.z * tMin);
    line.b = HsCommData::Point3d(origin.x + dir.x * tMax, origin.y + dir.y * tMax, origin.z + dir.z * tMax);
    const double rms = idx.indices.empty() ? 0.0 : std::sqrt(sumSq / static_cast<double>(idx.indices.size()));
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("ax", line.a.x); out->values.insert("ay", line.a.y); out->values.insert("az", line.a.z);
    out->values.insert("bx", line.b.x); out->values.insert("by", line.b.y); out->values.insert("bz", line.b.z);
    out->values.insert("rms", rms);
    out->values.insert("inliers", static_cast<int>(idx.indices.size()));
    PortShape3 shape; shape.kind = Shape3Kind::Line; shape.line = line; out->shapes3.push_back(shape);
    PortCloud pc; pc.port = "inlier"; pc.cloud = kept; out->clouds.push_back(pc);
    PortTuple tuple; HsCommData::HsTuple values;
    values.append(line.a.x); values.append(line.a.y); values.append(line.a.z);
    values.append(line.b.x); values.append(line.b.y); values.append(line.b.z);
    values.append(rms); values.append(static_cast<double>(idx.indices.size()));
    tuple.value = values; out->tuples.push_back(tuple);
    char msg[128]; sprintf_s(msg, "rms %.4f  内点 %d", rms, static_cast<int>(idx.indices.size()));
    out->message = msg;
    return HS_OK;
#endif
}
}