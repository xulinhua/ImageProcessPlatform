/**
 * @file IpcExport.h
 * @brief IpcBase 导出宏
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef IPC_EXPORT_H
#define IPC_EXPORT_H

#ifdef IPC_BASE_EXPORTS
#define IPC_API __declspec(dllexport)
#else
#define IPC_API __declspec(dllimport)
#endif

#endif
