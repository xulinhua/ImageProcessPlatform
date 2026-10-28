/**
 * @file VisionCommExport.h
 * @brief VisionCommExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMM_EXPORT_H
#define VISION_COMM_EXPORT_H

#ifdef VISION_COMM_EXPORTS
#define VISION_COMM_API __declspec(dllexport)
#else
#define VISION_COMM_API __declspec(dllimport)
#endif

#endif
