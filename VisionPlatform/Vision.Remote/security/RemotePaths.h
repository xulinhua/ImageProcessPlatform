/**
 * @file RemotePaths.h
 * @brief exe 同级「exe名」目录布局：config / log / image / video（无 Qt，两端共用）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_PATHS_H
#define VISION_REMOTE_PATHS_H

#include <string>

namespace Vision {

/**
 * @class RemotePaths
 * @brief 用 GetModuleFileName 定位路径：exe旁\\RemoteMaster\\config 等，与 VP 目录隔离
 */
class RemotePaths {
public:
    /** @brief exe 所在目录（无尾斜杠） */
    static std::wstring exeDirW();
    /** @brief exe 文件名（无扩展名），如 RemoteMaster / RemoteClient */
    static std::wstring exeStemW();
    /** @brief exe旁\\exe名，数据根目录 */
    static std::wstring dataRootW();
    /** @brief 数据根\\config */
    static std::wstring configDirW();
    /** @brief 数据根\\log */
    static std::wstring logDirW();
    /** @brief 数据根\\image */
    static std::wstring imageDirW();
    /** @brief 数据根\\video */
    static std::wstring videoDirW();

    /** @brief config\\name */
    static std::wstring configFileW(const wchar_t* name);
    /** @brief log\\name */
    static std::wstring logFileW(const wchar_t* name);

    /** @brief 宽路径转 UTF-8 */
    static std::string narrow(const std::wstring& path);
    /** @brief UTF-8 转宽路径 */
    static std::wstring widen(const char* utf8);

    /**
     * @brief 创建数据根与四类目录，并把旧位置迁到新位置（目标已存在则跳过）
     * @note 可重复调用；大体积旧运行日志不自动搬
     */
    static void ensureLayout();
};

}

#endif
