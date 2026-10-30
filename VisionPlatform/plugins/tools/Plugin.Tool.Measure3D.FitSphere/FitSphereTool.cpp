#include "FitSphereTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "拟合球" } };
}
std::string FitSphereTool::toolName() const { return "measure3d.fit_sphere"; }
ToolType FitSphereTool::type() const { return ToolType::Measure3D; }
std::string FitSphereTool::displayName() const { return "拟合球"; }
std::string FitSphereTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> FitSphereTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string FitSphereTool::description() const { return "用点云拟合球"; }
std::vector<ParamField> FitSphereTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("distance", "距离阈值", 0.5, true, 0.0, false, 0.0));
    return f;
}
ParamMap FitSphereTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> FitSphereTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Shape3));
    PortDesc inlier(PortDir::Out, PortData::Cloud);
    inlier.slot.name = "inlier";
    p.push_back(inlier);
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut FitSphereTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) {
        shot.algLib = AlgLibPcl;
    }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) {
        out.message = "未登记测量算法";
    }
    if (!shot.inputIo.clouds.empty() && !shot.inputIo.clouds[0].cloud.isEmpty()) {
        PortCloud base = shot.inputIo.clouds[0];
        base.port.clear();
        base.index = 0;
        out.io.clouds.insert(out.io.clouds.begin(), base);
    }
    return out;
}
ToolBase* FitSphereTool::clone() const
{
    FitSphereTool* p = new FitSphereTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
