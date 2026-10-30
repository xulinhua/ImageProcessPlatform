#include "ExtrinsicTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/Geom3.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "外参合成" } };
}
std::string ExtrinsicTool::toolName() const { return "calib3d.extrinsic"; }
ToolType ExtrinsicTool::type() const { return ToolType::Calib3D; }
std::string ExtrinsicTool::displayName() const { return "多相机外参"; }
std::string ExtrinsicTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> ExtrinsicTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string ExtrinsicTool::description() const { return "由参数或位姿口输出外参位姿"; }
std::vector<ParamField> ExtrinsicTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("tx", "平移X", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("ty", "平移Y", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("tz", "平移Z", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("qx", "四元数X", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("qy", "四元数Y", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("qz", "四元数Z", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("qw", "四元数W", 1.0, true, 0.0, false, 0.0));
    return f;
}
ParamMap ExtrinsicTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> ExtrinsicTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Pose3));
    p.push_back(PortDesc(PortDir::Out, PortData::Pose3));
    return p;
}
ToolOut ExtrinsicTool::execute(const ToolIn& in)
{
    ToolOut out;
    HsCommData::Pose3 pose(in.param.run.value("tx").toDouble(0.0),
                           in.param.run.value("ty").toDouble(0.0),
                           in.param.run.value("tz").toDouble(0.0),
                           in.param.run.value("qx").toDouble(0.0),
                           in.param.run.value("qy").toDouble(0.0),
                           in.param.run.value("qz").toDouble(0.0),
                           in.param.run.value("qw").toDouble(1.0));
    if (!in.inputIo.poses3.empty()) {
        pose = in.inputIo.poses3[0].pose;
    }
    pose = pose.normalized();
    if (!pose.isUnit()) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "外参四元数无效";
        return out;
    }
    PortPose3 pp;
    pp.pose = pose;
    out.io.poses3.push_back(pp);
    out.ok = true;
    out.errCode = HS_OK;
    out.message = "外参位姿";
    return out;
}
ToolBase* ExtrinsicTool::clone() const
{
    ExtrinsicTool* p = new ExtrinsicTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
