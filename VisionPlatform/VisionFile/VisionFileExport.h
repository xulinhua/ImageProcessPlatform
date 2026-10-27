/**
 * @file VisionFileExport.h
 * @brief VisionFileExportÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_FILE_EXPORT_H
#define VISION_FILE_EXPORT_H

#ifdef VISION_FILE_EXPORTS
#define VISION_FILE_API __declspec(dllexport)
#else
#define VISION_FILE_API __declspec(dllimport)
#endif

#endif
