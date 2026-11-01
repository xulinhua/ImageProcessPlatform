/**
 * @file RegionGrowTool.h
 * @brief 法向平滑的区域生长分割
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REGIONGROWTOOL_H
#define VISION_REGIONGROWTOOL_H

#include "ToolBase.h"
#include "AlgToolBase.h"
#include "ToolParaRegionGrow.hpp"

namespace Vision {

/**
 * @class RegionGrowTool
 * @brief 区域生长：登记参数，执行交给 PCL/Open3D 算法插件
 */
class RegionGrowTool : public ToolBase {
public:
    /** @brief 构造 */
    RegionGrowTool();
    /** @brief 析构 */
    ~RegionGrowTool() override;
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const override;
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
    /** @brief 缺省参数 */
    ParamMap default_params() const override;
    /** @brief 运行参数说明 */
    std::vector<ParamField> cfg_param() const override;
    /** @brief 本工具参数对象 */
    const ToolParaBase* tool_param() const override;
    /** @brief load 之后核对 */
    int check_param(std::string* detail) const override;
    /** @brief 输出项 */
    OutputSpec cfg_output() const override;
    /** @brief 公差项 */
    TolSpec cfg_tol() const override;
    /** @brief 显示项 */
    DisplaySpec cfg_display() const override;
    /** @brief 画布 ROI */
    ToolCaps cfg_roi() const override;
    /** @brief 端口说明 */
    std::vector<PortDesc> ports() const override;
    /** @brief 调算法插件执行 */
    int execute(const ToolIn& in, ToolOut* out) override;
private:
    /** @brief 写运行日志 */
    void log_result(ToolOut* out) const override;
    /** @brief 挂上算法、装参数、跑一拍或一个动作 */
    int shot(const ToolIn& in, ToolOut* out);
    /** @brief 把本拍图像/点云和分区收成算法输入 */
    void fill_alg_in(const ToolIn& in, AlgToolIn* ain) const;
    ToolParaRegionGrow param_; ///< 本工具参数对象
    std::string toolName() const override { return tool_name(); }
    /** @brief 转到 display_name */
    std::string displayName() const override { return display_name(); }
    /** @brief 转到 version_notes */
    std::vector<VerNote> versionNotes() const override { return version_notes(); }
    /** @brief 转到 default_params */
    ParamMap defaultParams() const override { return default_params(); }
    /** @brief 转到 cfg_param */
    std::vector<ParamField> paramFields() const override { return cfg_param(); }
    /** @brief 转到 cfg_roi */
    ToolCaps capabilities() const override { return cfg_roi(); }
    /** @brief 转到 tool_param */
    const ToolParaBase* toolParam() const override { return tool_param(); }
    /** @brief 调算法插件执行，返回 ToolOut */
    ToolOut execute(const ToolIn& in) override;
};

}
#endif
