#include "IcpTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点到点 ICP" } };
}
std::string IcpTool::toolName() const { return "align3d.icp"; }
ToolType IcpTool::type() const { return ToolType::Locate3D; }
std::string IcpTool::displayName() const { return "ICP 配准"; }
std::string IcpTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> IcpTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string IcpTool::description() const { return "点到点 ICP，源点云对齐到模板点云"; }
std::vector<ParamField> IcpTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("maxCorr", "最大对应距离", 5.0, true, 0.0, false, 0.0));
    f.push_back(paramInt("maxIter", "最大迭代", 30, true, 1, true, 200));
    return f;
}
ParamMap IcpTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> IcpTool::ports() const
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
ToolOut IcpTool::execute(const ToolIn& in)
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
ToolBase* IcpTool::clone() const
{
    IcpTool* p = new IcpTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
