#include "FitLineTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "拟合直线" } };
}
std::string FitLineTool::toolName() const { return "measure3d.fit_line"; }
ToolType FitLineTool::type() const { return ToolType::Measure3D; }
std::string FitLineTool::displayName() const { return "拟合直线"; }
std::string FitLineTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> FitLineTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string FitLineTool::description() const { return "拟合三维直线"; }
std::vector<ParamField> FitLineTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("distance", "距离阈值", 0.5, true, 0.0, false, 0.0));
    return f;
}
ParamMap FitLineTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> FitLineTool::ports() const
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
ToolOut FitLineTool::execute(const ToolIn& in)
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
ToolBase* FitLineTool::clone() const
{
    FitLineTool* p = new FitLineTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}