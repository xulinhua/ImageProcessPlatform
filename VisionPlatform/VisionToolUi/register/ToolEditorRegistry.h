/**
 * @file ToolEditorRegistry.h
 * @brief ToolEditorRegistry头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLEDITORREGISTRY_H
#define VISION_TOOLEDITORREGISTRY_H
#include "VisionToolUiExport.h"

#include <map>
#include <string>

class QWidget;

namespace Vision {

class ToolEditorBase;

/** @brief 构造某 toolName 的参数页 */
typedef ToolEditorBase* (*ToolEditorFactory)(QWidget* parent);

/**
 * @class ToolEditorRegistry
 * @brief 按 toolName 建参数页；未登记走通用键值页
 */
class VISION_TOOLUI_API ToolEditorRegistry {
public:
    /** @brief 进程内唯一登记表 */
    static ToolEditorRegistry& instance();
    /** @brief 登记工厂 */
    void add(const std::string& toolName, ToolEditorFactory factory);
    /**
     * @brief 新建编辑器，调用方接管所有权
     */
    ToolEditorBase* create(const std::string& toolName, QWidget* parent);

private:
    ToolEditorRegistry() {}
    std::map<std::string, ToolEditorFactory> m_map; ///< toolName 到工厂
};

/** @brief 登记全部工具编辑器，在 main 里算法登记之后调用 */
VISION_TOOLUI_API void registerToolEditors();

}

#endif
