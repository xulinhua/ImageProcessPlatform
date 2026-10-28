/**
 * @file PluginEntry.cpp
 * @brief ONNX 图像分类入口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PluginEntry.h"
#include "OnnxClassify.h"

VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion()
{
    return 1;
}

VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) {
        host->addAlg(new Vision::OnnxClassify());
    }
}
