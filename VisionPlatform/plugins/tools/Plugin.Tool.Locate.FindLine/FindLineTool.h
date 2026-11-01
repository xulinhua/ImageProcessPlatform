/**
 * @file FindLineTool.h
 * @brief 找线工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_FINDLINETOOL_H
#define VISION_FINDLINETOOL_H

#include "ToolBase.h"
#include "AlgToolBase.h"
#include "ToolParaFindLine.hpp"

namespace Vision {

/**
 * @class FindLineTool
 * @brief 找线：登记参数，执行交给算法插件
 */
class FindLineTool : public ToolBase {
public: // 骨架
    /** @brief 构造 */
    FindLineTool();
    /** @brief 析构 */
    ~FindLineTool() override;
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const override;

public: // 描述、版本
    /** @brief 注册名 */
    std::string tool_name() const override;
    /** @brief 工具大类 */
    ToolType type() const override;
    /** @brief 界面显示名 */
    std::string display_name() const override;
    /** @brief 工具说明 */
    std::string description() const override;
    /** @brief 工具版本 */
    std::string version() const override;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> version_notes() const override;

public: // 参数
    /** @brief 缺省参数 */
    ParamMap default_params() const override;
    /** @brief 运行参数说明 */
    std::vector<ParamField> cfg_param() const override;
    /** @brief 本工具参数对象 */
    const ToolParaBase* tool_param() const override;
    /** @brief load 之后核对参数 */
    int check_param(std::string* detail) const override;

public: // 输出、显示
    /** @brief 输出项 */
    OutputSpec cfg_output() const override;
    /** @brief 统计规格 */
    TolSpec cfg_tol() const override;
    /** @brief 显示规格 */
    DisplaySpec cfg_display() const override;
    /** @brief 画布 ROI 等能力 */
    ToolCaps cfg_roi() const override;

public: // 执行
    /** @brief 调算法插件执行 */
    int execute(const ToolIn& in, ToolOut* out) override;

private: // 执行
    /** @brief 挂上算法、装参数、跑一拍 */
    int shot(const ToolIn& in, const std::string& actionId, ToolOut* out);
    /** @brief 把本拍图像和分区收成算法输入 */
    void fill_alg_in(const ToolIn& in, AlgToolIn* ain) const;

private: // 参数
    ToolParaFindLine param_; ///< 本工具参数对象

private: // 基类仍用这套名字，都转到上面的接口
    /** @brief 转到 tool_name */
    std::string toolName() const override { return tool_name(); }
    /** @brief 转到 display_name */
    std::string displayName() const override { return display_name(); }
    /** @brief 转到 version_notes */
    std::vector<VerNote> versionNotes() const override { return version_notes(); }
    /** @brief 转到 default_params */
    ParamMap defaultParams() const override { return default_params(); }
    /** @brief 转到 cfg_param */
    std::vector<ParamField> paramFields() const override;
    /** @brief 转到 cfg_roi */
    ToolCaps capabilities() const override { return cfg_roi(); }
    /** @brief 转到 tool_param */
    const ToolParaBase* toolParam() const override { return tool_param(); }
    /** @brief 调算法插件执行，返回 ToolOut */
    ToolOut execute(const ToolIn& in) override;
};

}

#endif
