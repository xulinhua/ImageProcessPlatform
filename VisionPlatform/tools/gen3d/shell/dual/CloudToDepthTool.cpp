#include "CloudToDepthTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鐐逛簯杞繁搴﹀浘" } };
}
std::string CloudToDepthTool::toolName() const { return "prepare3d.cloud_to_depth"; }
ToolType CloudToDepthTool::type() const { return ToolType::Prepare3D; }
std::string CloudToDepthTool::displayName() const { return "鐐逛簯杞繁搴﹀浘"; }
std::string CloudToDepthTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> CloudToDepthTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string CloudToDepthTool::description() const { return "鐐逛簯鎶曞奖涓烘繁搴﹀浘"; }
std::vector<ParamField> CloudToDepthTool::paramFields() const
{
    std::vector<ParamField> f;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud)); p.push_back(PortDesc(PortDir::Out, PortData::Image));
    return f;
}
ParamMap CloudToDepthTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> CloudToDepthTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut CloudToDepthTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "鏈櫥璁扮偣浜戠畻娉?@
f.push_back(paramDouble("fx", "fx", 500.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("fy", "fy", 500.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("cx", "cx", 320.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("cy", "cy", 240.0, true, 0.0, false, 0.0));
f.push_back(paramInt("width", "瀹藉害", 640, true, 1, true, 8192));
f.push_back(paramInt("height", "楂樺害", 480, true, 1, true, 8192));
@"; }
    return out;
}
ToolBase* CloudToDepthTool::clone() const
{
    CloudToDepthTool* p = new CloudToDepthTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}