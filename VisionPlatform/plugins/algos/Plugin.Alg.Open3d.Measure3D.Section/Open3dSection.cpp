#include "Open3dSection.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Shape3.h>
#include <cmath>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D 平面带宽截面" } };
}
std::vector<VerNote> Open3dSection::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int Open3dSection::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    HsCommData::Plane3 plane;
    bool have = false;
    for (size_t i = 0; i < in.shapes3.size(); ++i) {
        if (in.shapes3[i].kind == Shape3Kind::Plane) {
            plane = in.shapes3[i].plane;
            have = true;
            break;
        }
    }
    if (!have) {
        return fail(HS_ERR_OPERATOR, "没有平面", out);
    }
    double band = in.run.value("band").toDouble(0.5);
    if (band < 0.01) { band = 0.01; }
    HsCommData::Cloud3 section;
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        const double d = std::fabs(plane.signedDistance(cloud.xyz[i]));
        if (d <= band) {
            section.xyz.push_back(cloud.xyz[i]);
        }
    }
    if (section.xyz.empty()) {
        return fail(HS_ERR_OPERATOR, "截面无点", out);
    }
    PortCloud pc; pc.cloud = section; out->clouds.push_back(pc);
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("points", static_cast<int>(section.xyz.size()));
    PortTuple tuple; HsCommData::HsTuple values;
    values.append(static_cast<double>(section.xyz.size()));
    tuple.value = values; out->tuples.push_back(tuple);
    char msg[64]; sprintf_s(msg, "截面点 %d", static_cast<int>(section.xyz.size()));
    out->message = msg;
    return HS_OK;
}
}