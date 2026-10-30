/**
 * @file Open3dFitPlane.cpp
 * @brief Open3D 拟合平面
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Open3dFitPlane.h"
#include "VisionVersion.h"
#include <HsCommData/Geom3.h>
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<open3d/geometry/PointCloud.h>)
#    define VISION_FIT_USE_OPEN3D 1
#    include <open3d/geometry/PointCloud.h>
#  endif
#endif

namespace Vision {

namespace {

const VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Open3D 拟合平面" },
};

}

std::vector<VerNote> Open3dFitPlane::versionNotes() const
{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

HsCommData::Point3d Open3dFitPlane::orientNormal(const HsCommData::Point3d& n) const
{
    if (n.z < 0.0 || (n.z == 0.0 && n.y < 0.0) || (n.z == 0.0 && n.y == 0.0 && n.x < 0.0)) {
        return HsCommData::Point3d(-n.x, -n.y, -n.z);
    }
    return n;
}

bool Open3dFitPlane::fit(const HsCommData::Cloud3& cloud, double distance,
                         HsCommData::Plane3* plane, std::vector<int>* inliers, std::string* err) const
{
#if defined(VISION_FIT_USE_OPEN3D)
    const int n = static_cast<int>(cloud.xyz.size());
    if (n < 3 || !plane || !inliers) {
        if (err) {
            *err = "点太少或退化";
        }
        return false;
    }
    open3d::geometry::PointCloud pcd;
    pcd.points_.resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const HsCommData::Point3d& p = cloud.xyz[static_cast<size_t>(i)];
        pcd.points_[static_cast<size_t>(i)] = Eigen::Vector3d(p.x, p.y, p.z);
    }
    const auto result = pcd.SegmentPlane(distance, 3, 1000);
    const Eigen::Vector4d& model = std::get<0>(result);
    const std::vector<size_t>& idx = std::get<1>(result);
    if (idx.size() < 3) {
        if (err) {
            *err = "点太少或退化";
        }
        return false;
    }
    const double a = model(0);
    const double b = model(1);
    const double c = model(2);
    double d = model(3);
    const double nn = std::sqrt(a * a + b * b + c * c);
    if (nn < 1e-12) {
        if (err) {
            *err = "点太少或退化";
        }
        return false;
    }
    HsCommData::Point3d normal(a / nn, b / nn, c / nn);
    d /= nn;
    const HsCommData::Point3d oriented = orientNormal(normal);
    if (oriented.x != normal.x || oriented.y != normal.y || oriented.z != normal.z) {
        d = -d;
    }
    plane->normal = oriented;
    plane->origin = oriented * (-d);
    inliers->clear();
    inliers->reserve(idx.size());
    for (size_t i = 0; i < idx.size(); ++i) {
        inliers->push_back(static_cast<int>(idx[i]));
    }
    return true;
#else
    (void)cloud;
    (void)distance;
    (void)plane;
    (void)inliers;
    if (err) {
        *err = "未安装 Open3D";
    }
    return false;
#endif
}

void Open3dFitPlane::publish(const AlgToolIn& in, const HsCommData::Cloud3& cloud,
                             const HsCommData::Plane3& plane, const std::vector<int>& inliers, AlgToolOut* out) const
{
    double sumSq = 0;
    HsCommData::Cloud3 kept;
    kept.xyz.reserve(inliers.size());
    for (size_t i = 0; i < inliers.size(); ++i) {
        const HsCommData::Point3d& p = cloud.xyz[static_cast<size_t>(inliers[i])];
        const double dist = plane.signedDistance(p);
        sumSq += dist * dist;
        kept.xyz.push_back(p);
    }
    const double rms = inliers.empty() ? 0.0 : std::sqrt(sumSq / static_cast<double>(inliers.size()));
    out->ok = true;
    out->errCode = HS_OK;
    out->values.insert("nx", plane.normal.x);
    out->values.insert("ny", plane.normal.y);
    out->values.insert("nz", plane.normal.z);
    out->values.insert("ox", plane.origin.x);
    out->values.insert("oy", plane.origin.y);
    out->values.insert("oz", plane.origin.z);
    out->values.insert("rms", rms);
    out->values.insert("inliers", static_cast<int>(inliers.size()));
    out->values.insert("points", static_cast<int>(cloud.xyz.size()));

    PortShape3 shape;
    shape.kind = Shape3Kind::Plane;
    shape.plane = plane;
    out->shapes3.push_back(shape);

    PortCloud portCloud;
    portCloud.port = "inlier";
    portCloud.cloud = kept;
    out->clouds.push_back(portCloud);

    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(plane.normal.x);
    values.append(plane.normal.y);
    values.append(plane.normal.z);
    values.append(plane.origin.x);
    values.append(plane.origin.y);
    values.append(plane.origin.z);
    values.append(rms);
    values.append(static_cast<double>(inliers.size()));
    values.append(static_cast<double>(cloud.xyz.size()));
    tuple.value = values;
    out->tuples.push_back(tuple);

    char msg[128];
    sprintf_s(msg, "rms %.4f  内点 %d", rms, static_cast<int>(inliers.size()));
    out->message = msg;

    for (size_t i = 0; i < in.judge.size(); ++i) {
        const JudgeParam& row = in.judge[i];
        if (!row.enable || !out->values.contains(row.name)) {
            continue;
        }
        const ParamValue got = out->values.value(row.name);
        if (got.kind() != ParamValue::KindInt && got.kind() != ParamValue::KindDouble) {
            continue;
        }
        const double value = got.toDouble(0);
        if (value < row.minVal || value > row.maxVal) {
            out->ok = false;
            out->message = row.name + " 超出范围";
        }
    }
}

int Open3dFitPlane::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) {
        return HS_ERR_NULL;
    }
    *out = AlgToolOut();
    if (in.clouds.empty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    double distance = in.run.value("distance").toDouble(0.5);
    if (distance < 0.0) {
        distance = 0.0;
    }
    HsCommData::Plane3 plane;
    std::vector<int> inliers;
    std::string err;
    if (!fit(in.clouds[0].cloud, distance, &plane, &inliers, &err)) {
        return fail(HS_ERR_OPERATOR, err.empty() ? "点太少或退化" : err, out);
    }
    publish(in, in.clouds[0].cloud, plane, inliers, out);
    return HS_OK;
}

}
