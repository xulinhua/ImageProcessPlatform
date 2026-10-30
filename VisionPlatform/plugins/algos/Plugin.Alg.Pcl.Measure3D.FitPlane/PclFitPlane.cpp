/**
 * @file PclFitPlane.cpp
 * @brief PCL 拟合平面
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclFitPlane.h"
#include "VisionVersion.h"
#include <HsCommData/Geom3.h>
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <random>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<pcl/segmentation/sac_segmentation.h>) && __has_include(<pcl/point_types.h>) && __has_include(<pcl/point_cloud.h>)
#    define VISION_FIT_USE_PCL 1
#    include <pcl/point_cloud.h>
#    include <pcl/point_types.h>
#    include <pcl/segmentation/sac_segmentation.h>
#  endif
#endif

namespace Vision {

namespace {

const VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "RANSAC 拟合平面" },
};

}

std::vector<VerNote> PclFitPlane::versionNotes() const
{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

HsCommData::Point3d PclFitPlane::orientNormal(const HsCommData::Point3d& n) const
{
    if (n.z < 0.0 || (n.z == 0.0 && n.y < 0.0) || (n.z == 0.0 && n.y == 0.0 && n.x < 0.0)) {
        return HsCommData::Point3d(-n.x, -n.y, -n.z);
    }
    return n;
}

bool PclFitPlane::fitRansac(const HsCommData::Cloud3& cloud, double distance,
                            HsCommData::Plane3* plane, std::vector<int>* inliers, std::string* err) const
{
    const int n = static_cast<int>(cloud.xyz.size());
    if (n < 3 || !plane || !inliers) {
        if (err) {
            *err = "点太少或退化";
        }
        return false;
    }
    // 固定种子随机取三点建面，迭代 1000 次，留内点最多的一组
    std::mt19937 rng(1);
    std::uniform_int_distribution<int> pick(0, n - 1);
    int bestCount = 0;
    HsCommData::Point3d bestN;
    HsCommData::Point3d bestP;
    for (int iter = 0; iter < 1000; ++iter) {
        const int ia = pick(rng);
        const int ib = pick(rng);
        const int ic = pick(rng);
        if (ia == ib || ib == ic || ia == ic) {
            continue;
        }
        const HsCommData::Point3d& a = cloud.xyz[static_cast<size_t>(ia)];
        const HsCommData::Point3d& b = cloud.xyz[static_cast<size_t>(ib)];
        const HsCommData::Point3d& c = cloud.xyz[static_cast<size_t>(ic)];
        const HsCommData::Point3d cr = HsCommData::cross(b - a, c - a);
        if (HsCommData::length(cr) < 1e-8) {
            continue;
        }
        const HsCommData::Point3d normal = orientNormal(HsCommData::normalized(cr));
        int count = 0;
        for (int i = 0; i < n; ++i) {
            const double d = std::fabs(HsCommData::dot(normal, cloud.xyz[static_cast<size_t>(i)] - a));
            if (d <= distance) {
                ++count;
            }
        }
        if (count > bestCount) {
            bestCount = count;
            bestN = normal;
            bestP = a;
        }
    }
    if (bestCount < 3) {
        if (err) {
            *err = "点太少或退化";
        }
        return false;
    }
    // 按最佳平面重取内点，原点取内点质心在平面上的投影
    inliers->clear();
    HsCommData::Point3d sum;
    for (int i = 0; i < n; ++i) {
        const HsCommData::Point3d& p = cloud.xyz[static_cast<size_t>(i)];
        if (std::fabs(HsCommData::dot(bestN, p - bestP)) <= distance) {
            inliers->push_back(i);
            sum += p;
        }
    }
    const double inv = 1.0 / static_cast<double>(inliers->size());
    const HsCommData::Point3d centroid = sum * inv;
    const double shift = HsCommData::dot(bestN, centroid - bestP);
    plane->normal = bestN;
    plane->origin = centroid - bestN * shift;
    return plane->isNormalUnit();
}

bool PclFitPlane::fit(const HsCommData::Cloud3& cloud, double distance,
                      HsCommData::Plane3* plane, std::vector<int>* inliers, std::string* err) const
{
#if defined(VISION_FIT_USE_PCL)
    const int n = static_cast<int>(cloud.xyz.size());
    if (n < 3 || !plane || !inliers) {
        if (err) {
            *err = "点太少或退化";
        }
        return false;
    }
    // 转成 PCL 点云，用 SACSegmentation 做 RANSAC；固定种子保证结果可复现
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts(new pcl::PointCloud<pcl::PointXYZ>());
    pts->resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const HsCommData::Point3d& p = cloud.xyz[static_cast<size_t>(i)];
        (*pts)[static_cast<size_t>(i)].x = static_cast<float>(p.x);
        (*pts)[static_cast<size_t>(i)].y = static_cast<float>(p.y);
        (*pts)[static_cast<size_t>(i)].z = static_cast<float>(p.z);
    }
    pcl::SACSegmentation<pcl::PointXYZ> seg;
    seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_PLANE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(static_cast<double>(distance));
    seg.setMaxIterations(1000);
    seg.setSeed(1);
    seg.setInputCloud(pts);
    pcl::PointIndices idx;
    pcl::ModelCoefficients coeff;
    seg.segment(idx, coeff);
    if (coeff.values.size() < 4 || idx.indices.size() < 3) {
        if (err) {
            *err = "点太少或退化";
        }
        return false;
    }
    // ax+by+cz+d=0 归一化成单位法向并定号；法向翻转时 d 同步取反，原点取平面上离坐标原点最近的点
    const double a = coeff.values[0];
    const double b = coeff.values[1];
    const double c = coeff.values[2];
    double d = coeff.values[3];
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
    inliers->reserve(idx.indices.size());
    for (size_t i = 0; i < idx.indices.size(); ++i) {
        inliers->push_back(idx.indices[i]);
    }
    return true;
#else
    return fitRansac(cloud, distance, plane, inliers, err);
#endif
}

void PclFitPlane::publish(const AlgToolIn& in, const HsCommData::Cloud3& cloud,
                          const HsCommData::Plane3& plane, const std::vector<int>& inliers, AlgToolOut* out) const
{
    // 内点到平面的有符号距离求 RMS，同时收集内点云
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

    // 平面图形、inlier 端口内点云，以及与测值同序的元组
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

    // 判定：启用且为数值的测值越界则 NG，errCode 仍为 HS_OK
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

int PclFitPlane::execute(const AlgToolIn& in, AlgToolOut* out)
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
