/**
 * @file PclCloudOps.cpp
 * @brief HsCloud 与 PCL 点云互转、位姿与矩阵互转、PCD 读写
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclCloudOps.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace Vision {

HsCommData::Pose3 PclCloudOps::rot_to_pose(double m00, double m01, double m02,
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

#if defined(VISION_HAS_PCL)

pcl::PointCloud<pcl::PointXYZ>::Ptr PclCloudOps::to_xyz(const HsCommData::Cloud3& cloud) const
{
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts(new pcl::PointCloud<pcl::PointXYZ>());
    pts->resize(cloud.xyz.size());
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        (*pts)[i].x = static_cast<float>(cloud.xyz[i].x);
        (*pts)[i].y = static_cast<float>(cloud.xyz[i].y);
        (*pts)[i].z = static_cast<float>(cloud.xyz[i].z);
    }
    if (cloud.isOrganized()) {
        pts->width = static_cast<uint32_t>(cloud.width);
        pts->height = static_cast<uint32_t>(cloud.height);
        pts->is_dense = false;
    } else {
        pts->width = static_cast<uint32_t>(cloud.xyz.size());
        pts->height = 1;
        pts->is_dense = true;
    }
    return pts;
}

HsCommData::Cloud3 PclCloudOps::from_xyz(const pcl::PointCloud<pcl::PointXYZ>& cloud) const
{
    HsCommData::Cloud3 out;
    out.xyz.resize(cloud.size());
    for (size_t i = 0; i < cloud.size(); ++i) {
        out.xyz[i] = HsCommData::Point3d(cloud[i].x, cloud[i].y, cloud[i].z);
    }
    if (cloud.height > 1) {
        out.width = static_cast<int>(cloud.width);
        out.height = static_cast<int>(cloud.height);
    }
    return out;
}

HsCommData::Pose3 PclCloudOps::matrix_to_pose(const Eigen::Matrix4f& t) const
{
    return rot_to_pose(t(0, 0), t(0, 1), t(0, 2),
                       t(1, 0), t(1, 1), t(1, 2),
                       t(2, 0), t(2, 1), t(2, 2),
                       t(0, 3), t(1, 3), t(2, 3));
}

Eigen::Matrix4f PclCloudOps::pose_to_matrix(const HsCommData::Pose3& pose) const
{
    const HsCommData::Pose3 u = pose.normalized();
    const double x = u.qx, y = u.qy, z = u.qz, w = u.qw;
    Eigen::Matrix4f t = Eigen::Matrix4f::Identity();
    t(0, 0) = static_cast<float>(1 - 2 * (y * y + z * z));
    t(0, 1) = static_cast<float>(2 * (x * y - z * w));
    t(0, 2) = static_cast<float>(2 * (x * z + y * w));
    t(1, 0) = static_cast<float>(2 * (x * y + z * w));
    t(1, 1) = static_cast<float>(1 - 2 * (x * x + z * z));
    t(1, 2) = static_cast<float>(2 * (y * z - x * w));
    t(2, 0) = static_cast<float>(2 * (x * z - y * w));
    t(2, 1) = static_cast<float>(2 * (y * z + x * w));
    t(2, 2) = static_cast<float>(1 - 2 * (x * x + y * y));
    t(0, 3) = static_cast<float>(u.tx);
    t(1, 3) = static_cast<float>(u.ty);
    t(2, 3) = static_cast<float>(u.tz);
    return t;
}

#endif

bool PclCloudOps::sample_depth(const HsCommData::HsImage& depth, int x, int y, double* z) const
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

HsCommData::Cloud3 PclCloudOps::depth_to_cloud(const HsCommData::HsImage& depth,
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
            const double x = (static_cast<double>(u) - cx) * z / fx;
            const double y = (static_cast<double>(v) - cy) * z / fy;
            out.xyz.push_back(HsCommData::Point3d(x, y, z));
        }
    }
    return out;
}

HsCommData::Cloud3 PclCloudOps::organized_depth_to_cloud(const HsCommData::Cloud3& depthCloud,
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
            const double x = (static_cast<double>(u) - cx) * z / fx;
            const double y = (static_cast<double>(v) - cy) * z / fy;
            out.xyz.push_back(HsCommData::Point3d(x, y, z));
        }
    }
    return out;
}

HsCommData::HsImage PclCloudOps::cloud_to_depth(const HsCommData::Cloud3& cloud,
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

bool PclCloudOps::read_pcd(const std::string& path, HsCommData::Cloud3* cloud, std::string* err) const
{
    if (cloud == nullptr || path.empty()) {
        if (err) {
            *err = "路径或输出为空";
        }
        return false;
    }
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        if (err) {
            *err = "无法打开 PCD";
        }
        return false;
    }
    std::string line;
    size_t points = 0;
    bool dataAscii = true;
    while (std::getline(in, line)) {
        if (line.rfind("POINTS", 0) == 0) {
            std::sscanf(line.c_str(), "POINTS %zu", &points);
        } else if (line.rfind("DATA", 0) == 0) {
            dataAscii = line.find("ascii") != std::string::npos;
            break;
        }
    }
    cloud->xyz.clear();
    cloud->xyz.reserve(points);
    if (!dataAscii) {
        if (err) {
            *err = "仅支持 ASCII PCD";
        }
        return false;
    }
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        double x = 0, y = 0, z = 0;
        if (std::sscanf(line.c_str(), "%lf %lf %lf", &x, &y, &z) >= 3) {
            cloud->xyz.push_back(HsCommData::Point3d(x, y, z));
        }
    }
    return !cloud->xyz.empty();
}

bool PclCloudOps::write_pcd(const std::string& path, const HsCommData::Cloud3& cloud, std::string* err) const
{
    if (path.empty()) {
        if (err) {
            *err = "路径为空";
        }
        return false;
    }
    FILE* fp = nullptr;
    if (fopen_s(&fp, path.c_str(), "wb") != 0 || fp == nullptr) {
        if (err) {
            *err = "无法写 PCD";
        }
        return false;
    }
    std::fprintf(fp, "# .PCD v0.7 - Point Cloud Data file format\n");
    std::fprintf(fp, "VERSION 0.7\n");
    std::fprintf(fp, "FIELDS x y z\n");
    std::fprintf(fp, "SIZE 4 4 4\n");
    std::fprintf(fp, "TYPE F F F\n");
    std::fprintf(fp, "COUNT 1 1 1\n");
    std::fprintf(fp, "WIDTH %zu\n", cloud.xyz.size());
    std::fprintf(fp, "HEIGHT 1\n");
    std::fprintf(fp, "VIEWPOINT 0 0 0 1 0 0 0\n");
    std::fprintf(fp, "POINTS %zu\n", cloud.xyz.size());
    std::fprintf(fp, "DATA ascii\n");
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        std::fprintf(fp, "%.9g %.9g %.9g\n", cloud.xyz[i].x, cloud.xyz[i].y, cloud.xyz[i].z);
    }
    std::fclose(fp);
    return true;
}

bool PclCloudOps::read_ply(const std::string& path, HsCommData::Cloud3* cloud, std::string* err) const
{
    if (cloud == nullptr || path.empty()) {
        if (err) {
            *err = "路径或输出为空";
        }
        return false;
    }
    std::ifstream in(path.c_str());
    if (!in) {
        if (err) {
            *err = "无法打开 PLY";
        }
        return false;
    }
    std::string line;
    size_t vertexCount = 0;
    bool headerDone = false;
    while (std::getline(in, line)) {
        if (line.rfind("element vertex", 0) == 0) {
            std::sscanf(line.c_str(), "element vertex %zu", &vertexCount);
        } else if (line.find("end_header") != std::string::npos) {
            headerDone = true;
            break;
        }
    }
    if (!headerDone) {
        if (err) {
            *err = "PLY 头损坏";
        }
        return false;
    }
    cloud->xyz.clear();
    cloud->xyz.reserve(vertexCount);
    while (std::getline(in, line)) {
        double x = 0, y = 0, z = 0;
        if (std::sscanf(line.c_str(), "%lf %lf %lf", &x, &y, &z) >= 3) {
            cloud->xyz.push_back(HsCommData::Point3d(x, y, z));
        }
    }
    return !cloud->xyz.empty();
}

bool PclCloudOps::write_ply(const std::string& path, const HsCommData::Cloud3& cloud, std::string* err) const
{
    if (path.empty()) {
        if (err) {
            *err = "路径为空";
        }
        return false;
    }
    FILE* fp = nullptr;
    if (fopen_s(&fp, path.c_str(), "wb") != 0 || fp == nullptr) {
        if (err) {
            *err = "无法写 PLY";
        }
        return false;
    }
    std::fprintf(fp, "ply\nformat ascii 1.0\n");
    std::fprintf(fp, "element vertex %zu\n", cloud.xyz.size());
    std::fprintf(fp, "property float x\nproperty float y\nproperty float z\n");
    std::fprintf(fp, "end_header\n");
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        std::fprintf(fp, "%.9g %.9g %.9g\n", cloud.xyz[i].x, cloud.xyz[i].y, cloud.xyz[i].z);
    }
    std::fclose(fp);
    return true;
}

}
