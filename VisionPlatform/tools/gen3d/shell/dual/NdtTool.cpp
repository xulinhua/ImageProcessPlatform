#include "NdtTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "NDT閰嶅噯" } };
}
std::string NdtTool::toolName() const { return "locate3d.ndt"; }
ToolType NdtTool::type() const { return ToolType::Locate3D; }
std::string NdtTool::displayName() const { return "NDT閰嶅噯"; }
std::string NdtTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> NdtTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string NdtTool::description() const { return "NDT"; }
std::vector<ParamField> NdtTool::paramFields() const
{
    std::vector<ParamField> f;
    鏈櫥璁扮偣浜戠畻娉?@
f.push_back(paramDouble("resolution", "浣撶礌鍒嗚鲸鐜?, 1.0, true, 0.01, false, 0.0));
f.push_back(paramInt("maxIter", "鏈€澶ц凯浠?, 35, true, 1, true, 200));
@
    return f;
}
ParamMap NdtTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> NdtTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
PortDesc tmpl(PortDir::In, PortData::Cloud); tmpl.slot.name = "template"; p.push_back(tmpl);
p.push_back(PortDesc(PortDir::Out, PortData::Pose3)); p.push_back(PortDesc(PortDir::Out, PortData::Tuple)); p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut NdtTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "閰嶅噯"; }
    return out;
}
ToolBase* NdtTool::clone() const
{
    NdtTool* p = new NdtTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}