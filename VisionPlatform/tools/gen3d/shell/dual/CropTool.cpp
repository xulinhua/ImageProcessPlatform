#include "CropTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "鐐逛簯瑁佸壀" } };
}
std::string CropTool::toolName() const { return "prepare3d.crop"; }
ToolType CropTool::type() const { return ToolType::Prepare3D; }
std::string CropTool::displayName() const { return "鐐逛簯瑁佸壀"; }
std::string CropTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> CropTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string CropTool::description() const { return "杞村榻愮洅瑁佸壀鐐逛簯"; }
std::vector<ParamField> CropTool::paramFields() const
{
    std::vector<ParamField> f;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud)); p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return f;
}
ParamMap CropTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> CropTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut CropTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "鏈櫥璁扮偣浜戠畻娉?@
f.push_back(paramDouble("minX", "鏈€灏廥", -1000.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("minY", "鏈€灏廦", -1000.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("minZ", "鏈€灏廧", -1000.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("maxX", "鏈€澶", 1000.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("maxY", "鏈€澶", 1000.0, true, 0.0, false, 0.0));
f.push_back(paramDouble("maxZ", "鏈€澶", 1000.0, true, 0.0, false, 0.0));
@"; }
    return out;
}
ToolBase* CropTool::clone() const
{
    CropTool* p = new CropTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}