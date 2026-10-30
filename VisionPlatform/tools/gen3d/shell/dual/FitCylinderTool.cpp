#include "FitCylinderTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鎷熷悎鍦嗘煴" } };
}
std::string FitCylinderTool::toolName() const { return "measure3d.fit_cylinder"; }
ToolType FitCylinderTool::type() const { return ToolType::Measure3D; }
std::string FitCylinderTool::displayName() const { return "鎷熷悎鍦嗘煴"; }
std::string FitCylinderTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> FitCylinderTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string FitCylinderTool::description() const { return "RANSAC"; }
std::vector<ParamField> FitCylinderTool::paramFields() const
{
    std::vector<ParamField> f;
    鏈櫥璁版祴閲忕畻娉?@
f.push_back(paramDouble("distance", "璺濈闃堝€?, 0.5, true, 0.0, false, 0.0));
@
    return f;
}
ParamMap FitCylinderTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> FitCylinderTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
p.push_back(PortDesc(PortDir::Out, PortData::Shape3));
PortDesc inlier(PortDir::Out, PortData::Cloud); inlier.slot.name = "inlier"; p.push_back(inlier);
p.push_back(PortDesc(PortDir::Out, PortData::Tuple)); p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut FitCylinderTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "鎷熷悎鍦嗘煴"; }
    return out;
}
ToolBase* FitCylinderTool::clone() const
{
    FitCylinderTool* p = new FitCylinderTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}