/**
 * @file Open3dCloudOps.h
 * @brief HsCloud 与 Open3D 点云互转、位姿与矩阵互转、PCD/PLY 读写
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPEN3D_CLOUD_OPS_H
#define VISION_OPEN3D_CLOUD_OPS_H

#include <HsCommData/Geom3.h>
#include <HsCommData/HsCloud.h>
#include <HsCommData/HsImage.h>
#include <string>

#if defined(VISION_HAS_OPEN3D)
#  include <open3d/geometry/PointCloud.h>
#  include <Eigen/Core>
#endif

namespace Vision {

/**
 * @class Open3dCloudOps
 * @brief 点云与位姿换算、深度图互转。全部为成员，无自由函数
 */
class Open3dCloudOps {
public:
    /** @brief 构造。无状态 */
    Open3dCloudOps() {}

#if defined(VISION_HAS_OPEN3D)
    /** @brief HsCloud → Open3D PointCloud */
    open3d::geometry::PointCloud to_cloud(const HsCommData::Cloud3& cloud) const;
    /** @brief Open3D PointCloud → HsCloud */
    HsCommData::Cloud3 from_cloud(const open3d::geometry::PointCloud& cloud) const;
    /** @brief 4x4 齐次矩阵 → Pose3（平移 + 四元数） */
    HsCommData::Pose3 matrix_to_pose(const Eigen::Matrix4d& t) const;
    /** @brief Pose3 → 4x4 齐次矩阵 */
    Eigen::Matrix4d pose_to_matrix(const HsCommData::Pose3& pose) const;
#endif

    /**
     * @brief 深度图按针孔模型转点云
     * @param depth 深度图。优先 mono real32/uint16/byte，Z=像素*scale
     * @param fx 焦距 x（像素）
     * @param fy 焦距 y（像素）
     * @param cx 主点 x
     * @param cy 主点 y
     * @param scale 深度倍率
     */
    HsCommData::Cloud3 depth_to_cloud(const HsCommData::HsImage& depth,
                                      double fx, double fy, double cx, double cy, double scale) const;
    /**
     * @brief 有序点云 Z 当深度，再按针孔反投影
     * @note 点云须 isOrganized；xyz.z 视为深度
     */
    HsCommData::Cloud3 organized_depth_to_cloud(const HsCommData::Cloud3& depthCloud,
                                                double fx, double fy, double cx, double cy, double scale) const;
    /** @brief 点云投影成深度图（real32 mono，无效深度为 0） */
    HsCommData::HsImage cloud_to_depth(const HsCommData::Cloud3& cloud,
                                       double fx, double fy, double cx, double cy,
                                       int width, int height) const;

    /** @brief 读点云文件（PCD/PLY），优先走 Open3D IO。失败返回 false */
    bool read_model(const std::string& path, HsCommData::Cloud3* cloud, std::string* err) const;
    /** @brief 写点云文件。失败返回 false */
    bool write_model(const std::string& path, const HsCommData::Cloud3& cloud, std::string* err) const;

private:
    /** @brief 从旋转矩阵提四元数 */
    HsCommData::Pose3 rot_to_pose(double m00, double m01, double m02,
                                  double m10, double m11, double m12,
                                  double m20, double m21, double m22,
                                  double tx, double ty, double tz) const;
    /** @brief 单像素深度值。失败返回 false */
    bool sample_depth(const HsCommData::HsImage& depth, int x, int y, double* z) const;
};

}

#endif
