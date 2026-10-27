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
    std::string toolName() const;
    ToolType type() const;
    std::string displayName() const;
    std::string version() const;
    std::vector<VerNote> versionNotes() const;
    std::string description() const;
    ParamMap defaultParams() const;
    std::vector<ParamField> paramFields() const;
    ToolCaps capabilities() const;
    ToolOut execute(const ToolIn& in);
    ToolBase* clone() const;
    const ToolParaBase* toolParam() const;
private:
    ToolParaFilter m_param;
};

}

#endif
