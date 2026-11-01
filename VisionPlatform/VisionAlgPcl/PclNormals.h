/**
 * @file PclNormals.h
 * @brief 估计点云法线（PCL）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PCLNORMALS_H
#define VISION_PCLNORMALS_H

#include "AlgToolBase.h"
#include "TolShot.h"
#include "ToolParaNormals.hpp"
#include "PclCloudOps.h"
#include <HsCommData/HsCloud.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class PclNormals
 * @brief 估计点云法线（PCL）
 * @note 单拍状态存在成员里，同一实例不可并发 execute
 */
class PclNormals : public AlgToolBase {
public:
    /** @brief 点云与测值置空 */
    PclNormals();
    /** @brief 析构 */
    ~PclNormals() override;

    /** @brief 克隆本实例 */
    AlgToolBase* clone() const override { return new PclNormals(); }

    /** @brief 注册名 */
    std::string tool_name() const override { return "prepare3d.normals"; }
    /** @brief 算法库 */
    AlgLibKind alg_lib() const override { return AlgLibPcl; }
    /** @brief 工具版本 */
    std::string version() const override { return "1.0.0.0"; }
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> version_notes() const override;

    /** @brief 估计点云法线 */
    int execute(const AlgToolIn& in, AlgToolOut* out) override;

    /** @brief 转到 tool_name */
    std::string toolName() const override { return tool_name(); }
    /** @brief 转到 alg_lib：PCL */
    AlgLibKind algLib() const override { return alg_lib(); }
    /** @brief 转到 version_notes */
    std::vector<VerNote> versionNotes() const override { return version_notes(); }

private:
    /** @brief 清空本拍点云、测值与错误码 */
    int tool_reset();
    /**
     * @brief 取输入点云与运行参数
     * @return 无点云或参数类型不对时返回对应错误码
     */
    int proc_input(const AlgToolIn& in);
    /** @brief 执行 估计点云法线 */
    int alg_run(const AlgToolIn& in);
    /** @brief 按本工具测值做公差，结果留在 tol_ */
    int judge_tol(const AlgToolIn& in) override;
    /** @brief 把结果点云/几何与测值写进 out */
    int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);

    PclCloudOps ops_; ///< 点云互转与 IO
    HsCommData::Cloud3 cloud_; ///< 本拍主输入点云
    HsCommData::Cloud3 cloud2_; ///< 第二路输入点云（可选）
    AlgToolOut result_; ///< 本拍算法结果缓冲
    TolShot tol_; ///< 本拍测值和公差。写出前不进 AlgToolOut
    int shot_err_; ///< 本拍错误码。0 为成功
    std::string shot_msg_; ///< 本拍说明
    ToolParaNormals para_; ///< 本拍运行参数快照
};

}

#endif