/**
 * @file PclFitPlane.h
 * @brief PCL 拟合平面
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PCLFITPLANE_H
#define VISION_PCLFITPLANE_H

#include "AlgToolBase.h"
#include <HsCommData/HsCloud.h>
#include <HsCommData/Shape3.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class PclFitPlane
 * @brief 用 RANSAC 拟合平面。有 PCL 头文件时走 SACSegmentation，否则用同一套迭代
 */
class PclFitPlane : public AlgToolBase {
public:
    /** @brief 挂到 measure3d.fit_plane 工具壳 */
    std::string toolName() const { return "measure3d.fit_plane"; }
    /** @brief 算法库为 PCL */
    AlgLibKind algLib() const { return AlgLibPcl; }
    /** @brief 算法版本号 */
    std::string version() const { return std::string("1.0.0.0"); }
    /** @brief 版本变更记录 */
    std::vector<VerNote> versionNotes() const;
    /**
     * @brief 对第一个输入点云拟合平面，输出法向、原点、RMS、内点云并按判定项判 OK/NG
     * @note 读 run 里的 distance：内点到平面的距离阈值，单位同点云坐标，缺省 0.5，负数按 0
     * @return 拟合成功返回 HS_OK（判定 NG 也是 HS_OK）；没有点云或点退化返回错误码
     */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    /** @brief 新建一个同类实例 */
    AlgToolBase* clone() const { return new PclFitPlane(); }

private:
    /** @brief 拟合。成功时平面法向已按 z、y、x 定号，inliers 为输入点下标 */
    bool fit(const HsCommData::Cloud3& cloud, double distance,
             HsCommData::Plane3* plane, std::vector<int>* inliers, std::string* err) const;
    /** @brief 无 PCL 时的固定种子 RANSAC */
    bool fitRansac(const HsCommData::Cloud3& cloud, double distance,
                   HsCommData::Plane3* plane, std::vector<int>* inliers, std::string* err) const;
    /** @brief 把平面、内点和测值写入输出。超出判断时 errCode 仍为 0 */
    void publish(const AlgToolIn& in, const HsCommData::Cloud3& cloud,
                 const HsCommData::Plane3& plane, const std::vector<int>& inliers, AlgToolOut* out) const;
    /** @brief 法向单位化后，使 z 非负，其次 y，其次 x */
    HsCommData::Point3d orientNormal(const HsCommData::Point3d& n) const;
};

}

#endif
