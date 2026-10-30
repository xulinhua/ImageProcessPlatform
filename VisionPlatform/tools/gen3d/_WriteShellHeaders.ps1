param([string]$Dir, [string]$Class)
$h = @"
#ifndef VISION_${Class}_TOOL_H
#define VISION_${Class}_TOOL_H
#include "ToolBase.h"
namespace Vision {
class ${Class}Tool : public ToolBase {
public:
    std::string toolName() const;
    ToolType type() const;
    std::string displayName() const;
    std::string version() const;
    std::vector<VerNote> versionNotes() const;
    std::string description() const;
    ParamMap defaultParams() const;
    std::vector<ParamField> paramFields() const;
    std::vector<PortDesc> ports() const;
    ToolOut execute(const ToolIn& in);
    ToolBase* clone() const;
};
}
#endif
"@
$entry = @"
#include "PluginEntry.h"
#include "${Class}Tool.h"
VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion() { return 1; }
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) { host->addTool(new Vision::${Class}Tool()); }
}
"@
[IO.File]::WriteAllText((Join-Path $Dir "${Class}Tool.h"), $h, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $Dir 'PluginEntry.cpp'), $entry, [Text.UTF8Encoding]::new($false))
