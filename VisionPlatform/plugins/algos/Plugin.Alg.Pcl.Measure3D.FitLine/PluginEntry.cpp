#include "PluginEntry.h"
#include "PclFitLine.h"
VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion() { return 1; }
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) {
        host->addAlg(new Vision::PclFitLine());
    }
}
