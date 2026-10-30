#include "NormalsTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "法向估计" } };
}
std::string NormalsTool::toolName() const { return "prepare3d.normals"; }
ToolType NormalsTool::type() const { return ToolType::Prepare3D; }
std::string NormalsTool::displayName() const { return "法向估计"; }
std::string NormalsTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> NormalsTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string NormalsTool::description() const { return "估计点云法向"; }
std::vector<ParamField> NormalsTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("radius", "搜索半径", 5.0, true, 0.01, false, 0.0));
    return f;
}
ParamMap NormalsTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> NormalsTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return p;
}
ToolOut NormalsTool::execute(const ToolIn& in)
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
ToolBase* NormalsTool::clone() const
{
    NormalsTool* p = new NormalsTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}