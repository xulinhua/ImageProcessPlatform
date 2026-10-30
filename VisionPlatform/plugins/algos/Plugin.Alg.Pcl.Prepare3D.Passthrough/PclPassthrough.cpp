#include "PclPassthrough.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/filters/passthrough.h>) && __has_include(<pcl/point_types.h>)
#    define VISION_PCL_PASS_USE 1
#    include <pcl/filters/passthrough.h>
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "PassThrough" } };
}
std::vector<VerNote> PclPassthrough::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclPassthrough::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_PASS_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    std::string axis = in.run.value("axis").toString();
    if (axis.empty()) { axis = "z"; }
    if (axis != "x" && axis != "y" && axis != "z") { axis = "z"; }
    const double vmin = in.run.value("min").toDouble(-1e6);
    const double vmax = in.run.value("max").toDouble(1e6);
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts(new pcl::PointCloud<pcl::PointXYZ>());
    pts->resize(cloud.xyz.size());
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        (*pts)[i].x = static_cast<float>(cloud.xyz[i].x);
        (*pts)[i].y = static_cast<float>(cloud.xyz[i].y);
        (*pts)[i].z = static_cast<float>(cloud.xyz[i].z);
    }
    pcl::PassThrough<pcl::PointXYZ> pass;
    pass.setInputCloud(pts);
    pass.setFilterFieldName(axis);
    pass.setFilterLimits(static_cast<float>(vmin), static_cast<float>(vmax));
    pcl::PointCloud<pcl::PointXYZ> filtered;
    pass.filter(filtered);
    HsCommData::Cloud3 result;
    result.xyz.resize(filtered.size());
    for (size_t i = 0; i < filtered.size(); ++i) {
        result.xyz[i] = HsCommData::Point3d(filtered[i].x, filtered[i].y, filtered[i].z);
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