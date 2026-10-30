#include "HeightTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <cmath>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点到平面高度" } };
}
std::string HeightTool::toolName() const { return "measure3d.height"; }
ToolType HeightTool::type() const { return ToolType::Measure3D; }
std::string HeightTool::displayName() const { return "点到面高度"; }
std::string HeightTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> HeightTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string HeightTool::description() const { return "计算点云相对平面的有符号高度统计"; }
std::vector<ParamField> HeightTool::paramFields() const { return std::vector<ParamField>(); }
ParamMap HeightTool::defaultParams() const { return ParamMap(); }
std::vector<PortDesc> HeightTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::In, PortData::Shape3));
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}

ToolOut HeightTool::execute(const ToolIn& in)
{
    ToolOut out;
    if (in.inputIo.clouds.empty() || in.inputIo.clouds[0].cloud.isEmpty()) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "没有点云";
        return out;
    }
    HsCommData::Plane3 plane;
    bool havePlane = false;
    for (size_t i = 0; i < in.inputIo.shapes3.size(); ++i) {
        if (in.inputIo.shapes3[i].kind == Shape3Kind::Plane) {
            plane = in.inputIo.shapes3[i].plane;
            havePlane = true;
            break;
        }
    }
    if (!havePlane) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "没有平面";
        return out;
    }
    const HsCommData::Cloud3& cloud = in.inputIo.clouds[0].cloud;
    double sum = 0;
    double sumSq = 0;
    double minH = 0;
    double maxH = 0;
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
        sum += h;
        sumSq += h * h;
    }
    const double n = static_cast<double>(cloud.xyz.size());
    const double mean = sum / n;
    const double rms = std::sqrt(sumSq / n);
    out.values.insert("mean", mean);
    out.values.insert("min", minH);
    out.values.insert("max", maxH);
    out.values.insert("rms", rms);
    out.values.insert("points", static_cast<int>(cloud.xyz.size()));
    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(mean);
    values.append(minH);
    values.append(maxH);
    values.append(rms);
    values.append(n);
    tuple.value = values;
    out.io.tuples.push_back(tuple);
    out.ok = true;
    out.errCode = HS_OK;
    char msg[128];
    sprintf_s(msg, "mean %.4f  rms %.4f", mean, rms);
    out.message = msg;
    for (size_t i = 0; i < in.param.judge.size(); ++i) {
        const JudgeParam& row = in.param.judge[i];
        if (!row.enable || !out.values.contains(row.name)) {
            continue;
        }
        const ParamValue got = out.values.value(row.name);
        if (got.kind() != ParamValue::KindInt && got.kind() != ParamValue::KindDouble) {
            continue;
        }
        const double value = got.toDouble(0);
        if (value < row.minVal || value > row.maxVal) {
            out.ok = false;
            out.message = row.name + " 超出范围";
        }
    }
    PortLogic logic;
    logic.ok = out.ok;
    logic.valid = true;
    logic.message = out.message;
    out.io.logics.push_back(logic);
    return out;
}

ToolBase* HeightTool::clone() const
{
    HeightTool* p = new HeightTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
