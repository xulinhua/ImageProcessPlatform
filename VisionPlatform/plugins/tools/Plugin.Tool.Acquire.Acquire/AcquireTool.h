/**
 * @file AcquireTool.h
 * @brief AcquireTool头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ACQUIRETOOL_H
#define VISION_ACQUIRETOOL_H

#include "ToolBase.h"
#include "ToolParaAcquire.hpp"

namespace Vision {

/**
 * @class AcquireTool
 * @brief 从已配置的相机、文件夹或图像模块取图
 */
class AcquireTool : public ToolBase {
public:
    /** @brief 注册名 */
    std::string toolName() const;
    /** @brief 工具大类 */
    Vision::ToolType type() const;
    /** @brief 界面显示名 */
    std::string displayName() const;
    /** @brief 工具版本 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 工具说明 */
    std::string description() const;
    /** @brief 参数说明 */
    std::vector<Vision::ParamField> paramFields() const;
    /** @brief 按文件、图像列表或文件夹取图 */
    Vision::ToolOut execute(const Vision::ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;
    const ToolParaBase* toolParam() const;
private:
    ToolParaAcquire m_param;
};

}

#endif
