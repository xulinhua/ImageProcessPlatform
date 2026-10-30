#include "Open3dCloudToDepth.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsImage.h>
#include <cmath>
#include <stdio.h>
#include <vector>
#include <limits>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D 点云转深度图" } };
}
std::vector<VerNote> Open3dCloudToDepth::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int Open3dCloudToDepth::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
    if (in.clouds.empty() || in.clouds[0].cloud.isEmpty()) {
        return fail(HS_ERR_OPERATOR, "点太少或退化", out);
    }
    const double fx = in.run.value("fx").toDouble(500.0);
    const double fy = in.run.value("fy").toDouble(500.0);
    const double cx = in.run.value("cx").toDouble(320.0);
    const double cy = in.run.value("cy").toDouble(240.0);
    int width = in.run.value("width").toInt(640);
    int height = in.run.value("height").toInt(480);
    if (width < 1 || height < 1 || fx == 0.0 || fy == 0.0) {
        return fail(HS_ERR_OPERATOR, "图像尺寸或内参无效", out);
    }
    HsCommData::HsImage img = HsCommData::HsImage::make(width, height, HsCommData::PixelReal32, HsCommData::EncodeMono, 0);
    std::vector<float> depth(static_cast<size_t>(width * height), 0.0f);
    const HsCommData::Cloud3& cloud = in.clouds[0].cloud;
    int count = 0;
    for (size_t i = 0; i < cloud.xyz.size(); ++i) {
        const HsCommData::Point3d& p = cloud.xyz[i];
        if (p.z <= 1e-9) { continue; }
        const int u = static_cast<int>(std::floor(fx * p.x / p.z + cx + 0.5));
        const int v = static_cast<int>(std::floor(fy * p.y / p.z + cy + 0.5));
        if (u < 0 || v < 0 || u >= width || v >= height) { continue; }
        const size_t idx = static_cast<size_t>(v * width + u);
        const float z = static_cast<float>(p.z);
        if (depth[idx] <= 0.0f || z < depth[idx]) {
            depth[idx] = z;
            ++count;
        }
    }
    for (int y = 0; y < height; ++y) {
        float* row = reinterpret_cast<float*>(img.scanLine(y));
        for (int x = 0; x < width; ++x) {
            row[x] = depth[static_cast<size_t>(y * width + x)];
        }
    }
    out->image.image = img;
    out->ok = true; out->errCode = HS_OK;
    out->values.insert("points", count);
    char msg[64]; sprintf_s(msg, "有效像素 %d", count);
    out->message = msg;
    return HS_OK;
}
}