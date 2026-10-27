/**
 * @file VisionCoreExport.h
 * @brief VisionCoreExportÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISIONCORE_EXPORT_H
#define VISIONCORE_EXPORT_H

#ifdef VISIONCORE_EXPORTS
#define VISIONCORE_API __declspec(dllexport)
#else
#define VISIONCORE_API __declspec(dllimport)
#endif

#endif
