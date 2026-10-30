#include "DiffTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鐐逛簯宸垎" } };
}
std::string DiffTool::toolName() const { return "detect3d.diff"; }
ToolType DiffTool::type() const { return ToolType::Detect3D; }
std::string DiffTool::displayName() const { return "鐐逛簯宸垎"; }
std::string DiffTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> DiffTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string DiffTool::description() const { return "鐐瑰埌妯℃澘璺濈宸垎"; }
std::vector<ParamField> DiffTool::paramFields() const
{
    std::vector<ParamField> f;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud)); PortDesc t(PortDir::In, PortData::Cloud); t.slot.name = "template"; p.push_back(t); p.push_back(PortDesc(PortDir::Out, PortData::Cloud)); p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    return f;
}
ParamMap DiffTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> DiffTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut DiffTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "鏈櫥璁版娴嬬畻娉?@
f.push_back(paramDouble("threshold", "璺濈闃堝€?, 1.0, true, 0.0, false, 0.0));
@"; }
    return out;
}
ToolBase* DiffTool::clone() const
{
    DiffTool* p = new DiffTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}