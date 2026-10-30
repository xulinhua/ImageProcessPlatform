#include "Open3dCrop.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<open3d/geometry/PointCloud.h>)
#    define VISION_O3_CROP_USE 1
#    include <open3d/geometry/PointCloud.h>
#    include <open3d/geometry/BoundingVolume.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D Crop" } };
}
std::vector<VerNote> Open3dCrop::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int Open3dCrop::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_O3_CROP_USE)
    return fail(HS_ERR_OPERATOR, "未安装 Open3D", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    const double minX = in.run.value("minX").toDouble(-1000.0);
    const double minY = in.run.value("minY").toDouble(-1000.0);
    const double minZ = in.run.value("minZ").toDouble(-1000.0);
    const double maxX = in.run.value("maxX").toDouble(1000.0);
    const double maxY = in.run.value("maxY").toDouble(1000.0);
    const double maxZ = in.run.value("maxZ").toDouble(1000.0);
    open3d::geometry::PointCloud pcd;
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    pcd.points_.resize(cloud.xyz.size());
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        pcd.points_[i] = Eigen::Vector3d(cloud.xyz[i].x, cloud.xyz[i].y, cloud.xyz[i].z);
    }
    open3d::geometry::AxisAlignedBoundingBox box(
        Eigen::Vector3d(minX, minY, minZ), Eigen::Vector3d(maxX, maxY, maxZ));
    auto cropped = pcd.Crop(box);
    HsCommData::Cloud3 result;
    result.xyz.resize(cropped->points_.size());
    for (size_t i = 0; i < cropped->points_.size(); ++i) {
        result.xyz[i] = HsCommData::Point3d(cropped->points_[i].x(), cropped->points_[i].y(), cropped->points_[i].z());
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