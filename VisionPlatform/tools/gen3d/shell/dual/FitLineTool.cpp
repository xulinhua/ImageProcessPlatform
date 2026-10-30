#include "FitLineTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鎷熷悎鐩寸嚎" } };
}
std::string FitLineTool::toolName() const { return "measure3d.fit_line"; }
ToolType FitLineTool::type() const { return ToolType::Measure3D; }
std::string FitLineTool::displayName() const { return "鎷熷悎鐩寸嚎"; }
std::string FitLineTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> FitLineTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string FitLineTool::description() const { return "鎷熷悎涓夌淮鐩寸嚎"; }
std::vector<ParamField> FitLineTool::paramFields() const
{
    std::vector<ParamField> f;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
p.push_back(PortDesc(PortDir::Out, PortData::Shape3));
PortDesc inlier(PortDir::Out, PortData::Cloud); inlier.slot.name = "inlier"; p.push_back(inlier);
p.push_back(PortDesc(PortDir::Out, PortData::Tuple)); p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return f;
}
ParamMap FitLineTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> FitLineTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut FitLineTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "鏈櫥璁版祴閲忕畻娉?@
f.push_back(paramDouble("distance", "璺濈闃堝€?, 0.5, true, 0.0, false, 0.0));
@"; }
    return out;
}
ToolBase* FitLineTool::clone() const
{
    FitLineTool* p = new FitLineTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}