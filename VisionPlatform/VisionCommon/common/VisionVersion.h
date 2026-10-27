/**
 * @file VisionVersion.h
 * @brief VisionVersion头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_VISIONVERSION_H
#define VISION_VISIONVERSION_H
#include "VisionCommonExport.h"
#include "AlgLibKind.h"

#include <string>
#include <vector>

/** @brief 产品主版本（改号只改这里） */
#define VISION_VERSION_STRING "1.0.0"

/** @brief 模块版本初值；工具可单独覆盖 ToolBase::version */
#define VISION_MODULE_CORE_VERSION       "1.0.0"
#define VISION_MODULE_COMM_VERSION       "1.0.0"
#define VISION_MODULE_ALGOBASE_VERSION   "1.0.0"
#define VISION_MODULE_HALCON_VERSION     "1.0.0"
#define VISION_MODULE_GRABFILE_VERSION   "1.0.0"
#define VISION_MODULE_HSIMAGE_VERSION    "1.0.0"

namespace Vision {

/** @brief cpp 里的一行变更。note 按系统 ANSI 存放 */
struct VerNoteRaw {
    const char* ver;  ///< 四段版本
    const char* note; ///< 这一版改了什么
};

/** @brief 交给界面的一行变更。note 为 UTF-8 */
struct VerNote {
    std::string ver;  ///< 四段版本
    std::string note; ///< 这一版改了什么
};


/**
 * @class VersionInfo
 * @brief 一条版本记录（软件模块或已注册工具）
 */
struct VersionInfo {
    std::string id;       ///< 稳定键，如 vision.platform / locate.template
    std::string name;     ///< 显示名
    std::string version;  ///< major.minor.patch
    std::string modified; ///< 工具 DLL 修改时间（本地）
    bool algoTool;        ///< 是否算法工具
    bool hasAlgLib[AlgLibKindMax]; ///< 各算法库是否已登记（Hal/Hik/Hray/CV）
    VersionInfo()
        : algoTool(false)
    {
        for (int i = 0; i < AlgLibKindMax; ++i) {
            hasAlgLib[i] = false;
        }
    }
};

/** @brief 把 cpp 里的变更表转成 UTF-8 */
VISION_COMMON_API std::vector<VerNote> verNotesFrom(const VerNoteRaw* rows, int count);
/**
 * @brief 列出 (saved, current] 之间的说明。saved 比 current 新时说明本程序到哪一版
 */
VISION_COMMON_API std::string verNotesBetween(const std::vector<VerNote>& notes,
                                              const std::string& saved,
                                              const std::string& current);

/** @brief 产品显示名 */
VISION_COMMON_API const char* appProductName();
/** @brief 软件版本字符串 */
VISION_COMMON_API const char* appVersion();
/** @brief 编译时间戳（__DATE__ __TIME__） */
VISION_COMMON_API std::string buildStamp();
/** @brief 静态模块版本表 */
VISION_COMMON_API std::vector<VersionInfo> moduleVersions();
#ifndef VISION_TOOL_API
#ifdef VISION_TOOL_EXPORTS
#define VISION_TOOL_API __declspec(dllexport)
#else
#define VISION_TOOL_API __declspec(dllimport)
#endif
#endif

/**
 * @brief 已注册工具版本表（实现在 VisionToolBase）
 */
VISION_TOOL_API std::vector<VersionInfo> toolVersions();

}

#endif
