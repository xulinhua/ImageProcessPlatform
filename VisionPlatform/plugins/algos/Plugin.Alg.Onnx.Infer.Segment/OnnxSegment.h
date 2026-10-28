/**
 * @file OnnxSegment.h
 * @brief ONNX 语义分割
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ONNXSEGMENT_H
#define VISION_ONNXSEGMENT_H

#include "AlgToolBase.h"

namespace Vision {

/** @class OnnxSegment @brief 语义分割。没有 ONNX Runtime 时说明未接入 */
class OnnxSegment : public AlgToolBase {
public:
    std::string toolName() const { return "infer.segment"; }
    AlgLibKind algLib() const { return AlgLibOnnx; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new OnnxSegment(); }
};

}

#endif
