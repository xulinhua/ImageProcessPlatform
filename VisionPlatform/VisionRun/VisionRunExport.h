/**
 * @file VisionRunExport.h
 * @brief VisionRun 导出宏
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RUN_EXPORT_H
#define VISION_RUN_EXPORT_H

#ifdef VISION_RUN_EXPORTS
#define VISION_RUN_API __declspec(dllexport)
#else
#define VISION_RUN_API __declspec(dllimport)
#endif

#endif
