/**
 * @file VisionCommonExport.h
 * @brief VisionCommonExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMMON_EXPORT_H
#define VISION_COMMON_EXPORT_H

#ifdef VISION_COMMON_EXPORTS
#define VISION_COMMON_API __declspec(dllexport)
#else
#define VISION_COMMON_API __declspec(dllimport)
#endif

#endif
