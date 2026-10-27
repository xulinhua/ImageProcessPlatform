/**
 * @file VisionCvComExport.h
 * @brief VisionCvComExportÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CVCOM_EXPORT_H
#define VISION_CVCOM_EXPORT_H

#ifdef VISION_CVCOM_EXPORTS
#define VISION_CVCOM_API __declspec(dllexport)
#else
#define VISION_CVCOM_API __declspec(dllimport)
#endif

#endif
