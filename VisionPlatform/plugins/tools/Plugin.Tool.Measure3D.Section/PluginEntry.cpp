#include "PluginEntry.h"
#include "SectionTool.h"
VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion() { return 1; }
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) {
        host->addTool(new Vision::SectionTool());
    }
}
