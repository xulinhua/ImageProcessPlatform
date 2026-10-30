/**
 * @file CloudViewBinder.cpp
 * @brief CloudViewBinder 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CloudViewBinder.h"
#include "ToolIo.h"
#include <HsCtrl/HsCloudWindow.h>
#include <HsCtrl/HsViewShell.h>

namespace Vision {

bool CloudViewBinder::apply(HsCtrl::HsViewShell* shell, const ToolIo& io, bool switchTo3d)
{
    if (!shell) {
        return false;
    }
    if (io.clouds.empty() && io.shapes3.empty() && io.poses3.empty()) {
        return false;
    }
    HsCtrl::HsCloudWindow* cloudView = shell->ensureCloudWindow();
    if (!cloudView) {
        return false;
    }

    HsCommData::Cloud3 baseCloud;
    HsCommData::Cloud3 inlierCloud;
    bool haveInlier = false;
    for (size_t i = 0; i < io.clouds.size(); ++i) {
        if (io.clouds[i].port == "inlier") {
            inlierCloud = io.clouds[i].cloud;
            haveInlier = true;
        } else if (baseCloud.isEmpty()) {
            baseCloud = io.clouds[i].cloud;
        }
    }
    if (baseCloud.isEmpty() && haveInlier) {
        baseCloud = inlierCloud;
        haveInlier = false;
    }
    cloudView->setCloud(baseCloud);
    cloudView->clearExtraClouds();
    if (haveInlier && !inlierCloud.isEmpty()) {
        cloudView->addCloud("inlier", inlierCloud, HsCommData::Rgb8(40, 220, 80));
    }

    std::vector<HsCtrl::HsCloudShape> shapes;
    for (size_t i = 0; i < io.shapes3.size(); ++i) {
        HsCtrl::HsCloudShape s;
        switch (io.shapes3[i].kind) {
        case Shape3Kind::Plane:
            s.kind = HsCtrl::HsCloudShape::Plane;
            s.plane = io.shapes3[i].plane;
            break;
        case Shape3Kind::Box:
            s.kind = HsCtrl::HsCloudShape::Box;
            s.box = io.shapes3[i].box;
            break;
        case Shape3Kind::Sphere:
            s.kind = HsCtrl::HsCloudShape::Sphere;
            s.sphere = io.shapes3[i].sphere;
            break;
        case Shape3Kind::Cylinder:
            continue;
        case Shape3Kind::Line:
            s.kind = HsCtrl::HsCloudShape::Line;
            s.line = io.shapes3[i].line;
            break;
        case Shape3Kind::Circle:
            s.kind = HsCtrl::HsCloudShape::Circle;
            s.circle = io.shapes3[i].circle;
            break;
        default:
            continue;
        }
        shapes.push_back(s);
    }
    cloudView->setShapes(shapes);

    std::vector<HsCommData::Pose3> poses;
    for (size_t i = 0; i < io.poses3.size(); ++i) {
        poses.push_back(io.poses3[i].pose);
    }
    cloudView->setPoses(poses);

    if (switchTo3d) {
        shell->setCloudModeEnabled(true);
        shell->setViewMode(HsCtrl::ViewModeCloud3D);
    }
    return true;
}

}
