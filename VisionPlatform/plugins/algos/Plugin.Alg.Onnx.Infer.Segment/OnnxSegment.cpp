/**
 * @file OnnxSegment.cpp
 * @brief ONNX 语义分割
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "OnnxSegment.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {

namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "语义分割" } };
}

std::vector<VerNote> OnnxSegment::versionNotes() const
{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

int OnnxSegment::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) {
        return HS_ERR_NULL;
    }
    *out = AlgToolOut();
    if (inputImage(in).isNull()) {
        return fail(HS_ERR_OPERATOR, "没有图像", out);
    }
    const std::string device = in.run.value("device").toString();
    if (!device.empty() && device != "cpu") {
        return fail(HS_ERR_OPERATOR, "只支持 cpu", out);
    }
    if (in.run.value("width").toInt(512) < 1 || in.run.value("height").toInt(512) < 1) {
        return fail(HS_ERR_OPERATOR, "网络输入尺寸无效", out);
    }
    return fail(HS_ERR_OPERATOR, "ONNX Runtime 未接入", out);
}

}
