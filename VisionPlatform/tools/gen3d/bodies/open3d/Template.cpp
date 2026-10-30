#include "Open3dTemplate.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<open3d/geometry/PointCloud.h>)
#    define VISION_O3_Template_USE 1
#    include <open3d/geometry/PointCloud.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D 体素降采样" } };
}
std::vector<VerNote> Open3dTemplate::versionNotes() const
{
    return verNotesFrom(kVerNotes, 1);
}

int Open3dTemplate::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) {
        return HS_ERR_NULL;
    }
    *out = AlgToolOut();
#if !defined(VISION_O3_Template_USE)
    return fail(HS_ERR_OPERATOR, "未安装 Open3D", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    double leaf = in.run.value("leaf").toDouble(1.0);
    if (leaf < 0.0) {
        leaf = 0.0;
    }
    HsCommData::Cloud3 result;
    if (leaf > 0.0) {
        open3d::geometry::PointCloud pcd;
        const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
        pcd.points_.resize(cloud.xyz.size());
        for (size_t i = 0; i < cloud.xyz.size(); ++i) {
            pcd.points_[i] = Eigen::Vector3d(cloud.xyz[i].x, cloud.xyz[i].y, cloud.xyz[i].z);
        }
        auto down = pcd.VoxelDownSample(leaf);
        result.xyz.resize(down->points_.size());
        for (size_t i = 0; i < down->points_.size(); ++i) {
            result.xyz[i] = HsCommData::Point3d(down->points_[i].x(), down->points_[i].y(), down->points_[i].z());
        }
    } else {
        result = in.clouds[0].cloud;
    }
    PortCloud pc;
    pc.cloud = result;
    out->clouds.push_back(pc);
    out->ok = true;
    out->errCode = HS_OK;
    out->values.insert("points", static_cast<int>(result.xyz.size()));
    char msg[64];
    sprintf_s(msg, "点数 %d", static_cast<int>(result.xyz.size()));
    out->message = msg;
    return HS_OK;
#endif
}
}
