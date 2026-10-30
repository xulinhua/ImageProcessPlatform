#include "DepthToCloudTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "娣卞害鍥捐浆鐐逛簯" } };
}
std::string DepthToCloudTool::toolName() const { return "prepare3d.depth_to_cloud"; }
ToolType DepthToCloudTool::type() const { return ToolType::Prepare3D; }
std::string DepthToCloudTool::displayName() const { return "娣卞害鍥捐浆鐐逛簯"; }
std::string DepthToCloudTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> DepthToCloudTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string DepthToCloudTool::description() const { return "娣卞害鍥捐浆鐐逛簯"; }
std::vector<ParamField> DepthToCloudTool::paramFields() const
{
    std::vector<ParamField> f;
    p.push_back(PortDesc(PortDir::In, PortData::Image)); p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return f;
}
ParamMap DepthToCloudTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> DepthToCloudTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut DepthToCloudTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "鏈櫥璁扮偣浜戠畻娉?@
f.push_back(paramDouble("fx", "fx", 500.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("fy", "fy", 500.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("cx", "cx", 320.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("cy", "cy", 240.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("scale", "娣卞害姣斾緥", 1.0, true, 0.0, false, 0.0));
@"; }
    return out;
}
ToolBase* DepthToCloudTool::clone() const
{
    DepthToCloudTool* p = new DepthToCloudTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}