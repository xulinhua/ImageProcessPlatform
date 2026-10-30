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
 * @brief 用 PCL RANSAC 拟合平面。本机无 PCL 时执行失败（与二维缺 Hal/CV DLL 同类）
 */
class PclFitPlane : public AlgToolBase {
public:
    std::string toolName() const { return "measure3d.fit_plane"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclFitPlane(); }

private:
    bool fit(const HsCommData::Cloud3& cloud, double distance,
             HsCommData::Plane3* plane, std::vector<int>* inliers, std::string* err) const;
    void publish(const AlgToolIn& in, const HsCommData::Cloud3& cloud,
                 const HsCommData::Plane3& plane, const std::vector<int>& inliers, AlgToolOut* out) const;
    HsCommData::Point3d orientNormal(const HsCommData::Point3d& n) const;
};

}

#endif
