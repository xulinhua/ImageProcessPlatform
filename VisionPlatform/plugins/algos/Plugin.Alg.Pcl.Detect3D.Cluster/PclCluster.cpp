#include "PclCluster.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/segmentation/extract_clusters.h>) && __has_include(<pcl/point_types.h>)
#    define VISION_PCL_CLUSTER_USE 1
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#    include <pcl/search/kdtree.h>
#    include <pcl/segmentation/extract_clusters.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "EuclideanClusterExtraction" } };
}
std::vector<VerNote> PclCluster::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclCluster::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_CLUSTER_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    double tol = in.run.value("tolerance").toDouble(2.0);
    if (tol < 0.01) { tol = 0.01; }
    int minSize = in.run.value("minSize").toInt(10);
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
    tree->setInputCloud(pts);
    std::vector<pcl::PointIndices> clusters;
    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    ec.setClusterTolerance(tol);
    ec.setMinClusterSize(minSize);
    ec.setMaxClusterSize(static_cast<int>(pts->size()));
    ec.setSearchMethod(tree);
    ec.setInputCloud(pts);
    ec.extract(clusters);
    int total = 0;
    for (size_t c = 0; c < clusters.size(); ++c) {
        HsCommData::Cloud3 one;
        one.xyz.reserve(clusters[c].indices.size());
        for (size_t i = 0; i < clusters[c].indices.size(); ++i) {
            const int idx = clusters[c].indices[i];
            one.xyz.push_back(cloud.xyz[static_cast<size_t>(idx)]);
        }
        PortCloud pc;
        pc.port = "cluster";
        pc.index = static_cast<int>(c);
        pc.cloud = one;
        out->clouds.push_back(pc);
        total += static_cast<int>(one.xyz.size());
    }
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("clusters", static_cast<int>(clusters.size()));
    out->values.insert("points", total);
    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(static_cast<double>(clusters.size()));
    values.append(static_cast<double>(total));
    tuple.value = values;
    out->tuples.push_back(tuple);
    char msg[64]; sprintf_s(msg, "簇 %d", static_cast<int>(clusters.size()));
    out->message = msg;
    return HS_OK;
#endif
}
}