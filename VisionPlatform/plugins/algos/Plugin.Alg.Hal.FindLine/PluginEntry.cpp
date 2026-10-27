/**
 * @file PluginEntry.cpp
 * @brief PluginEntryÊµÏÖ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PluginEntry.h"
#include "HalFindLine.h"

VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion()
{
    return 1;
}

VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) {
        host->addAlg(new Vision::HalFindLine());
    }
}
