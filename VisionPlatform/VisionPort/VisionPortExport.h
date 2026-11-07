/**
 * @file VisionPortExport.h
 * @brief VisionPortExport头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PORT_EXPORT_H
#define VISION_PORT_EXPORT_H

#ifdef VISION_PORT_EXPORTS
#define VISION_PORT_API __declspec(dllexport)
#else
#define VISION_PORT_API __declspec(dllimport)
#endif

#endif
