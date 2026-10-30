#include "Open3dDiff.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <stdio.h>
#include <limits>

#if defined(__has_include)
#  if __has_include(<open3d/geometry/PointCloud.h>)
#    define VISION_O3_DIFF_USE 1
#    include <open3d/geometry/PointCloud.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D 点云差分" } };
}
std::vector<VerNote> Open3dDiff::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int Open3dDiff::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_O3_DIFF_USE)
    return fail(HS_ERR_OPERATOR, "未安装 Open3D", out);
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
    // 用 Open3D 点云容器承载模板点，最近邻用手写平方距离（避开 KDTreeFlann 拉入 registration/Feature）
    open3d::geometry::PointCloud target;
    target.points_.resize(tmpl->xyz.size());
    for (size_t i = 0; i < tmpl->xyz.size(); ++i) {
        target.points_[i] = Eigen::Vector3d(tmpl->xyz[i].x, tmpl->xyz[i].y, tmpl->xyz[i].z);
    }
    HsCommData::Cloud3 defect;
    const HsCommData::Cloud3& src = in.clouds[0].cloud;
    const double thr2 = threshold * threshold;
    for (size_t i = 0; i < src.xyz.size(); ++i) {
        const Eigen::Vector3d q(src.xyz[i].x, src.xyz[i].y, src.xyz[i].z);
        double best = std::numeric_limits<double>::infinity();
        for (size_t j = 0; j < target.points_.size(); ++j) {
            const double d2 = (q - target.points_[j]).squaredNorm();
            if (d2 < best) { best = d2; }
        }
        if (best > thr2) {
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