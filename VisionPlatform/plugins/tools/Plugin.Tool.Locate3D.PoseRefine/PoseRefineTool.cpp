#include "PoseRefineTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "位姿精修" } };
}
std::string PoseRefineTool::toolName() const { return "locate3d.pose_refine"; }
ToolType PoseRefineTool::type() const { return ToolType::Locate3D; }
std::string PoseRefineTool::displayName() const { return "位姿精修"; }
std::string PoseRefineTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> PoseRefineTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string PoseRefineTool::description() const { return "短迭代 ICP 精修"; }
std::vector<ParamField> PoseRefineTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("maxCorr", "最大对应距离", 2.0, true, 0.0, false, 0.0));
    f.push_back(paramInt("maxIter", "最大迭代", 10, true, 1, true, 50));
    return f;
}
ParamMap PoseRefineTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> PoseRefineTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    PortDesc tmpl(PortDir::In, PortData::Cloud);
    tmpl.slot.name = "template";
    p.push_back(tmpl);
    p.push_back(PortDesc(PortDir::Out, PortData::Pose3));
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut PoseRefineTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) {
        shot.algLib = AlgLibPcl;
    }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) {
        out.message = "未登记点云算法";
    }
    return out;
}
ToolBase* PoseRefineTool::clone() const
{
    PoseRefineTool* p = new PoseRefineTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}