/**
 * @file VisionLangExport.h
 * @brief VisionLangExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LANGUAGE_EXPORT_H
#define VISION_LANGUAGE_EXPORT_H

#ifdef VISION_LANGUAGE_EXPORTS
#define VISION_LANGUAGE_API __declspec(dllexport)
#else
#define VISION_LANGUAGE_API __declspec(dllimport)
#endif

#endif
