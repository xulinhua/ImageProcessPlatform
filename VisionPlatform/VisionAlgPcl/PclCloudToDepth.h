/**
 * @file PclCloudToDepth.h
 * @brief 点云投影成深度图（PCL / 针孔模型）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PCLCLOUDTODEPTH_H
#define VISION_PCLCLOUDTODEPTH_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaCloudToDepth.hpp"
#include "PclCloudOps.h"
#include <HsCommData/HsCloud.h>
#include <HsCommData/HsImage.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class PclCloudToDepth
 * @brief 按内参把点云投影成 real32 深度图
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class PclCloudToDepth : public AlgToolBase {
public:
    /** @brief 点云置空 */
    PclCloudToDepth();
    /** @brief 析构 */
    ~PclCloudToDepth() override;

    /** @brief 克隆本实例 */
    AlgToolBase* clone() const override { return new PclCloudToDepth(); }

    /** @brief 注册名 */
    std::string tool_name() const override { return "prepare3d.cloud_to_depth"; }
    /** @brief 算法库 */
    AlgLibKind alg_lib() const override { return AlgLibPcl; }
    /** @brief 工具版本 */
    std::string version() const override { return "1.0.0.0"; }
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> version_notes() const override;

    /** @brief 点云投影成深度图 */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;

    /** @brief 转到 tool_name */
    std::string toolName() const override { return tool_name(); }
    /** @brief 转到 alg_lib：PCL */
    AlgLibKind algLib() const override { return alg_lib(); }
    /** @brief 转到 version_notes */
    std::vector<VerNote> versionNotes() const override { return version_notes(); }

private:
    /** @brief 清空本拍点云、结果与错误码 */
    int tool_reset();
    /**
     * @brief 取点云与内参、输出尺寸
     * @return 无点云或参数类型不对时返回对应错误码
     */
    int proc_input(const AlgToolIn& in);
    /** @brief 按针孔模型投影 */
    int alg_run(const AlgToolIn& in);
    /** @brief 有效深度像素等测值做公差，结果留在 tol_ */
    int judge_tol(const AlgToolIn& in) override;
    /** @brief 写出深度图与测值 */
    int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

    PclCloudOps ops_; ///< 点云互转与 IO
    HsCommData::Cloud3 cloud_; ///< 本拍输入点云
    AlgToolOut result_; ///< 本拍算法结果缓冲
    TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
    int shot_err_; ///< 本拍错误码。0 为成功
    std::string shot_msg_; ///< 本拍说明
    ToolParaCloudToDepth para_; ///< 本拍运行参数快照
};

}

#endif