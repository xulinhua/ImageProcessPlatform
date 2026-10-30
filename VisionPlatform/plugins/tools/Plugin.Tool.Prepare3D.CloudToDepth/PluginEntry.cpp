#include "PluginEntry.h"
#include "CloudToDepthTool.h"

VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion() { return 1; }
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) {
        host->addTool(new Vision::CloudToDepthTool());
    }
}
