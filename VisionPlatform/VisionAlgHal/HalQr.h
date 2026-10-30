/**
 * @file HalQr.h
 * @brief 二维码识别（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONQR_H
#define VISION_HALCONQR_H

#include "AlgToolBase.h"
#include "HalconCpp.h"
#include <string>

namespace Vision {

/**
 * @class HalQr
 * @brief 在搜索区里读二维码，写出文本、码区和位姿（Halcon）
 */
class HalQr : public AlgToolBase {
public:
    /** @brief 挂到 identify.qr 工具壳 */
    std::string toolName() const { return "identify.qr"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 在搜索区里读码。不留本拍状态，可重入 */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalQr(); }

private:
    /** @brief 二维码模型句柄。出作用域时释放 */
    struct Model {
        HalconCpp::HTuple id; ///< 模型句柄
        bool open;            ///< 已创建
        /** @brief 未创建状态 */
        Model() : open(false) {}
        /** @brief 已创建时清除二维码模型，异常吞掉 */
        ~Model();
    };
};

}

#endif
