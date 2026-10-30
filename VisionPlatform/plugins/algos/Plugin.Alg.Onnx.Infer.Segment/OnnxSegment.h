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

/**
 * @class OnnxSegment
 * @brief 语义分割。没有 ONNX Runtime 时说明未接入
 */
class OnnxSegment : public AlgToolBase {
public:
    /** @brief 挂到 infer.segment 工具壳 */
    std::string toolName() const { return "infer.segment"; }
    /** @brief 算法库为 ONNX */
    AlgLibKind algLib() const { return AlgLibOnnx; }
    /** @brief 算法版本号 */
    std::string version() const { return std::string("1.0.0.0"); }
    /** @brief 版本变更记录 */
    std::vector<VerNote> versionNotes() const;
    /**
     * @brief 校验输入后返回失败：当前未接 ONNX Runtime，不产出分割结果
     * @note 读 run 里的 device（只认空或 cpu）、width/height（网络输入尺寸，缺省 512）
     * @return 总是返回错误码，原因写在 out->message
     */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    /** @brief 新建一个同类实例 */
    AlgToolBase* clone() const { return new OnnxSegment(); }
};

}

#endif
