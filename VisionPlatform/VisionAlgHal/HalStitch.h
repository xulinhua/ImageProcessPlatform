/**
 * @file HalStitch.h
 * @brief 把多张图拼成一张（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONSTITCH_H
#define VISION_HALCONSTITCH_H

#include "AlgToolBase.h"
#include "HalconCpp.h"
#include <vector>

namespace Vision {

/**
 * @class HalStitch
 * @brief 把上游多张图按行列拼成一张（Halcon）
 */
class HalStitch : public AlgToolBase {
public:
    /** @brief 挂到 preprocess.stitch 工具壳 */
    std::string toolName() const { return "preprocess.stitch"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 按列数把多张图拼成一张。不留本拍状态，可重入 */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalStitch(); }

private:
    /** @brief 缺省图加其它图，转成 Halcon 图。转换失败返回 false */
    bool collect(const AlgToolIn& in, std::vector<HalconCpp::HObject>* images, std::string* err) const;
    /** @brief 通道数不一致时把多通道图转灰度 */
    void unifyChannels(std::vector<HalconCpp::HObject>* images) const;
};

}

#endif
