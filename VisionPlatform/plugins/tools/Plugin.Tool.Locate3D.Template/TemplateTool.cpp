#include "TemplateTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云模板匹配" } };
}
std::string TemplateTool::toolName() const { return "locate3d.template"; }
ToolType TemplateTool::type() const { return ToolType::Locate3D; }
std::string TemplateTool::displayName() const { return "点云模板匹配"; }
std::string TemplateTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> TemplateTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string TemplateTool::description() const { return "ICP 模板配准"; }
std::vector<ParamField> TemplateTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("maxCorr", "最大对应距离", 5.0, true, 0.0, false, 0.0));
    f.push_back(paramInt("maxIter", "最大迭代", 50, true, 1, true, 200));
    return f;
}
ParamMap TemplateTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> TemplateTool::ports() const
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
ToolOut TemplateTool::execute(const ToolIn& in)
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
ToolBase* TemplateTool::clone() const
{
    TemplateTool* p = new TemplateTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}