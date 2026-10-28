/**
 * @file VisionIpcExport.h
 * @brief VisionIpc 导出宏
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_IPC_EXPORT_H
#define VISION_IPC_EXPORT_H

#ifdef VISION_IPC_EXPORTS
#define VISION_IPC_API __declspec(dllexport)
#else
#define VISION_IPC_API __declspec(dllimport)
#endif

#endif
