/**
 * @file OnnxClassify.h
 * @brief ONNX 图像分类
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ONNXCLASSIFY_H
#define VISION_ONNXCLASSIFY_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class OnnxClassify
 * @brief 分类推理。本机没有 ONNX Runtime 时说明未接入，不假装已经跑过模型
 */
class OnnxClassify : public AlgToolBase {
public:
    std::string toolName() const { return "infer.classify"; }
    AlgLibKind algLib() const { return AlgLibOnnx; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new OnnxClassify(); }
};

}

#endif
