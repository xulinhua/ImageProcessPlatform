#include "VolumeTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <cmath>
#include <map>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "体积估计" } };
}
std::string VolumeTool::toolName() const { return "measure3d.volume"; }
ToolType VolumeTool::type() const { return ToolType::Measure3D; }
std::string VolumeTool::displayName() const { return "体积"; }
std::string VolumeTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> VolumeTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string VolumeTool::description() const { return "相对基准面以上体积（柱体近似）"; }
std::vector<ParamField> VolumeTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("cell", "网格边长", 1.0, true, 0.01, false, 0.0));
    return f;
}
ParamMap VolumeTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> VolumeTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::In, PortData::Shape3));
    p.push_back(PortDesc(PortDir::Out, PortData::Tuple));
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}
ToolOut VolumeTool::execute(const ToolIn& in)
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
    double cell = in.param.run.value("cell").toDouble(1.0);
    if (cell <= 0.0) {
        cell = 1.0;
    }
    const double area = cell * cell;
    const HsCommData::Cloud3& cloud = in.inputIo.clouds[0].cloud;
    std::map<long long, double> gridMax;
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        const double h = plane.signedDistance(cloud.xyz[i]);
        if (h <= 0.0) {
            continue;
        }
        const long long gx = static_cast<long long>(std::floor(cloud.xyz[i].x / cell));
        const long long gy = static_cast<long long>(std::floor(cloud.xyz[i].y / cell));
        const long long key = (gx << 32) ^ (gy & 0xffffffff);
        std::map<long long, double>::iterator it = gridMax.find(key);
        if (it == gridMax.end() || h > it->second) {
            gridMax[key] = h;
        }
    }
    double volume = 0;
    for (std::map<long long, double>::const_iterator it = gridMax.begin(); it != gridMax.end(); ++it) {
        volume += it->second * area;
    }
    out.values.insert("volume", volume);
    out.values.insert("cells", static_cast<int>(gridMax.size()));
    PortTuple tuple;
    HsCommData::HsTuple values;
    values.append(volume);
    values.append(static_cast<double>(gridMax.size()));
    tuple.value = values;
    out.io.tuples.push_back(tuple);
    out.ok = true;
    out.errCode = HS_OK;
    char msg[128];
    sprintf_s(msg, "volume %.4f", volume);
    out.message = msg;
    PortLogic logic;
    logic.ok = true;
    logic.valid = true;
    logic.message = out.message;
    out.io.logics.push_back(logic);
    return out;
}
ToolBase* VolumeTool::clone() const
{
    VolumeTool* p = new VolumeTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
