/**
 * @file HalOcr.h
 * @brief 字符识别（Halcon）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONOCR_H
#define VISION_HALCONOCR_H

#include "AlgToolBase.h"
#include "HalconCpp.h"

namespace Vision {

/**
 * @class HalOcr
 * @brief 用 Halcon 自动读字器在搜索区里读字符，写出文本、字符区和位姿（Halcon）
 */
class HalOcr : public AlgToolBase {
public:
    /** @brief 挂到 identify.ocr 工具壳 */
    std::string toolName() const { return "identify.ocr"; }
    /** @brief 算法库：Halcon */
    AlgLibKind algLib() const { return AlgLibHal; }
    /** @brief 算法版本号 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 读字符。不留本拍状态，可重入 */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    /** @brief 克隆一份新实例 */
    AlgToolBase* clone() const { return new HalOcr(); }

private:
    /** @brief 读字模型句柄。出作用域时释放 */
    struct Model {
        HalconCpp::HTuple id; ///< 模型句柄
        bool open;            ///< 已创建
        /** @brief 未创建状态 */
        Model() : open(false) {}
        /** @brief 已创建时清除读字模型，异常吞掉 */
        ~Model();
    };
    /** @brief 读字结果句柄。出作用域时释放 */
    struct Result {
        HalconCpp::HTuple id; ///< 结果句柄
        bool open;            ///< 已创建
        /** @brief 未创建状态 */
        Result() : open(false) {}
        /** @brief 已创建时清除读字结果，异常吞掉 */
        ~Result();
    };
};

}

#endif
