#include "PlanePlaneTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <HsCommData/Geom3.h>
#include <cmath>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "面面测量" } };
}
std::string PlanePlaneTool::toolName() const { return "measure3d.plane_plane"; }
ToolType PlanePlaneTool::type() const { return ToolType::Measure3D; }
std::string PlanePlaneTool::displayName() const { return "面面测量"; }
std::string PlanePlaneTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> PlanePlaneTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string PlanePlaneTool::description() const { return "两平面距离与夹角"; }
std::vector<ParamField> PlanePlaneTool::paramFields() const { return std::vector<ParamField>(); }
ParamMap PlanePlaneTool::defaultParams() const { return ParamMap(); }
std::vector<PortDesc> PlanePlaneTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Shape3));
    PortDesc p2(PortDir::In, PortData::Shape3);
    p2.slot.name = "plane2";
    p.push_back(p2);
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut PlanePlaneTool::execute(const ToolIn& in)
{
    ToolOut out;
    HsCommData::Plane3 pa;
    HsCommData::Plane3 pb;
    int found = 0;
    for (size_t i = 0; i < in.inputIo.shapes3.size(); ++i) {
        if (in.inputIo.shapes3[i].kind != Shape3Kind::Plane) {
            continue;
        }
        if (found == 0) {
            pa = in.inputIo.shapes3[i].plane;
            ++found;
        } else {
            pb = in.inputIo.shapes3[i].plane;
            ++found;
            break;
        }
    }
    if (found < 2) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "需要两个平面";
        return out;
    }
    const double dist = std::fabs(pa.signedDistance(pb.origin));
    const double dotn = HsCommData::dot(pa.normal, pb.normal);
    const double clamped = dotn > 1.0 ? 1.0 : (dotn < -1.0 ? -1.0 : dotn);
    const double angleDeg = std::acos(clamped) * 180.0 / 3.14159265358979323846;
    out.values.insert("dist", dist);
    out.values.insert("angle", angleDeg);
    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(dist);
    values.append(angleDeg);
    tuple.value = values;
    out.io.tuples.push_back(tuple);
    out.ok = true;
    out.errCode = HS_OK;
    char msg[128];
    sprintf_s(msg, "dist %.4f  angle %.2f", dist, angleDeg);
    out.message = msg;
    PortLogic logic;
    logic.ok = true;
    logic.valid = true;
    logic.message = out.message;
    out.io.logics.push_back(logic);
    return out;
}
ToolBase* PlanePlaneTool::clone() const
{
    PlanePlaneTool* p = new PlanePlaneTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
