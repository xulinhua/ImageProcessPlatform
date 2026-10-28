/**
 * @file VisionGrabExport.h
 * @brief VisionGrabExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_GRAB_EXPORT_H
#define VISION_GRAB_EXPORT_H

#ifdef VISION_GRAB_EXPORTS
#define VISION_GRAB_API __declspec(dllexport)
#else
#define VISION_GRAB_API __declspec(dllimport)
#endif

#endif
