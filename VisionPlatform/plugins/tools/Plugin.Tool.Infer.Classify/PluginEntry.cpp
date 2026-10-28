/**
 * @file PluginEntry.cpp
 * @brief 图像分类工具入口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PluginEntry.h"
#include "ClassifyTool.h"

VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion()
{
    return 1;
}

VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (!host) {
        return;
    }
    host->addTool(new Vision::ClassifyTool());
}
