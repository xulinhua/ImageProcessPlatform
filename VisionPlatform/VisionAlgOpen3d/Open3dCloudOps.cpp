/**
 * @file Open3dCloudOps.cpp
 * @brief HsCloud 与 Open3D 点云互转、位姿与矩阵互转
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Open3dCloudOps.h"
#include <cmath>
#include <cstdio>
#include <fstream>

#if defined(VISION_HAS_OPEN3D)
#  include <open3d/io/PointCloudIO.h>
#endif

namespace Vision {

HsCommData::Pose3 Open3dCloudOps::rot_to_pose(double m00, double m01, double m02,
                                              double m10, double m11, double m12,
                                              double m20, double m21, double m22,
                                              double tx, double ty, double tz) const
{
    HsCommData::Pose3 pose;
    pose.tx = tx;
    pose.ty = ty;
    pose.tz = tz;
    const double tr = m00 + m11 + m22;
    if (tr > 0) {
        const double s = std::sqrt(tr + 1.0) * 2.0;
        pose.qw = 0.25 * s;
        pose.qx = (m21 - m12) / s;
        pose.qy = (m02 - m20) / s;
        pose.qz = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const double s = std::sqrt(1.0 + m00 - m11 - m22) * 2.0;
        pose.qw = (m21 - m12) / s;
        pose.qx = 0.25 * s;
        pose.qy = (m01 + m10) / s;
        pose.qz = (m02 + m20) / s;
    } else if (m11 > m22) {
        const double s = std::sqrt(1.0 + m11 - m00 - m22) * 2.0;
        pose.qw = (m02 - m20) / s;
        pose.qx = (m01 + m10) / s;
        pose.qy = 0.25 * s;
        pose.qz = (m12 + m21) / s;
    } else {
        const double s = std::sqrt(1.0 + m22 - m00 - m11) * 2.0;
        pose.qw = (m10 - m01) / s;
        pose.qx = (m02 + m20) / s;
        pose.qy = (m12 + m21) / s;
        pose.qz = 0.25 * s;
    }
    return pose.normalized();
}

#if defined(VISION_HAS_OPEN3D)

open3d::geometry::PointCloud Open3dCloudOps::to_cloud(const HsCommData::Cloud3& cloud) const
{
    open3d::geometry::PointCloud out;
    out.points_.resize(cloud.xyz.size());
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        out.points_[i] = Eigen::Vector3d(cloud.xyz[i].x, cloud.xyz[i].y, cloud.xyz[i].z);
    }
    if (!cloud.normals.empty() && cloud.normals.size() == cloud.xyz.size()) {
        out.normals_.resize(cloud.normals.size());
        for (size_t i = 0; i < cloud.normals.size(); ++i) {
            out.normals_[i] = Eigen::Vector3d(cloud.normals[i].x, cloud.normals[i].y, cloud.normals[i].z);
        }
    }
    if (!cloud.rgb.empty() && cloud.rgb.size() == cloud.xyz.size()) {
        out.colors_.resize(cloud.rgb.size());
        for (size_t i = 0; i < cloud.rgb.size(); ++i) {
            out.colors_[i] = Eigen::Vector3d(cloud.rgb[i].r / 255.0, cloud.rgb[i].g / 255.0, cloud.rgb[i].b / 255.0);
        }
    }
    return out;
}

HsCommData::Cloud3 Open3dCloudOps::from_cloud(const open3d::geometry::PointCloud& cloud) const
{
    HsCommData::Cloud3 out;
    out.xyz.resize(cloud.points_.size());
    for (size_t i = 0; i < cloud.points_.size(); ++i) {
        out.xyz[i] = HsCommData::Point3d(cloud.points_[i].x(), cloud.points_[i].y(), cloud.points_[i].z());
    }
    if (cloud.HasNormals()) {
        out.normals.resize(cloud.normals_.size());
        for (size_t i = 0; i < cloud.normals_.size(); ++i) {
            out.normals[i] = HsCommData::Point3d(cloud.normals_[i].x(), cloud.normals_[i].y(), cloud.normals_[i].z());
        }
    }
    if (cloud.HasColors()) {
        out.rgb.resize(cloud.colors_.size());
        for (size_t i = 0; i < cloud.colors_.size(); ++i) {
            out.rgb[i] = HsCommData::Rgb8(
                static_cast<unsigned char>(cloud.colors_[i].x() * 255.0),
                static_cast<unsigned char>(cloud.colors_[i].y() * 255.0),
                static_cast<unsigned char>(cloud.colors_[i].z() * 255.0));
        }
    }
    return out;
}

HsCommData::Pose3 Open3dCloudOps::matrix_to_pose(const Eigen::Matrix4d& t) const
{
    return rot_to_pose(t(0, 0), t(0, 1), t(0, 2),
                       t(1, 0), t(1, 1), t(1, 2),
                       t(2, 0), t(2, 1), t(2, 2),
                       t(0, 3), t(1, 3), t(2, 3));
}

Eigen::Matrix4d Open3dCloudOps::pose_to_matrix(const HsCommData::Pose3& pose) const
{
    const HsCommData::Pose3 u = pose.normalized();
    const double x = u.qx, y = u.qy, z = u.qz, w = u.qw;
    Eigen::Matrix4d t = Eigen::Matrix4d::Identity();
    t(0, 0) = 1 - 2 * (y * y + z * z);
    t(0, 1) = 2 * (x * y - z * w);
    t(0, 2) = 2 * (x * z + y * w);
    t(1, 0) = 2 * (x * y + z * w);
    t(1, 1) = 1 - 2 * (x * x + z * z);
    t(1, 2) = 2 * (y * z - x * w);
    t(2, 0) = 2 * (x * z - y * w);
    t(2, 1) = 2 * (y * z + x * w);
    t(2, 2) = 1 - 2 * (x * x + y * y);
    t(0, 3) = u.tx;
    t(1, 3) = u.ty;
    t(2, 3) = u.tz;
    return t;
}

#endif

bool Open3dCloudOps::sample_depth(const HsCommData::HsImage& depth, int x, int y, double* z) const
{
    if (z == nullptr || depth.isNull() || x < 0 || y < 0 || x >= depth.width || y >= depth.height) {
        return false;
    }
    double gray = 0.0;
    if (!depth.sampleGray(x, y, &gray)) {
        return false;
    }
    *z = gray;
    return true;
}

HsCommData::Cloud3 Open3dCloudOps::depth_to_cloud(const HsCommData::HsImage& depth,
                                                  double fx, double fy, double cx, double cy, double scale) const
{
    HsCommData::Cloud3 out;
    if (depth.isNull() || fx <= 0.0 || fy <= 0.0) {
        return out;
    }
    const double s = (scale == 0.0) ? 1.0 : scale;
    out.xyz.reserve(static_cast<size_t>(depth.width) * static_cast<size_t>(depth.height));
    for (int v = 0; v < depth.height; ++v) {
        for (int u = 0; u < depth.width; ++u) {
            double zRaw = 0.0;
            if (!sample_depth(depth, u, v, &zRaw)) {
                continue;
            }
            const double z = zRaw * s;
            if (z <= 0.0) {
                continue;
            }
            out.xyz.push_back(HsCommData::Point3d(
                (static_cast<double>(u) - cx) * z / fx,
                (static_cast<double>(v) - cy) * z / fy,
                z));
        }
    }
    return out;
}

HsCommData::Cloud3 Open3dCloudOps::organized_depth_to_cloud(const HsCommData::Cloud3& depthCloud,
                                                            double fx, double fy, double cx, double cy, double scale) const
{
    HsCommData::Cloud3 out;
    if (!depthCloud.isOrganized() || fx <= 0.0 || fy <= 0.0) {
        return out;
    }
    const double s = (scale == 0.0) ? 1.0 : scale;
    out.xyz.reserve(depthCloud.xyz.size());
    for (int v = 0; v < depthCloud.height; ++v) {
        for (int u = 0; u < depthCloud.width; ++u) {
            const size_t i = static_cast<size_t>(v) * static_cast<size_t>(depthCloud.width) + static_cast<size_t>(u);
            if (i >= depthCloud.xyz.size()) {
                continue;
            }
            const double z = depthCloud.xyz[i].z * s;
            if (z <= 0.0) {
                continue;
            }
            out.xyz.push_back(HsCommData::Point3d(
                (static_cast<double>(u) - cx) * z / fx,
                (static_cast<double>(v) - cy) * z / fy,
                z));
        }
    }
    return out;
}

HsCommData::HsImage Open3dCloudOps::cloud_to_depth(const HsCommData::Cloud3& cloud,
                                                   double fx, double fy, double cx, double cy,
                                                   int width, int height) const
{
    if (width <= 0 || height <= 0 || fx <= 0.0 || fy <= 0.0) {
        return HsCommData::HsImage();
    }
    HsCommData::HsImage depth = HsCommData::HsImage::make(width, height, HsCommData::PixelReal32, HsCommData::EncodeMono, 0);
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        const double z = cloud.xyz[i].z;
        if (z <= 1.0e-9) {
            continue;
        }
        const int u = static_cast<int>(std::lround(fx * cloud.xyz[i].x / z + cx));
        const int v = static_cast<int>(std::lround(fy * cloud.xyz[i].y / z + cy));
        if (u < 0 || v < 0 || u >= width || v >= height) {
            continue;
        }
        float* row = reinterpret_cast<float*>(depth.scanLine(v));
        const float old = row[u];
        if (old <= 0.0f || static_cast<float>(z) < old) {
            row[u] = static_cast<float>(z);
        }
    }
    return depth;
}

bool Open3dCloudOps::read_model(const std::string& path, HsCommData::Cloud3* cloud, std::string* err) const
{
    if (cloud == nullptr || path.empty()) {
        if (err) {
            *err = "路径或输出为空";
        }
        return false;
    }
#if defined(VISION_HAS_OPEN3D)
    open3d::geometry::PointCloud pc;
    if (!open3d::io::ReadPointCloud(path, pc)) {
        if (err) {
            *err = "Open3D 读点云失败";
        }
        return false;
    }
    *cloud = from_cloud(pc);
    return !cloud->isEmpty();
#else
    (void)path;
    if (err) {
        *err = "未安装 Open3D";
    }
    return false;
#endif
}

bool Open3dCloudOps::write_model(const std::string& path, const HsCommData::Cloud3& cloud, std::string* err) const
{
    if (path.empty()) {
        if (err) {
            *err = "路径为空";
        }
        return false;
    }
#if defined(VISION_HAS_OPEN3D)
    const open3d::geometry::PointCloud pc = to_cloud(cloud);
    if (!open3d::io::WritePointCloud(path, pc)) {
        if (err) {
            *err = "Open3D 写点云失败";
        }
        return false;
    }
    return true;
#else
    (void)cloud;
    if (err) {
        *err = "未安装 Open3D";
    }
    return false;
#endif
}

}
