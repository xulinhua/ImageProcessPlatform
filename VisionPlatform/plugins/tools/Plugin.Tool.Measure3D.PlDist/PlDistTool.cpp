#include "PlDistTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <HsCommData/Geom3.h>
#include <cmath>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点线距离" } };
}
std::string PlDistTool::toolName() const { return "measure3d.pl_dist"; }
ToolType PlDistTool::type() const { return ToolType::Measure3D; }
std::string PlDistTool::displayName() const { return "点线距离"; }
std::string PlDistTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> PlDistTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string PlDistTool::description() const { return "点到三维直线距离"; }
std::vector<ParamField> PlDistTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("px", "点 X", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("py", "点 Y", 0.0, true, 0.0, false, 0.0));
    f.push_back(paramDouble("pz", "点 Z", 0.0, true, 0.0, false, 0.0));
    return f;
}
ParamMap PlDistTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> PlDistTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Shape3));
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut PlDistTool::execute(const ToolIn& in)
{
    ToolOut out;
    HsCommData::Line3 line;
    bool have = false;
    for (size_t i = 0; i < in.inputIo.shapes3.size(); ++i) {
        if (in.inputIo.shapes3[i].kind == Shape3Kind::Line) {
            line = in.inputIo.shapes3[i].line;
            have = true;
            break;
        }
    }
    if (!have) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "没有直线";
        return out;
    }
    const HsCommData::Point3d p(in.param.run.value("px").toDouble(0.0),
                                in.param.run.value("py").toDouble(0.0),
                                in.param.run.value("pz").toDouble(0.0));
    const HsCommData::Point3d ab = line.b - line.a;
    const double len = HsCommData::length(ab);
    double dist = 0;
    if (len < 1e-12) {
        dist = HsCommData::dist(p, line.a);
    } else {
        const HsCommData::Point3d ap = p - line.a;
        const HsCommData::Point3d crossv = HsCommData::cross(ap, ab);
        dist = HsCommData::length(crossv) / len;
    }
    out.values.insert("dist", dist);
    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(dist);
    tuple.value = values;
    out.io.tuples.push_back(tuple);
    out.ok = true;
    out.errCode = HS_OK;
    char msg[64];
    sprintf_s(msg, "dist %.4f", dist);
    out.message = msg;
    PortLogic logic;
    logic.ok = true;
    logic.valid = true;
    logic.message = out.message;
    out.io.logics.push_back(logic);
    return out;
}
ToolBase* PlDistTool::clone() const
{
    PlDistTool* p = new PlDistTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
