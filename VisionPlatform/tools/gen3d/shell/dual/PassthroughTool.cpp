#include "PassthroughTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鐩撮€氭护娉?Prepare3D" } };
}
std::string PassthroughTool::toolName() const { return "prepare3d.passthrough"; }
ToolType PassthroughTool::type() const { return ToolType::鎸夎酱鑼冨洿婊ゆ尝; }
std::string PassthroughTool::displayName() const { return "鐩撮€氭护娉?Prepare3D"; }
std::string PassthroughTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> PassthroughTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string PassthroughTool::description() const { return "鏈櫥璁扮偣浜戠畻娉?@
f.push_back(paramString("axis", "杞?, std::string("z"), paramChoices("x", "y", "z")));
f.push_back(paramDouble("min", "鏈€灏?, -1e6, true, 0.0, false, 0.0));
f.push_back(paramDouble("max", "鏈€澶?, 1e6, true, 0.0, false, 0.0));
@"; }
std::vector<ParamField> PassthroughTool::paramFields() const
{
    std::vector<ParamField> f;
    
    return f;
}
ParamMap PassthroughTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> PassthroughTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut PassthroughTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "p.push_back(PortDesc(PortDir::In, PortData::Cloud)); p.push_back(PortDesc(PortDir::Out, PortData::Cloud));"; }
    return out;
}
ToolBase* PassthroughTool::clone() const
{
    PassthroughTool* p = new PassthroughTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}