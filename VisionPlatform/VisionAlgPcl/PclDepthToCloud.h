/**
 * @file PclDepthToCloud.h
 * @brief 深度图转点云（PCL / 针孔模型）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PCLDEPTHTOCLOUD_H
#define VISION_PCLDEPTHTOCLOUD_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaDepthToCloud.hpp"
#include "PclCloudOps.h"
#include <HsCommData/HsCloud.h>
#include <HsCommData/HsImage.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class PclDepthToCloud
 * @brief 深度图或有序深度点云按内参反投影成点云
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class PclDepthToCloud : public AlgToolBase {
public:
    /** @brief 图像与点云置空 */
    PclDepthToCloud();
    /** @brief 析构 */
    ~PclDepthToCloud() override;

    /** @brief 克隆本实例 */
    AlgToolBase* clone() const override { return new PclDepthToCloud(); }

    /** @brief 注册名 */
    std::string tool_name() const override { return "prepare3d.depth_to_cloud"; }
    /** @brief 算法库 */
    AlgLibKind alg_lib() const override { return AlgLibPcl; }
    /** @brief 工具版本 */
    std::string version() const override { return "1.0.0.0"; }
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> version_notes() const override;

    /** @brief 深度图或有序深度点云转点云 */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;

    /** @brief 转到 tool_name */
    std::string toolName() const override { return tool_name(); }
    /** @brief 转到 alg_lib：PCL */
    AlgLibKind algLib() const override { return alg_lib(); }
    /** @brief 转到 version_notes */
    std::vector<VerNote> versionNotes() const override { return version_notes(); }

private:
    /** @brief 清空本拍输入、结果与错误码 */
    int tool_reset();
    /**
     * @brief 取深度图或有序点云与内参
     * @return 无深度图/点云或参数类型不对时返回对应错误码
     */
    int proc_input(const AlgToolIn& in);
    /** @brief 按针孔模型反投影 */
    int alg_run(const AlgToolIn& in);
    /** @brief 点数等测值做公差，结果留在 tol_ */
    int judge_tol(const AlgToolIn& in) override;
    /** @brief 写出点云与测值 */
    int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

    PclCloudOps ops_; ///< 点云互转与 IO
    HsCommData::Cloud3 cloud_; ///< 有序深度点云备选
    HsCommData::HsImage image_; ///< 深度图
    bool has_image_; ///< 本拍以深度图为输入
    AlgToolOut result_; ///< 本拍算法结果缓冲
    TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
    int shot_err_; ///< 本拍错误码。0 为成功
    std::string shot_msg_; ///< 本拍说明
    ToolParaDepthToCloud para_; ///< 本拍运行参数快照
};

}

#endif