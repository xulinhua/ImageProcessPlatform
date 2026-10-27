/**
 * @file SolutionDataPaths.h
 * @brief SolutionDataPaths头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SOLUTIONDATAPATHS_H
#define VISION_SOLUTIONDATAPATHS_H
#include "VisionCommonExport.h"

#include "ObjectId.h"
#include <string>

namespace Vision {

/** @brief 路径拼接（Windows 反斜杠；路径串为 UTF-8） */
VISION_COMMON_API std::string pathJoin(const std::string& a, const std::string& b);
/** @brief 取目录部分；无分隔符返回空 */
VISION_COMMON_API std::string pathDirName(const std::string& filePath);
/** @brief 无扩展文件名（MyLine.vpsol → MyLine） */
VISION_COMMON_API std::string pathFileStem(const std::string& filePath);
/** @brief 文件是否存在（UTF-8 路径） */
VISION_COMMON_API bool pathFileExists(const std::string& filePath);
/**
 * @brief 方案旁路数据根：与 .vpsol 同目录同名 .vpdata
 * @param vpsolPath 方案文件全路径（UTF-8）；空则返回空
 */
VISION_COMMON_API std::string solutionDataRootFromFile(const std::string& vpsolPath);
/**
 * @brief 单工具数据目录：vpdata/stations/{sid}/jobs/{jid}/tools/{tid}
 * @param vpsolPath 方案 .vpsol 全路径（UTF-8）；空则返回空
 */
VISION_COMMON_API std::string toolDataDir(const std::string& vpsolPath,
                        const ObjectId& stationId,
                        const ObjectId& jobId,
                        const ObjectId& toolId);
/** @brief 递归创建目录（含多级；UTF-8 路径） */
VISION_COMMON_API bool ensureDirectoryTree(const std::string& absPath);
/**
 * @brief 递归拷贝目录（另存方案时同步 vpdata）
 * @param src 源目录（UTF-8）
 * @param dst 目标目录（UTF-8）
 */
VISION_COMMON_API bool copyDirectoryRecursive(const std::string& src, const std::string& dst);
/** @brief 删除文件；不存在也算成功 */
VISION_COMMON_API bool removeFileIfExists(const std::string& filePath);
/**
 * @brief 工具相对文件 → 绝对路径
 * @param dataDir toolDataDir
 * @param relativeName 如 model.ncm；已是绝对路径则原样返回
 */
VISION_COMMON_API std::string resolveToolAssetPath(const std::string& dataDir, const std::string& relativeName);

}

#endif