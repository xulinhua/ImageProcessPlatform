/**
 * @file ToolTreeIcons.h
 * @brief ToolTreeIcons头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLTREEICONS_H
#define VISION_TOOLTREEICONS_H
#include "VisionUiExport.h"

#include "ToolEnums.h"
#include <QIcon>
#include <QString>

namespace Vision {

/**
 * @brief 本流程工具树图标键（与 icons/tools/*.svg 文件名对应）
 */
enum ToolTreeIconKey {
    ToolTreeIconCamera = 0,     ///< camera.svg
    ToolTreeIconPreprocess,     ///< preprocess.svg
    ToolTreeIconLocate,         ///< locate.svg
    ToolTreeIconDetect,         ///< detect.svg
    ToolTreeIconLogic,          ///< logic.svg
    ToolTreeIconComm,           ///< comm.svg
    ToolTreeIconResult,         ///< result.svg
    ToolTreeIconParallel,       ///< parallel.svg
    ToolTreeIconIf,             ///< if.svg
    ToolTreeIconBranch,         ///< branch.svg
    ToolTreeIconCount ///< 图标个数
};

/** @brief ToolType → 图标键 */
VISION_UI_API int toolTreeIconKeyForType(ToolType type);

/**
 * @brief 按键取图标：优先 exe/icons/tools/*.svg，缺失时用细线占位
 * @param key ToolTreeIconKey
 */
VISION_UI_API QIcon toolTreeIcon(int key);

/**
 * @brief 按具体 toolName 取图标（tools/<toolName>.svg），缺失时回退到类型图标
 * @param toolName 如 locate.template
 * @param typeKeyFallback ToolTreeIconKey
 */
VISION_UI_API QIcon toolTreeIconForName(const QString& toolName, int typeKeyFallback);

/** @brief 清空缓存（替换磁盘图标后可调用以重新加载） */
VISION_UI_API void clearToolTreeIconCache();

}

#endif
