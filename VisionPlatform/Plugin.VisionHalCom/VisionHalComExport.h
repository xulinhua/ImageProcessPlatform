/**
 * @file VisionHalComExport.h
 * @brief VisionHalComExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCOM_EXPORT_H
#define VISION_HALCOM_EXPORT_H

#ifdef VISION_HALCOM_EXPORTS
#define VISION_HALCOM_API __declspec(dllexport)
#else
#define VISION_HALCOM_API __declspec(dllimport)
#endif

#endif
