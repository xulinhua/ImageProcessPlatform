/**
 * @file VisionLogExport.h
 * @brief VisionLogExportÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LOG_EXPORT_H
#define VISION_LOG_EXPORT_H

#ifdef VISION_LOG_EXPORTS
#define VISION_LOG_API __declspec(dllexport)
#else
#define VISION_LOG_API __declspec(dllimport)
#endif

#endif
