/**
 * @file ToolExport.h
 * @brief ToolExportÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOL_EXPORT_H
#define VISION_TOOL_EXPORT_H

#ifndef VISION_TOOL_API
#ifdef VISION_TOOL_EXPORTS
#define VISION_TOOL_API __declspec(dllexport)
#else
#define VISION_TOOL_API __declspec(dllimport)
#endif
#endif

#endif
