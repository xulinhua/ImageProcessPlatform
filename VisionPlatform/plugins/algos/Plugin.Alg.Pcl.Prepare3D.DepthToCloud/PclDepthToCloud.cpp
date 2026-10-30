#include "PclDepthToCloud.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "深度图转点云" } };
}
std::vector<VerNote> PclDepthToCloud::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int PclDepthToCloud::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
    const HsCommData::HsImage& img = inputImage(in);
    if (img.isNull()) {
        return fail(HS_ERR_OPERATOR, "没有深度图", out);
    }
    const double fx = in.run.value("fx").toDouble(500.0);
    const double fy = in.run.value("fy").toDouble(500.0);
    const double cx = in.run.value("cx").toDouble(320.0);
    const double cy = in.run.value("cy").toDouble(240.0);
    double scale = in.run.value("scale").toDouble(1.0);
    if (scale == 0.0) { scale = 1.0; }
    if (fx == 0.0 || fy == 0.0) {
        return fail(HS_ERR_OPERATOR, "相机内参无效", out);
    }
    HsCommData::Cloud3 result;
    result.xyz.reserve(static_cast<size_t>(img.width * img.height / 4));
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            double z = 0.0;
            if (!img.sampleGray(x, y, &z)) {
                continue;
            }
            z *= scale;
            if (z <= 0.0) {
                continue;
            }
            const double X = (static_cast<double>(x) - cx) * z / fx;
            const double Y = (static_cast<double>(y) - cy) * z / fy;
            result.xyz.push_back(HsCommData::Point3d(X, Y, z));
        }
    }
    if (result.xyz.empty()) {
        return fail(HS_ERR_OPERATOR, "没有有效深度点", out);
    }
    PortCloud pc; pc.cloud = result; out->clouds.push_back(pc);
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("points", static_cast<int>(result.xyz.size()));
    char msg[64]; sprintf_s(msg, "点数 %d", static_cast<int>(result.xyz.size()));
    out->message = msg;
    return HS_OK;
}
}