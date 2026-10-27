/**
 * @file VisionToolUiExport.h
 * @brief VisionToolUiExportÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLUI_EXPORT_H
#define VISION_TOOLUI_EXPORT_H

#ifdef VISION_TOOLUI_EXPORTS
#define VISION_TOOLUI_API __declspec(dllexport)
#else
#define VISION_TOOLUI_API __declspec(dllimport)
#endif

#endif
