/**
 * @file OnnxDetect.h
 * @brief ONNX 目标检测
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ONNXDETECT_H
#define VISION_ONNXDETECT_H

#include "AlgToolBase.h"

namespace Vision {

/** @class OnnxDetect @brief 检测推理。没有 ONNX Runtime 时说明未接入 */
class OnnxDetect : public AlgToolBase {
public:
    std::string toolName() const { return "infer.detect"; }
    AlgLibKind algLib() const { return AlgLibOnnx; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new OnnxDetect(); }
};

}

#endif
