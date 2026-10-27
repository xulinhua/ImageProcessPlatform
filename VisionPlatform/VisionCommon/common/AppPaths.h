/**
 * @file AppPaths.h
 * @brief AppPaths头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_APPPATHS_H
#define VISION_APPPATHS_H
#include "VisionCommonExport.h"

#include <string>

namespace Vision {

/**
 * @class AppPaths
 * @brief 本机目录约定：一律相对 exe，数据落在 exe/data（不跟系统 AppData）
 */
class VISION_COMMON_API AppPaths {
public:
    /** @brief 启动时由 VisionApp 写入 exe 所在目录 */
    static void setExeDir(const std::string& dir);
    /** @brief exe 目录 */
    static std::string exeDir();
    /** @brief 数据根：exe\data */
    static std::string rootDir();
    /** @brief 数据目录（同 rootDir） */
    static std::string dataDir();
    /** @brief 日志目录：data\logs */
    static std::string logsDir();
    /** @brief 方案目录：data\solutions（*.vpsol） */
    static std::string solutionsDir();
    /** @brief 独立组合：data\composites，一个文件一份 */
    static std::string compositesDir();
    /** @brief 本机设置：data\settings.json */
    static std::string settingsFile();
    /** @brief SQLite：data\vision.db */
    static std::string databaseFile();
    /** @brief 插件根：exe\plugins */
    static std::string pluginsDir();
    /** @brief 工具壳：plugins\tools */
    static std::string pluginsToolsDir();
    /** @brief 算法后端：plugins\algos */
    static std::string pluginsAlgosDir();
    /** @brief 确保 data、logs、solutions、plugins 存在 */
    static void ensureDirs();
};

}

#endif