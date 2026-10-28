/**
 * @file PluginEntry.cpp
 * @brief PluginEntry实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PluginEntry.h"
#include "CvCalibPlate.h"

VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion()
{
    return 1;
}

// register plate and npoint finders
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (!host) {
        return;
    }
    host->addAlg(new Vision::CvCalibPlate());
    host->addAlg(new Vision::CvCalibPlane());
}
