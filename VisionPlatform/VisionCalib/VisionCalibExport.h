/**
 * @file VisionCalibExport.h
 * @brief VisionCalibExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CALIB_EXPORT_H
#define VISION_CALIB_EXPORT_H

#ifdef VISION_CALIB_EXPORTS
#define VISION_CALIB_API __declspec(dllexport)
#else
#define VISION_CALIB_API __declspec(dllimport)
#endif

#endif
