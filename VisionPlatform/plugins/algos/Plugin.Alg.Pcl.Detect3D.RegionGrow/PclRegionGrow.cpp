#include "PclRegionGrow.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <stdio.h>
#include <cmath>

#if defined(__has_include)
#  if __has_include(<pcl/segmentation/region_growing.h>) && __has_include(<pcl/features/normal_3d.h>)
#    define VISION_PCL_RG_USE 1
#    include <pcl/features/normal_3d.h>
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#    include <pcl/search/kdtree.h>
#    include <pcl/segmentation/region_growing.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "RegionGrowing" } };
}
std::vector<VerNote> PclRegionGrow::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclRegionGrow::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_RG_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    double smoothnessDeg = in.run.value("smoothness").toDouble(3.0);
    if (smoothnessDeg < 0.1) { smoothnessDeg = 0.1; }
    int minSize = in.run.value("minSize").toInt(50);
    if (minSize < 1) { minSize = 1; }
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
    ne.setInputCloud(pts);
    ne.setSearchMethod(tree);
    ne.setKSearch(20);
    ne.compute(*normals);
    pcl::RegionGrowing<pcl::PointXYZ, pcl::Normal> reg;
    reg.setMinClusterSize(minSize);
    reg.setMaxClusterSize(static_cast<int>(pts->size()));
    reg.setSearchMethod(tree);
    reg.setNumberOfNeighbours(30);
    reg.setInputCloud(pts);
    reg.setInputNormals(normals);
    reg.setSmoothnessThreshold(static_cast<float>(smoothnessDeg / 180.0 * 3.14159265358979323846));
    reg.setCurvatureThreshold(1.0);
    std::vector<pcl::PointIndices> clusters;
    reg.extract(clusters);
    if (clusters.empty()) {
        return fail(HS_ERR_OPERATOR, "未生长出区域", out);
    }
    // 输出最大簇
    size_t best = 0;
    for (size_t i = 1; i < clusters.size(); ++i) {
        if (clusters[i].indices.size() > clusters[best].indices.size()) { best = i; }
    }
    HsCommData::Cloud3 result;
    result.xyz.reserve(clusters[best].indices.size());
    for (size_t i = 0; i < clusters[best].indices.size(); ++i) {
        result.xyz.push_back(cloud.xyz[static_cast<size_t>(clusters[best].indices[i])]);
    }
    PortCloud pc; pc.cloud = result; out->clouds.push_back(pc);
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("regions", static_cast<int>(clusters.size()));
    out->values.insert("points", static_cast<int>(result.xyz.size()));
    char msg[64]; sprintf_s(msg, "区域 %d 最大 %d", static_cast<int>(clusters.size()), static_cast<int>(result.xyz.size()));
    out->message = msg;
    return HS_OK;
#endif
}
}