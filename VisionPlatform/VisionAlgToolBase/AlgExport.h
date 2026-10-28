/**
 * @file AlgExport.h
 * @brief AlgExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ALGOR_EXPORT_H
#define VISION_ALGOR_EXPORT_H

#ifdef VISION_ALGOR_EXPORTS
#define VISION_ALGOR_API __declspec(dllexport)
#else
#define VISION_ALGOR_API __declspec(dllimport)
#endif

#endif
