/**
 * @file VisionAuthExport.h
 * @brief VisionAuthExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_AUTH_EXPORT_H
#define VISION_AUTH_EXPORT_H

#ifdef VISION_AUTH_EXPORTS
#define VISION_AUTH_API __declspec(dllexport)
#else
#define VISION_AUTH_API __declspec(dllimport)
#endif

#endif
