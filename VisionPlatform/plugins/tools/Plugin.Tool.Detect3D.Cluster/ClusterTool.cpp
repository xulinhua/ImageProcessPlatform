#include "ClusterTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云聚类" } };
}
std::string ClusterTool::toolName() const { return "detect3d.cluster"; }
ToolType ClusterTool::type() const { return ToolType::Detect3D; }
std::string ClusterTool::displayName() const { return "点云聚类"; }
std::string ClusterTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> ClusterTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string ClusterTool::description() const { return "欧式聚类"; }
std::vector<ParamField> ClusterTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("tolerance", "距离阈值", 2.0, true, 0.01, false, 0.0));
    f.push_back(paramInt("minSize", "最小点数", 10, true, 1, true, 1000000));
    return f;
}
ParamMap ClusterTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> ClusterTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    PortDesc c(PortDir::Out, PortData::Cloud);
    c.slot.name = "cluster";
    p.push_back(c);
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    return p;
}
ToolOut ClusterTool::execute(const ToolIn& in)
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
ToolBase* ClusterTool::clone() const
{
    ClusterTool* p = new ClusterTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}