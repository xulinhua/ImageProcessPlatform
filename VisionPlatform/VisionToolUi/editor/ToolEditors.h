/**
 * @file ToolEditors.h
 * @brief ToolEditors头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLEDITORS_H
#define VISION_TOOLEDITORS_H
#include "VisionToolUiExport.h"

#include "ToolEditorBase.h"
#include <string>

namespace Vision {

/** @brief 按工具壳参数说明生成参数页 */
VISION_TOOLUI_API ToolEditorBase* createGenericEditor(QWidget* parent);

/** @brief 滤波和斑点的多步骤参数页 */
VISION_TOOLUI_API ToolEditorBase* createStepEditor(const std::string& toolName, QWidget* parent);

}

#endif
