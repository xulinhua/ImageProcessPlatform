#include "PclOutlier.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/filters/statistical_outlier_removal.h>) && __has_include(<pcl/point_types.h>)
#    define VISION_PCL_OUTLIER_USE 1
#    include <pcl/filters/statistical_outlier_removal.h>
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "StatisticalOutlierRemoval" } };
}
std::vector<VerNote> PclOutlier::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclOutlier::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_OUTLIER_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    int meanK = in.run.value("meanK").toInt(50);
    if (meanK < 1) { meanK = 1; }
    double stddev = in.run.value("stddev").toDouble(1.0);
    if (stddev < 0.01) { stddev = 0.01; }
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts(new pcl::PointCloud<pcl::PointXYZ>());
    pts->resize(cloud.xyz.size());
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        (*pts)[i].x = static_cast<float>(cloud.xyz[i].x);
        (*pts)[i].y = static_cast<float>(cloud.xyz[i].y);
        (*pts)[i].z = static_cast<float>(cloud.xyz[i].z);
    }
    pcl::StatisticalOutlierRemoval<pcl::PointXYZ> sor;
    sor.setInputCloud(pts);
    sor.setMeanK(meanK);
    sor.setStddevMulThresh(stddev);
    pcl::PointCloud<pcl::PointXYZ> filtered;
    sor.filter(filtered);
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