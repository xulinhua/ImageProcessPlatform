#include "CropTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云裁剪" } };
}
std::string CropTool::toolName() const { return "prepare3d.crop"; }
ToolType CropTool::type() const { return ToolType::Prepare3D; }
std::string CropTool::displayName() const { return "点云裁剪"; }
std::string CropTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> CropTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string CropTool::description() const { return "轴对齐盒裁剪点云"; }
std::vector<ParamField> CropTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("minX", "最小X", -1000.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("minY", "最小Y", -1000.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("minZ", "最小Z", -1000.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("maxX", "最大X", 1000.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("maxY", "最大Y", 1000.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("maxZ", "最大Z", 1000.0, true, 0.0, false, 0.0));
    return f;
}
ParamMap CropTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> CropTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return p;
}
ToolOut CropTool::execute(const ToolIn& in)
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
ToolBase* CropTool::clone() const
{
    CropTool* p = new CropTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
