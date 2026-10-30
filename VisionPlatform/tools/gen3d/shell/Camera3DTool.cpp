#include "Camera3DTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "三维相机占位" } };
}
std::string Camera3DTool::toolName() const { return "acquire3d.camera"; }
ToolType Camera3DTool::type() const { return ToolType::Acquire3D; }
std::string Camera3DTool::displayName() const { return "三维相机"; }
std::string Camera3DTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> Camera3DTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string Camera3DTool::description() const { return "从三维相机采集点云"; }
std::vector<ParamField> Camera3DTool::paramFields() const { return std::vector<ParamField>(); }
ParamMap Camera3DTool::defaultParams() const { return ParamMap(); }
std::vector<PortDesc> Camera3DTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return p;
}
ToolOut Camera3DTool::execute(const ToolIn& in)
{
    ToolOut out;
    (void)in;
    out.error = true;
    out.errCode = HS_ERR_OPERATOR;
    out.message = "未接三维相机";
    return out;
}
ToolBase* Camera3DTool::clone() const
{
    Camera3DTool* p = new Camera3DTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
