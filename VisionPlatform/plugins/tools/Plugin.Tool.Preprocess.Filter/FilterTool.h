/**
 * @file FilterTool.h
 * @brief 图像滤波：多步骤增强、平滑和灰度形态学
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_FILTERTOOL_H
#define VISION_FILTERTOOL_H

#include "ToolBase.h"
#include "ToolParaFilter.hpp"

namespace Vision {

/**
 * @class FilterTool
 * @brief 图像滤波：登记参数，执行交给算法插件
 */
class FilterTool : public ToolBase {
public:
    /************************************************** 身份 **************************************************/
    /** @brief 注册名 preprocess.filter */
    std::string toolName() const;
    /** @brief 工具大类：预处理 */
    ToolType type() const;
    /** @brief 界面显示名 */
    std::string displayName() const;
    /** @brief 工具版本 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 工具说明 */
    std::string description() const;

    /************************************************** 参数 **************************************************/
    /** @brief 由参数说明生成的缺省参数 */
    ParamMap defaultParams() const;
    /** @brief 参数说明：步骤数加第 0 步字段 */
    std::vector<ParamField> paramFields() const;
    /** @brief 画布能力：roi 槽最多 50 块，可画框/圆/多边形或引用区域 */
    ToolCaps capabilities() const;

    /************************************************** 执行 **************************************************/
    /** @brief 调算法插件执行，没有输入图也没有结果图时判 NG */
    ToolOut execute(const ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;
    /** @brief 本工具参数对象，供参数版本适配与 load 使用 */
    const ToolParaBase* toolParam() const;
private:
    ToolParaFilter m_param; ///< 参数快照，ToolBase::adaptParam 时从 ToolParam 载入
};

}

#endif
