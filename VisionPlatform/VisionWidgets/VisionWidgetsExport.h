/**
 * @file VisionWidgetsExport.h
 * @brief VisionWidgetsExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_WIDGETS_EXPORT_H
#define VISION_WIDGETS_EXPORT_H

#ifdef VISION_WIDGETS_EXPORTS
#define VISION_WIDGETS_API __declspec(dllexport)
#else
#define VISION_WIDGETS_API __declspec(dllimport)
#endif

#endif
