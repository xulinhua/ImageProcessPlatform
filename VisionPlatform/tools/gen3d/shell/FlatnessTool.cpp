#include "FlatnessTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <cmath>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "平面度" } };
}
std::string FlatnessTool::toolName() const { return "measure3d.flatness"; }
ToolType FlatnessTool::type() const { return ToolType::Measure3D; }
std::string FlatnessTool::displayName() const { return "平面度"; }
std::string FlatnessTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> FlatnessTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string FlatnessTool::description() const { return "点云相对平面的平面度（峰谷与RMS）"; }
std::vector<ParamField> FlatnessTool::paramFields() const { return std::vector<ParamField>(); }
ParamMap FlatnessTool::defaultParams() const { return ParamMap(); }
std::vector<PortDesc> FlatnessTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::In, PortData::Shape3));
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut FlatnessTool::execute(const ToolIn& in)
{
    ToolOut out;
    if (in.inputIo.clouds.empty() || in.inputIo.clouds[0].cloud.isEmpty()) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "没有点云";
        return out;
    }
    HsCommData::Plane3 plane;
    bool have = false;
    for (size_t i = 0; i < in.inputIo.shapes3.size(); ++i) {
        if (in.inputIo.shapes3[i].kind == Shape3Kind::Plane) {
            plane = in.inputIo.shapes3[i].plane;
            have = true;
            break;
        }
    }
    if (!have) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "没有平面";
        return out;
    }
    const HsCommData::Cloud3& cloud = in.inputIo.clouds[0].cloud;
    double minH = 0;
    double maxH = 0;
    double sumSq = 0;
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        const double h = plane.signedDistance(cloud.xyz[i]);
        if (i == 0) {
            minH = h;
            maxH = h;
        } else {
            if (h < minH) {
                minH = h;
            }
            if (h > maxH) {
                maxH = h;
            }
        }
        sumSq += h * h;
    }
    const double n = static_cast<double>(cloud.xyz.size());
    const double rms = std::sqrt(sumSq / n);
    const double flatness = maxH - minH;
    out.values.insert("flatness", flatness);
    out.values.insert("min", minH);
    out.values.insert("max", maxH);
    out.values.insert("rms", rms);
    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(flatness);
    values.append(minH);
    values.append(maxH);
    values.append(rms);
    tuple.value = values;
    out.io.tuples.push_back(tuple);
    out.ok = true;
    out.errCode = HS_OK;
    char msg[128];
    sprintf_s(msg, "flatness %.4f  rms %.4f", flatness, rms);
    out.message = msg;
    PortLogic logic;
    logic.ok = out.ok;
    logic.valid = true;
    logic.message = out.message;
    out.io.logics.push_back(logic);
    return out;
}
ToolBase* FlatnessTool::clone() const
{
    FlatnessTool* p = new FlatnessTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
