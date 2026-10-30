#include "HandEyeTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Geom3.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "手眼标定占位" } };
}
std::string HandEyeTool::toolName() const { return "calib3d.hand_eye"; }
ToolType HandEyeTool::type() const { return ToolType::Calib3D; }
std::string HandEyeTool::displayName() const { return "手眼标定"; }
std::string HandEyeTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> HandEyeTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string HandEyeTool::description() const { return "由多组位姿求解手眼矩阵"; }
std::vector<ParamField> HandEyeTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramInt("pairs", "位姿对数", 0, true, 0, true, 64));
    return f;
}
ParamMap HandEyeTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> HandEyeTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::Out, PortData::Pose3));
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    return p;
}
ToolOut HandEyeTool::execute(const ToolIn& in)
{
    ToolOut out;
    const int pairs = in.param.run.value("pairs").toInt(0);
    if (pairs < 2 || in.inputIo.poses3.size() < static_cast<size_t>(pairs * 2)) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "手眼标定需要至少两组机器人与标定位姿对";
        return out;
    }
    out.error = true;
    out.errCode = HS_ERR_OPERATOR;
    out.message = "手眼标定求解未实现";
    return out;
}
ToolBase* HandEyeTool::clone() const
{
    HandEyeTool* p = new HandEyeTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
