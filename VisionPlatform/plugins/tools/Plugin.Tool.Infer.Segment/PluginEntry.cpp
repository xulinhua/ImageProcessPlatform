/**
 * @file PluginEntry.cpp
 * @brief 语义分割工具入口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PluginEntry.h"
#include "SegmentTool.h"

VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion()
{
    return 1;
}

VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (!host) {
        return;
    }
    host->addTool(new Vision::SegmentTool());
}
