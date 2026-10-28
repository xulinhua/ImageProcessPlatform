/**
 * @file OnnxClassify.cpp
 * @brief ONNX 图像分类
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "OnnxClassify.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {

namespace {

const VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "图像分类" },
};

}

std::vector<VerNote> OnnxClassify::versionNotes() const
{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

int OnnxClassify::execute(const AlgToolIn& in, AlgToolOut* out)
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
    const int width = in.run.value("width").toInt(224);
    const int height = in.run.value("height").toInt(224);
    if (width < 1 || height < 1) {
        return fail(HS_ERR_OPERATOR, "网络输入尺寸无效", out);
    }
#if defined(__has_include)
#  if __has_include(<onnxruntime_cxx_api.h>)
#    define VISION_HAS_ONNX 1
#  endif
#endif
#if defined(VISION_HAS_ONNX)
    return fail(HS_ERR_OPERATOR, "ONNX 分类尚未接到会话", out);
#else
    (void)in;
    return fail(HS_ERR_OPERATOR, "ONNX Runtime 未接入", out);
#endif
}

}
