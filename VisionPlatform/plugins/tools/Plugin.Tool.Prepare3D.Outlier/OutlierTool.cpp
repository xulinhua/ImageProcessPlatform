#include "OutlierTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "离群点滤波" } };
}
std::string OutlierTool::toolName() const { return "prepare3d.outlier"; }
ToolType OutlierTool::type() const { return ToolType::Prepare3D; }
std::string OutlierTool::displayName() const { return "离群点滤波"; }
std::string OutlierTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> OutlierTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string OutlierTool::description() const { return "统计离群点滤波"; }
std::vector<ParamField> OutlierTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramInt("meanK", "邻域点数", 50, true, 1, true, 500));
    f.push_back(paramDouble("stddev", "标准差倍数", 1.0, true, 0.01, false, 0.0));
    return f;
}
ParamMap OutlierTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> OutlierTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return p;
}
ToolOut OutlierTool::execute(const ToolIn& in)
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
ToolBase* OutlierTool::clone() const
{
    OutlierTool* p = new OutlierTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}