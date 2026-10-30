#include "DiffTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云差分" } };
}
std::string DiffTool::toolName() const { return "detect3d.diff"; }
ToolType DiffTool::type() const { return ToolType::Detect3D; }
std::string DiffTool::displayName() const { return "点云差分"; }
std::string DiffTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> DiffTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string DiffTool::description() const { return "点到模板距离差分"; }
std::vector<ParamField> DiffTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("threshold", "距离阈值", 1.0, true, 0.0, false, 0.0));
    return f;
}
ParamMap DiffTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> DiffTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    PortDesc t(PortDir::In, PortData::Cloud);
    t.slot.name = "template";
    p.push_back(t);
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    return p;
}
ToolOut DiffTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) {
        shot.algLib = AlgLibPcl;
    }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) {
        out.message = "未登记检测算法";
    }
    return out;
}
ToolBase* DiffTool::clone() const
{
    DiffTool* p = new DiffTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}