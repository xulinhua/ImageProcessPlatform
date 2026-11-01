/**
 * @file PclCloudOps.h
 * @brief HsCloud 与 PCL 点云互转、位姿与矩阵互转、PCD 读写
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PCL_CLOUD_OPS_H
#define VISION_PCL_CLOUD_OPS_H

#include <HsCommData/Geom3.h>
#include <HsCommData/HsCloud.h>
#include <HsCommData/HsImage.h>
#include <string>

#if defined(VISION_HAS_PCL)
#  include <pcl/point_cloud.h>
#  include <pcl/point_types.h>
#  include <Eigen/Core>
#endif

namespace Vision {

/**
 * @class PclCloudOps
 * @brief 点云与位姿换算、深度图互转。全部为成员，无自由函数
 */
class PclCloudOps {
public:
    PclCloudOps() {}

#if defined(VISION_HAS_PCL)
    /** @brief HsCloud → PCL XYZ。空点云得到空 Ptr */
    pcl::PointCloud<pcl::PointXYZ>::Ptr to_xyz(const HsCommData::Cloud3& cloud) const;
    /** @brief PCL XYZ → HsCloud。保留宽高 */
    HsCommData::Cloud3 from_xyz(const pcl::PointCloud<pcl::PointXYZ>& cloud) const;
    /** @brief 4x4 齐次矩阵 → Pose3（平移 + 四元数） */
    HsCommData::Pose3 matrix_to_pose(const Eigen::Matrix4f& t) const;
    /** @brief Pose3 → 4x4 齐次矩阵 */
    Eigen::Matrix4f pose_to_matrix(const HsCommData::Pose3& pose) const;
#endif

    /**
     * @brief 深度图按针孔模型转点云
     * @param depth 深度图。优先 mono real32/uint16/byte，Z=像素*scale
     * @param fx 焦距 x（像素）
     * @param fy 焦距 y（像素）
     * @param cx 主点 x
     * @param cy 主点 y
     * @param scale 深度倍率，毫米/像素单位
     */
    HsCommData::Cloud3 depth_to_cloud(const HsCommData::HsImage& depth,
                                      double fx, double fy, double cx, double cy, double scale) const;
    /**
     * @brief 有序点云按 Z 当深度，再按针孔反投影
     * @note 点云须 isOrganized；xyz.z 视为深度
     */
    HsCommData::Cloud3 organized_depth_to_cloud(const HsCommData::Cloud3& depthCloud,
                                                double fx, double fy, double cx, double cy, double scale) const;
    /**
     * @brief 点云投影成深度图（real32 mono，无效深度为 0）
     */
    HsCommData::HsImage cloud_to_depth(const HsCommData::Cloud3& cloud,
                                       double fx, double fy, double cx, double cy,
                                       int width, int height) const;

    /** @brief 读 ASCII/二进制 PCD 到 HsCloud。失败返回 false */
    bool read_pcd(const std::string& path, HsCommData::Cloud3* cloud, std::string* err) const;
    /** @brief 写 ASCII PCD。失败返回 false */
    bool write_pcd(const std::string& path, const HsCommData::Cloud3& cloud, std::string* err) const;
    /** @brief 读 PLY（仅 ascii xyz）。失败返回 false */
    bool read_ply(const std::string& path, HsCommData::Cloud3* cloud, std::string* err) const;
    /** @brief 写 ASCII PLY */
    bool write_ply(const std::string& path, const HsCommData::Cloud3& cloud, std::string* err) const;

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
