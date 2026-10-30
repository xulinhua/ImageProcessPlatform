#include "PluginEntry.h"
#include "Open3dDiff.h"
VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion() { return 1; }
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) {
        host->addAlg(new Vision::Open3dDiff());
    }
}
