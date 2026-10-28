/**
 * @file TmplTool.h
 * @brief 模板定位：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TEMPLATETOOL_H
#define VISION_TEMPLATETOOL_H

#include "ToolBase.h"
#include "AlgToolBase.h"
#include "ToolParaTmpl.hpp"
#include <HsCommData/HsImage.h>

namespace Vision {

/**
 * @class TmplTool
 * @brief 模板定位：登记参数，执行交给算法插件
 */
class TmplTool : public ToolBase {
public: // 骨架
    /** @brief 构造。示教原图标记为空 */
    TmplTool();
    /** @brief 析构 */
    ~TmplTool() override;
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
    /** @brief 示教参数说明 */
    std::vector<ParamField> cfg_teach() const override;
    /** @brief 本工具参数对象 */
    const ToolParaBase* tool_param() const override;
    /** @brief load 之后核对建模方式和键表 */
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

public: // 执行、保存
    /** @brief 调算法插件执行 */
    int execute(const ToolIn& in, ToolOut* out) override;
    /** @brief 调试动作 */
    std::vector<ToolAction> cfg_action() const override;
    /** @brief 按动作 id 执行调试动作 */
    int action(const std::string& id, const ToolIn& in, ToolOut* out) override;
    /** @brief 保存原图，再让算法保存本库模型。返回错误码 */
    int save() override;
    /** @brief 丢掉示教原图并清掉本工具数据目录 */
    void clear(const std::string& toolDataDir) override;

private: // 输入
    /** @brief 把每个定位结果写入日志 */
    void log_result(ToolOut* out) const override;
    /** @brief 在结果位置补十字 */
    void disp_extra(const ToolIn& in, ToolOut* out) const override;

private: // 调试动作。只由 action 调用
    /** @brief 学习模板。错误码返回，结果写入 out */
    int teach_model(const ToolIn& in, ToolOut* out);
    /** @brief 查看轮廓。错误码返回，结果写入 out */
    int inspect_contour(const ToolIn& in, ToolOut* out);
    /** @brief 示教成功后留下整图，点保存时写入工具目录 */
    void keep_org_image(const ToolIn& in);
    /** @brief 挂上算法、装参数、跑一拍或一个动作 */
    int shot(const ToolIn& in, const std::string& actionId, ToolOut* out);
    /** @brief 把本拍图像和分区收成算法输入 */
    void fill_alg_in(const ToolIn& in, AlgToolIn* ain) const;

private: // 参数
    ToolParaTmpl param_; ///< 本工具参数对象

private: // 示教原图。点保存时写入工具目录
    HsCommData::HsImage image_org_; ///< 示教时的整图
    bool image_org_ok_ = false; ///< 已有可保存的原图
    std::string data_dir_; ///< 原图所在的工具数据目录

private: // 基类仍用这套名字，都转到上面的接口
    std::string toolName() const override { return tool_name(); }
    std::string displayName() const override { return display_name(); }
    std::vector<VerNote> versionNotes() const override { return version_notes(); }
    ParamMap defaultParams() const override { return default_params(); }
    std::vector<ParamField> paramFields() const override;
    ToolCaps capabilities() const override { return cfg_roi(); }
    const ToolParaBase* toolParam() const override { return tool_param(); }
    ToolOut execute(const ToolIn& in) override;
    ToolOut teach(const ToolIn& in) override;
};

}

#endif
