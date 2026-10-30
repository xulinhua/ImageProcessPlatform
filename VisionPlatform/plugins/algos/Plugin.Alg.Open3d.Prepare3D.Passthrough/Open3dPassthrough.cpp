#include "Open3dPassthrough.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <stdio.h>

#if defined(__has_include)
#  if __has_include(<open3d/geometry/PointCloud.h>)
#    define VISION_O3_PASS_USE 1
#    include <open3d/geometry/PointCloud.h>
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D 直通滤波" } };
}
std::vector<VerNote> Open3dPassthrough::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int Open3dPassthrough::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
#if !defined(VISION_O3_PASS_USE)
    return fail(HS_ERR_OPERATOR, "未安装 Open3D", out);
#else
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    std::string axis = in.run.value("axis").toString();
    if (axis.empty()) { axis = "z"; }
    const double vmin = in.run.value("min").toDouble(-1e6);
    const double vmax = in.run.value("max").toDouble(1e6);
    int dim = 2;
    if (axis == "x") dim = 0; else if (axis == "y") dim = 1;
    HsCommData::Cloud3 result;
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        const double v = (dim == 0) ? cloud.xyz[i].x : ((dim == 1) ? cloud.xyz[i].y : cloud.xyz[i].z);
        if (v >= vmin && v <= vmax) {
            result.xyz.push_back(cloud.xyz[i]);
        }
    }
    PortCloud pc; pc.cloud = result; out->clouds.push_back(pc);
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("points", static_cast<int>(result.xyz.size()));
    char msg[64]; sprintf_s(msg, "点数 %d", static_cast<int>(result.xyz.size()));
    out->message = msg;
    return HS_OK;
#endif
}
}