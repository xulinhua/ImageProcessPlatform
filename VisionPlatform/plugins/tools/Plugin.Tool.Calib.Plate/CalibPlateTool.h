/**
 * @file CalibPlateTool.h
 * @brief CalibPlateTool头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CALIBPLATETOOL_H
#define VISION_CALIBPLATETOOL_H

#include "ToolBase.h"
#include "ToolParaCalibPlate.hpp"

namespace Vision {

/**
 * @class CalibPlateTool
 * @brief 标定板。学习时导入已算好的相机模型
 */
class CalibPlateTool : public ToolBase {
public:
    /** @brief 注册名 */
    std::string toolName() const;
    /** @brief 工具大类 */
    ToolType type() const;
    /** @brief 界面显示名 */
    std::string displayName() const;
    /** @brief 工具说明 */
    std::string description() const;
    /** @brief 工具版本 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 缺省参数 */
    ParamMap defaultParams() const;
    /** @brief 参数说明 */
    std::vector<ParamField> paramFields() const;
    /** @brief 画布能力 */
    ToolCaps capabilities() const;
    /** @brief 端口 */
    std::vector<PortDesc> ports() const;
    /** @brief 执行 */
    ToolOut execute(const ToolIn& in);
    /** @brief 克隆本实例 */
    ToolBase* clone() const;
    const ToolParaBase* toolParam() const;
private:
    ToolParaCalibPlate m_param;
};

}

#endif
