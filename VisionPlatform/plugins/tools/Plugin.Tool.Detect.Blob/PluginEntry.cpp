/**
 * @file PluginEntry.cpp
 * @brief PluginEntry实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PluginEntry.h"
#include "BlobTool.h"

VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion()
{
    return 1;
}

// 注册 Blob 检测工具
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (!host) {
        return;
    }
    host->addTool(new Vision::BlobTool());
}
