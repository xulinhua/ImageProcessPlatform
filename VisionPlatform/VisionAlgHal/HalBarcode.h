/**
 * @file HalBarcode.h
 * @brief 一维码识别（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONBARCODE_H
#define VISION_HALCONBARCODE_H

#include "AlgToolBase.h"
#include "HalconCpp.h"
#include <string>

namespace Vision {

/**
 * @class HalBarcode
 * @brief 在搜索区里读一维码，写出文本、码区和位姿（Halcon）
 */
class HalBarcode : public AlgToolBase {
public:
    /** @brief 挂到 identify.barcode 工具壳 */
    std::string toolName() const { return "identify.barcode"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 在搜索区里读码。不留本拍状态，可重入 */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalBarcode(); }

private:
    /** @brief 一维码模型句柄。出作用域时释放 */
    struct Model {
        HalconCpp::HTuple id; ///< 模型句柄
        bool open;            ///< 已创建
        /** @brief 未创建状态 */
        Model() : open(false) {}
        /** @brief 已创建时清除条码模型，异常吞掉 */
        ~Model();
    };
};

}

#endif
