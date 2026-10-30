#include "PclNormals.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/features/normal_3d.h>) && __has_include(<pcl/point_types.h>)
#    define VISION_PCL_NORMALS_USE 1
#    include <pcl/features/normal_3d.h>
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#    include <pcl/search/kdtree.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "NormalEstimation" } };
}
std::vector<VerNote> PclNormals::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclNormals::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_NORMALS_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    double radius = in.run.value("radius").toDouble(5.0);
    if (radius < 0.01) { radius = 0.01; }
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts(new pcl::PointCloud<pcl::PointXYZ>());
    pts->resize(cloud.xyz.size());
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        (*pts)[i].x = static_cast<float>(cloud.xyz[i].x);
        (*pts)[i].y = static_cast<float>(cloud.xyz[i].y);
        (*pts)[i].z = static_cast<float>(cloud.xyz[i].z);
    }
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    ne.setInputCloud(pts);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>());
    ne.setSearchMethod(tree);
    ne.setRadiusSearch(radius);
    pcl::PointCloud<pcl::Normal> normals;
    ne.compute(normals);
    HsCommData::Cloud3 result;
    result.xyz = cloud.xyz;
    result.normals.resize(normals.size());
    for (size_t i = 0; i < normals.size(); ++i) {
        result.normals[i] = HsCommData::Point3d(normals[i].normal_x, normals[i].normal_y, normals[i].normal_z);
    }
    PortCloud pc; pc.cloud = result; out->clouds.push_back(pc);
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("points", static_cast<int>(result.xyz.size()));
    char msg[64]; sprintf_s(msg, "点数 %d", static_cast<int>(result.xyz.size()));
    out->message = msg;
    return HS_OK;
#endif
}
}