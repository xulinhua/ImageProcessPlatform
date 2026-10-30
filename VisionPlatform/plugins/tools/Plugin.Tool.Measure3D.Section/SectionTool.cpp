#include "SectionTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "截面轮廓" } };
}
std::string SectionTool::toolName() const { return "measure3d.section"; }
ToolType SectionTool::type() const { return ToolType::Measure3D; }
std::string SectionTool::displayName() const { return "截面轮廓"; }
std::string SectionTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> SectionTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string SectionTool::description() const { return "平面带宽截取截面点"; }
std::vector<ParamField> SectionTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("band", "带宽", 0.5, true, 0.01, false, 0.0));
    return f;
}
ParamMap SectionTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> SectionTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::In, PortData::Shape3));
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    return p;
}
ToolOut SectionTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) {
        shot.algLib = AlgLibPcl;
    }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) {
        out.message = "未登记测量算法";
    }
    return out;
}
ToolBase* SectionTool::clone() const
{
    SectionTool* p = new SectionTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}