#include "FitCircleTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鎷熷悎鍦?Measure3D" } };
}
std::string FitCircleTool::toolName() const { return "measure3d.fit_circle"; }
ToolType FitCircleTool::type() const { return ToolType::鎷熷悎涓夌淮鍦?鏈櫥璁版祴閲忕畻娉?@
f.push_back(paramDouble("distance", "璺濈闃堝€?, 0.5, true, 0.0, false, 0.0));
@; }
std::string FitCircleTool::displayName() const { return "鎷熷悎鍦?Measure3D"; }
std::string FitCircleTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> FitCircleTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string FitCircleTool::description() const { return "p.push_back(PortDesc(PortDir::In, PortData::Cloud));
p.push_back(PortDesc(PortDir::Out, PortData::Shape3));
PortDesc inlier(PortDir::Out, PortData::Cloud); inlier.slot.name = "inlier"; p.push_back(inlier);
p.push_back(PortDesc(PortDir::Out, PortData::Tuple)); p.push_back(PortDesc(PortDir::Out, PortData::Logic));"; }
std::vector<ParamField> FitCircleTool::paramFields() const
{
    std::vector<ParamField> f;
    
    return f;
}
ParamMap FitCircleTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> FitCircleTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut FitCircleTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = ""; }
    return out;
}
ToolBase* FitCircleTool::clone() const
{
    FitCircleTool* p = new FitCircleTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}