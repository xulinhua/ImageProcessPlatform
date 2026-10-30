#include "PpDistTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Geom3.h>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点点距离" } };
}
std::string PpDistTool::toolName() const { return "measure3d.pp_dist"; }
ToolType PpDistTool::type() const { return ToolType::Measure3D; }
std::string PpDistTool::displayName() const { return "点点距离"; }
std::string PpDistTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> PpDistTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string PpDistTool::description() const { return "计算两点欧氏距离"; }
std::vector<ParamField> PpDistTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("x1", "点1 X", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("y1", "点1 Y", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("z1", "点1 Z", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("x2", "点2 X", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("y2", "点2 Y", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("z2", "点2 Z", 0.0, true, 0.0, false, 0.0));
    return f;
}
ParamMap PpDistTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> PpDistTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut PpDistTool::execute(const ToolIn& in)
{
    ToolOut out;
    HsCommData::Point3d p1(in.param.run.value("x1").toDouble(0.0),
                           in.param.run.value("y1").toDouble(0.0),
                           in.param.run.value("z1").toDouble(0.0));
    HsCommData::Point3d p2(in.param.run.value("x2").toDouble(0.0),
                           in.param.run.value("y2").toDouble(0.0),
                           in.param.run.value("z2").toDouble(0.0));
    const double d = HsCommData::dist(p1, p2);
    out.values.insert("dist", d);
    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(d);
    tuple.value = values;
    out.io.tuples.push_back(tuple);
    out.ok = true;
    out.errCode = HS_OK;
    char msg[64];
    sprintf_s(msg, "dist %.4f", d);
    out.message = msg;
    PortLogic logic;
    logic.ok = true;
    logic.valid = true;
    logic.message = out.message;
    out.io.logics.push_back(logic);
    return out;
}
ToolBase* PpDistTool::clone() const
{
    PpDistTool* p = new PpDistTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
