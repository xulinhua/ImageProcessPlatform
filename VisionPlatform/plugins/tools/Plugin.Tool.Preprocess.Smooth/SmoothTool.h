/**
 * @file SmoothTool.h
 * @brief SmoothTool头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SMOOTHTOOL_H
#define VISION_SMOOTHTOOL_H

#include "ToolBase.h"
#include "ToolParaSmooth.hpp"

namespace Vision {

/**
 * @class SmoothTool
 * @brief 图像平滑：登记参数，执行交给算法插件
 */
class SmoothTool : public ToolBase {
public:
    /** @brief 注册名 */
    std::string toolName() const;
    /** @brief 工具大类 */
    ToolType type() const;
    /** @brief 界面显示名 */
    std::string displayName() const;
    /** @brief 工具版本 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 工具说明 */
    std::string description() const;
    /** @brief 缺省参数 */
    ParamMap defaultParams() const;
    /** @brief 参数说明 */
    std::vector<ParamField> paramFields() const;
    /** @brief 画布 ROI 等能力 */
    ToolCaps capabilities() const;
    /** @brief 调算法插件执行 */
    ToolOut execute(const ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;
    const ToolParaBase* toolParam() const;
private:
    ToolParaSmooth m_param;
};

}

#endif