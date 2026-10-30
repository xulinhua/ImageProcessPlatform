#include "TemplateTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鐐逛簯妯℃澘鍖归厤" } };
}
std::string TemplateTool::toolName() const { return "locate3d.template"; }
ToolType TemplateTool::type() const { return ToolType::Locate3D; }
std::string TemplateTool::displayName() const { return "鐐逛簯妯℃澘鍖归厤"; }
std::string TemplateTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> TemplateTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string TemplateTool::description() const { return "ICP"; }
std::vector<ParamField> TemplateTool::paramFields() const
{
    std::vector<ParamField> f;
    鏈櫥璁扮偣浜戠畻娉?@
f.push_back(paramDouble("maxCorr", "鏈€澶у搴旇窛绂?, 5.0, true, 0.0, false, 0.0));
f.push_back(paramInt("maxIter", "鏈€澶ц凯浠?, 50, true, 1, true, 200));
@
    return f;
}
ParamMap TemplateTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> TemplateTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
PortDesc tmpl(PortDir::In, PortData::Cloud); tmpl.slot.name = "template"; p.push_back(tmpl);
p.push_back(PortDesc(PortDir::Out, PortData::Pose3)); p.push_back(PortDesc(PortDir::Out, PortData::Tuple)); p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut TemplateTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "妯℃澘閰嶅噯"; }
    return out;
}
ToolBase* TemplateTool::clone() const
{
    TemplateTool* p = new TemplateTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}