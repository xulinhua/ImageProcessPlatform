#include "ClusterTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鐐逛簯鑱氱被" } };
}
std::string ClusterTool::toolName() const { return "detect3d.cluster"; }
ToolType ClusterTool::type() const { return ToolType::Detect3D; }
std::string ClusterTool::displayName() const { return "鐐逛簯鑱氱被"; }
std::string ClusterTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> ClusterTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string ClusterTool::description() const { return "娆ф皬鑱氱被"; }
std::vector<ParamField> ClusterTool::paramFields() const
{
    std::vector<ParamField> f;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud)); PortDesc c(PortDir::Out, PortData::Cloud); c.slot.name = "cluster"; p.push_back(c); p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    return f;
}
ParamMap ClusterTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> ClusterTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut ClusterTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "鏈櫥璁版娴嬬畻娉?@
f.push_back(paramDouble("tolerance", "璺濈闃堝€?, 2.0, true, 0.01, false, 0.0));
f.push_back(paramInt("minSize", "鏈€灏忕偣鏁?, 10, true, 1, true, 1000000));
@"; }
    return out;
}
ToolBase* ClusterTool::clone() const
{
    ClusterTool* p = new ClusterTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}