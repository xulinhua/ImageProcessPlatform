#include "SectionTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鎴潰杞粨" } };
}
std::string SectionTool::toolName() const { return "measure3d.section"; }
ToolType SectionTool::type() const { return ToolType::Measure3D; }
std::string SectionTool::displayName() const { return "鎴潰杞粨"; }
std::string SectionTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> SectionTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string SectionTool::description() const { return "骞抽潰甯︽埅闈㈢偣"; }
std::vector<ParamField> SectionTool::paramFields() const
{
    std::vector<ParamField> f;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud)); p.push_back(PortDesc(PortDir::In, PortData::Shape3)); p.push_back(PortDesc(PortDir::Out, PortData::Cloud)); p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    return f;
}
ParamMap SectionTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> SectionTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut SectionTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "鏈櫥璁版祴閲忕畻娉?@
f.push_back(paramDouble("band", "甯﹀帤", 0.5, true, 0.01, false, 0.0));
@"; }
    return out;
}
ToolBase* SectionTool::clone() const
{
    SectionTool* p = new SectionTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}