/**
 * @file ToolLibrary.h
 * @brief ToolLibrary头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLLIBRARY_H
#define VISION_TOOLLIBRARY_H
#include "VisionUiExport.h"

class QTreeWidget;

namespace Vision {

/**
 * @brief 按 ToolType 分组填充已注册的具体工具。父节点是类型，子节点是 toolName
 * @param withFlowStructure 是否附带并行/If 等流程结构节点
 */
VISION_UI_API void fillRegisteredToolTree(QTreeWidget* lib, bool withFlowStructure);

}

#endif
