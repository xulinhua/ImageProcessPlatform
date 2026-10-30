#include "PoseRefineTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "浣嶅Э绮句慨" } };
}
std::string PoseRefineTool::toolName() const { return "locate3d.pose_refine"; }
ToolType PoseRefineTool::type() const { return ToolType::Locate3D; }
std::string PoseRefineTool::displayName() const { return "浣嶅Э绮句慨"; }
std::string PoseRefineTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> PoseRefineTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string PoseRefineTool::description() const { return "鐭凯浠?ICP"; }
std::vector<ParamField> PoseRefineTool::paramFields() const
{
    std::vector<ParamField> f;
    鏈櫥璁扮偣浜戠畻娉?@
f.push_back(paramDouble("maxCorr", "鏈€澶у搴旇窛绂?, 2.0, true, 0.0, false, 0.0));
f.push_back(paramInt("maxIter", "鏈€澶ц凯浠?, 10, true, 1, true, 50));
@
    return f;
}
ParamMap PoseRefineTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> PoseRefineTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
PortDesc tmpl(PortDir::In, PortData::Cloud); tmpl.slot.name = "template"; p.push_back(tmpl);
p.push_back(PortDesc(PortDir::Out, PortData::Pose3)); p.push_back(PortDesc(PortDir::Out, PortData::Tuple)); p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut PoseRefineTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "绮句慨"; }
    return out;
}
ToolBase* PoseRefineTool::clone() const
{
    PoseRefineTool* p = new PoseRefineTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}