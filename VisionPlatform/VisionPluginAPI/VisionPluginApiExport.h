/**
 * @file VisionPluginApiExport.h
 * @brief VisionPluginAPI.dll 导出宏
 * @author Hoson
 * @note 宿主链接本 DLL 时用 dllimport；编译本工程时定义 VISION_PLUGINAPI_EXPORTS。
 *       插件 DLL 一般只包含 IPluginHost / PluginEntry 头，不必链接本 DLL。
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PLUGINAPI_EXPORT_H
#define VISION_PLUGINAPI_EXPORT_H

#ifdef VISION_PLUGINAPI_EXPORTS
#define VISION_PLUGINAPI_API __declspec(dllexport)
#else
#define VISION_PLUGINAPI_API __declspec(dllimport)
#endif

#endif
