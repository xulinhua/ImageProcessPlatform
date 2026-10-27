/**
 * @file VisionDbExport.h
 * @brief VisionDbExportÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DB_EXPORT_H
#define VISION_DB_EXPORT_H

#ifdef VISION_DB_EXPORTS
#define VISION_DB_API __declspec(dllexport)
#else
#define VISION_DB_API __declspec(dllimport)
#endif

#endif
