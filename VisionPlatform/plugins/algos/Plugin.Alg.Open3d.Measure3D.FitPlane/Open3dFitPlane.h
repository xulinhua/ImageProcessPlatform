/**
 * @file Open3dFitPlane.h
 * @brief Open3D 拟合平面
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPEN3DFITPLANE_H
#define VISION_OPEN3DFITPLANE_H

#include "AlgToolBase.h"
#include <HsCommData/HsCloud.h>
#include <HsCommData/Shape3.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class Open3dFitPlane
 * @brief Open3D SegmentPlane。本机无 Open3D 时执行失败
 */
class Open3dFitPlane : public AlgToolBase {
public:
    std::string toolName() const { return "measure3d.fit_plane"; }
    AlgLibKind algLib() const { return AlgLibOpen3d; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new Open3dFitPlane(); }

private:
    bool fit(const HsCommData::Cloud3& cloud, double distance,
             HsCommData::Plane3* plane, std::vector<int>* inliers, std::string* err) const;
    void publish(const AlgToolIn& in, const HsCommData::Cloud3& cloud,
                 const HsCommData::Plane3& plane, const std::vector<int>& inliers, AlgToolOut* out) const;
    HsCommData::Point3d orientNormal(const HsCommData::Point3d& n) const;
};

}

#endif
