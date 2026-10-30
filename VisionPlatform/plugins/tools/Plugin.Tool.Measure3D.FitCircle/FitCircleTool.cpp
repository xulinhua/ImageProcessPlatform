#include "FitCircleTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "拟合圆" } };
}
std::string FitCircleTool::toolName() const { return "measure3d.fit_circle"; }
ToolType FitCircleTool::type() const { return ToolType::Measure3D; }
std::string FitCircleTool::displayName() const { return "拟合圆"; }
std::string FitCircleTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> FitCircleTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string FitCircleTool::description() const { return "拟合三维圆"; }
std::vector<ParamField> FitCircleTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("distance", "距离阈值", 0.5, true, 0.0, false, 0.0));
    return f;
}
ParamMap FitCircleTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> FitCircleTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Shape3));
    PortDesc inlier(PortDir::Out, PortData::Cloud);
    inlier.slot.name = "inlier";
    p.push_back(inlier);
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut FitCircleTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) {
        shot.algLib = AlgLibPcl;
    }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) {
        out.message = "未登记测量算法";
    }
    return out;
}
ToolBase* FitCircleTool::clone() const
{
    FitCircleTool* p = new FitCircleTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}