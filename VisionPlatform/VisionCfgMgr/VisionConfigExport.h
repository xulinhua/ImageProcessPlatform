/**
 * @file VisionConfigExport.h
 * @brief VisionConfigExportÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONFIG_EXPORT_H
#define VISION_CONFIG_EXPORT_H

#ifdef VISION_CONFIG_EXPORTS
#define VISION_CONFIG_API __declspec(dllexport)
#else
#define VISION_CONFIG_API __declspec(dllimport)
#endif

#endif
