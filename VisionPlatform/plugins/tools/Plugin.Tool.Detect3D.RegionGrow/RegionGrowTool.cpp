#include "RegionGrowTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "区域生长" } };
}
std::string RegionGrowTool::toolName() const { return "detect3d.region_grow"; }
ToolType RegionGrowTool::type() const { return ToolType::Detect3D; }
std::string RegionGrowTool::displayName() const { return "区域生长"; }
std::string RegionGrowTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> RegionGrowTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string RegionGrowTool::description() const { return "法向区域生长"; }
std::vector<ParamField> RegionGrowTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("smoothness", "平滑度", 3.0, true, 0.1, false, 0.0));
    f.push_back(paramInt("minSize", "最小点数", 50, true, 1, true, 1000000));
    return f;
}
ParamMap RegionGrowTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> RegionGrowTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return p;
}
ToolOut RegionGrowTool::execute(const ToolIn& in)
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
ToolBase* RegionGrowTool::clone() const
{
    RegionGrowTool* p = new RegionGrowTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}