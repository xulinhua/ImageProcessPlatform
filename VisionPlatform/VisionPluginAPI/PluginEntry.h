#ifndef VISION_PLUGINENTRY_H
#define VISION_PLUGINENTRY_H

/**
 * @file PluginEntry.h
 * @brief 插件 DLL 入口约定（工具 / 算法插件共用）
 *
 * 每个 plugins\\tools\\*.dll、plugins\\algos\\*.dll 须导出下列 C 符号，
 * 宿主 loadVisionPlugins 用 GetProcAddress 查找；apiVersion 须为 1。
 *
 * @code
 * VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion() { return 1; }
 *
 * VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
 * {
 *     host->addTool(new MyTool());   // 或 host->addAlg(new MyAlg());
 * }
 * @endcode
 *
 * @note 插件工程只需包含本头（及 ToolBase / AlgToolBase），不必链接 VisionPluginAPI.lib。
 * @note 子类头文件仅留在插件工程内，平台不可见。
 */

#include "PluginHost.h"

#ifdef _WIN32
/** @brief 插件导出 C 符号（Windows） */
#define VISION_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
/** @brief 插件导出 C 符号（非 Windows） */
#define VISION_PLUGIN_EXPORT extern "C"
#endif

#endif
