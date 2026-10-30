#include "TransformTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/Geom3.h>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "坐标变换" } };
}
std::string TransformTool::toolName() const { return "prepare3d.transform"; }
ToolType TransformTool::type() const { return ToolType::Prepare3D; }
std::string TransformTool::displayName() const { return "坐标变换"; }
std::string TransformTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> TransformTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string TransformTool::description() const { return "用位姿或参数刚体变换点云"; }
std::vector<ParamField> TransformTool::paramFields() const
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
ParamMap TransformTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> TransformTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::In, PortData::Pose3));
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return p;
}
ToolOut TransformTool::execute(const ToolIn& in)
{
    ToolOut out;
    if (in.inputIo.clouds.empty() || in.inputIo.clouds[0].cloud.isEmpty()) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "没有点云";
        return out;
    }
    HsCommData::Pose3 pose(in.param.run.value("tx").toDouble(0.0),
                           in.param.run.value("ty").toDouble(0.0),
                           in.param.run.value("tz").toDouble(0.0),
                           in.param.run.value("qx").toDouble(0.0),
                           in.param.run.value("qy").toDouble(0.0),
                           in.param.run.value("qz").toDouble(0.0),
                           in.param.run.value("qw").toDouble(1.0));
    if (!in.inputIo.poses3.empty()) {
        pose = in.inputIo.poses3[0].pose.normalized();
    } else {
        pose = pose.normalized();
    }
    const HsCommData::Cloud3& src = in.inputIo.clouds[0].cloud;
    HsCommData::Cloud3 dst;
    dst.xyz.resize(src.xyz.size());
    for (size_t i = 0; i < src.xyz.size(); ++i) {
        dst.xyz[i] = pose.apply(src.xyz[i]);
    }
    if (!src.normals.empty() && src.normals.size() == src.xyz.size()) {
        dst.normals.resize(src.normals.size());
        for (size_t i = 0; i < src.normals.size(); ++i) {
            dst.normals[i] = pose.rotate(src.normals[i]);
        }
    }
    PortCloud pc;
    pc.cloud = dst;
    out.io.clouds.push_back(pc);
    out.ok = true;
    out.errCode = HS_OK;
    out.message = "已变换";
    return out;
}
ToolBase* TransformTool::clone() const
{
    TransformTool* p = new TransformTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
