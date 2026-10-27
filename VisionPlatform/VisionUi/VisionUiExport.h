/**
 * @file VisionUiExport.h
 * @brief VisionUiExportÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_UI_EXPORT_H
#define VISION_UI_EXPORT_H

#ifdef VISION_UI_EXPORTS
#define VISION_UI_API __declspec(dllexport)
#else
#define VISION_UI_API __declspec(dllimport)
#endif

#endif
