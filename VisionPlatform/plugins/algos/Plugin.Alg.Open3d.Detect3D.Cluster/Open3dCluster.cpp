#include "Open3dCluster.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <stdio.h>
#include <map>
#include <vector>

#if defined(__has_include)
#  if __has_include(<open3d/geometry/PointCloud.h>)
#    define VISION_O3_CLUSTER_USE 1
#    include <open3d/geometry/PointCloud.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D DBSCAN" } };
}
std::vector<VerNote> Open3dCluster::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int Open3dCluster::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_O3_CLUSTER_USE)
    return fail(HS_ERR_OPERATOR, "未安装 Open3D", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    double tol = in.run.value("tolerance").toDouble(2.0);
    if (tol < 0.01) { tol = 0.01; }
    int minSize = in.run.value("minSize").toInt(10);
    if (minSize < 1) { minSize = 1; }
    open3d::geometry::PointCloud pcd;
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    pcd.points_.resize(cloud.xyz.size());
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        pcd.points_[i] = Eigen::Vector3d(cloud.xyz[i].x, cloud.xyz[i].y, cloud.xyz[i].z);
    }
    std::vector<int> labels = pcd.ClusterDBSCAN(tol, minSize, false);
    std::map<int, HsCommData::Cloud3> groups;
    for (size_t i = 0; i < labels.size(); ++i) {
        if (labels[i] < 0) { continue; }
        groups[labels[i]].xyz.push_back(cloud.xyz[i]);
    }
    int total = 0; int index = 0;
    for (auto it = groups.begin(); it != groups.end(); ++it, ++index) {
        PortCloud pc; pc.port = "cluster"; pc.index = index; pc.cloud = it->second;
        out->clouds.push_back(pc);
        total += static_cast<int>(it->second.xyz.size());
    }
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("clusters", static_cast<int>(groups.size()));
    out->values.insert("points", total);
    PortTuple tuple; HsCommData::HsTuple values;
    values.append(static_cast<double>(groups.size()));
    values.append(static_cast<double>(total));
    tuple.value = values; out->tuples.push_back(tuple);
    char msg[64]; sprintf_s(msg, "簇 %d", static_cast<int>(groups.size()));
    out->message = msg;
    return HS_OK;
#endif
}
}