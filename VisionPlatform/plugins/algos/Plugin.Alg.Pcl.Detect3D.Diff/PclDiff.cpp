#include "PclDiff.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/kdtree/kdtree_flann.h>) && __has_include(<pcl/point_types.h>)
#    define VISION_PCL_DIFF_USE 1
#    include <pcl/kdtree/kdtree_flann.h>
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云差分" } };
}
std::vector<VerNote> PclDiff::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclDiff::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_PCL_DIFF_USE)
    return fail(HS_ERR_OPERATOR, "未安装 PCL", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "没有源点云", out);
    }
    const HsCommData::Cloud3* tmpl = nullptr;
    for (size_t i = 0; i < in.clouds.size(); ++i) {
        if (in.clouds[i].port == "template") { tmpl = &in.clouds[i].cloud; break; }
    }
    if (!tmpl || tmpl->isEmpty()) {
        if (in.clouds.size() >= 2 && !in.clouds[1].cloud.isEmpty()) { tmpl = &in.clouds[1].cloud; }
    }
    if (!tmpl || tmpl->isEmpty()) {
        return fail(HS_ERR_OPERATOR, "没有模板点云", out);
    }
    double threshold = in.run.value("threshold").toDouble(1.0);
    if (threshold < 0.0) { threshold = 0.0; }
    pcl::PointCloud<pcl::PointXYZ>::Ptr tgt(new pcl::PointCloud<pcl::PointXYZ>());
    tgt->resize(tmpl->xyz.size());
    for (size_t i = 0; i < tmpl->xyz.size(); ++i) {
        (*tgt)[i].x = static_cast<float>(tmpl->xyz[i].x);
        (*tgt)[i].y = static_cast<float>(tmpl->xyz[i].y);
        (*tgt)[i].z = static_cast<float>(tmpl->xyz[i].z);
    }
    pcl::KdTreeFLANN<pcl::PointXYZ> kdtree;
    kdtree.setInputCloud(tgt);
    HsCommData::Cloud3 defect;
    const HsCommData::Cloud3& src = in.clouds[0].cloud;
    std::vector<int> nn(1);
    std::vector<float> dist2(1);
    for (size_t i = 0; i < src.xyz.size(); ++i) {
        pcl::PointXYZ q;
        q.x = static_cast<float>(src.xyz[i].x);
        q.y = static_cast<float>(src.xyz[i].y);
        q.z = static_cast<float>(src.xyz[i].z);
        if (kdtree.nearestKSearch(q, 1, nn, dist2) <= 0) { continue; }
        if (std::sqrt(dist2[0]) > threshold) {
            defect.xyz.push_back(src.xyz[i]);
        }
    }
    PortCloud pc; pc.cloud = defect; out->clouds.push_back(pc);
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("points", static_cast<int>(defect.xyz.size()));
    PortTuple tuple; HsCommData::HsTuple values;
    values.append(static_cast<double>(defect.xyz.size()));
    tuple.value = values; out->tuples.push_back(tuple);
    char msg[64]; sprintf_s(msg, "差分点 %d", static_cast<int>(defect.xyz.size()));
    out->message = msg;
    return HS_OK;
#endif
}
}