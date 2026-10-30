/**
 * @file VisionLicenseExport.h
 * @brief 许可库导出
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LICENSE_EXPORT_H
#define VISION_LICENSE_EXPORT_H

#ifdef VISION_LICENSE_EXPORTS
#define VISION_LICENSE_API __declspec(dllexport)
#else
#define VISION_LICENSE_API __declspec(dllimport)
#endif

#endif
