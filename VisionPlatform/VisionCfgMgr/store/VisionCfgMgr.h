/**
 * @file VisionCfgMgr.h
 * @brief VisionCfgMgr头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONFIGMGR_H
#define VISION_CONFIGMGR_H
#include "VisionConfigExport.h"

#include "AppSettings.h"
#include "Solution.h"
#include <string>

namespace Vision {

enum FileLoadStatus {
    FileLoadOk = 0,      ///< 读到有效内容
    FileLoadMissing,     ///< 文件不存在
    FileLoadCorrupt      ///< 存在但无法解析
};

/**
 * @class VisionCfgMgr
 * @brief 方案与本机设置的存盘入口。用户与日志在 SQLite，不经过本类
 */
class VISION_CONFIG_API VisionCfgMgr {
public:
    /**
     * @brief 写方案到 s.filePath
     * @param error 失败原因
     */
    static bool saveSolution(const Solution& s, std::string* error);
    /**
     * @brief 读方案
     * @param error 失败原因
     */
    static bool loadSolution(const std::string& path, Solution* s, std::string* error);
    /** @brief 可存盘配方指纹，不含保存者、时间、软件版本和 paraVersion */
    static std::string persistFingerprint(const Solution& s);
    /**
     * @brief 为方案旁路 .vpdata 建好全部工具目录（保存成功后调用）
     */
    static bool ensureSolutionDataDirs(const Solution& s);
    /** @brief 写本机设置 */
    static bool saveSettings(const AppSettings& s);
    /**
     * @brief 读本机设置到 *s；失败时 *s 为出厂默认
     */
    static FileLoadStatus loadSettings(AppSettings* s, std::string* error);
    /** @brief 读本机设置，失败返回出厂默认 */
    static AppSettings loadSettings();
};

}

#endif
