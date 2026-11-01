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
#include "TolShot.h"
#include "ToolParaStitch.hpp"
#include "HalconCpp.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class HalStitch
 * @brief 把上游多张图按行列拼成一张（Halcon）
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class HalStitch : public AlgToolBase {
public:
    /** @brief 区域置空 */
    HalStitch();

    /** @brief 克隆本实例 */
    AlgToolBase* clone() const override;

public:
    /** @brief 注册名 */
    std::string tool_name() const override;
    /** @brief 算法库 */
    AlgLibKind alg_lib() const override;
    /** @brief 工具版本 */
    std::string version() const override;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> version_notes() const override;

public:
    /** @brief 按列数把多张图拼成一张 */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;

private:
    /** @brief 清空本拍拼图缓冲、公差和错误码 */
    int tool_reset();
    /** @brief 收图、校验参数与张数 */
    int proc_input(const AlgToolIn& in);
    /** @brief TileImagesOffset 拼成一张 */
    int alg_run(const AlgToolIn& in);
    /** @brief 张数、列数写入 tol_ 并按 judge 表判定 */
    int judge_tol(const AlgToolIn& in) override;
    /** @brief 写出拼图和测值 */
    int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

    /** @brief 缺省图与其余 in.images 转成 Halcon 图 */
    int collect_images(const AlgToolIn& in);
    /** @brief 通道数不一致时把多通道图转灰度 */
    void unify_channels();

    std::vector<HalconCpp::HObject> tiles_; ///< 参与拼接的 Halcon 图
    std::vector<int> widths_;                ///< 各图宽
    std::vector<int> heights_;               ///< 各图高
    HalconCpp::HObject tiled_;               ///< 拼图结果
    int count_;                              ///< 图张数
    int columns_;                            ///< 每行列数
    int64_t frame_id_;                       ///< 输出图 frameId
    TolShot tol_;                            ///< 本拍测值和公差。写出前不进 AlgToolOut
    int shot_err_;                           ///< 本拍错误码。0 为成功
    std::string shot_msg_;                   ///< 本拍说明

    /** @brief 转到 tool_name */
    std::string toolName() const override { return tool_name(); }
    /** @brief 转到 alg_lib：Halcon */
    AlgLibKind algLib() const override { return alg_lib(); }
    /** @brief 转到 version_notes */
    std::vector<VerNote> versionNotes() const override { return version_notes(); }
};

}

#endif
