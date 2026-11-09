/**
 * @file RemoteFiles.h
 * @brief 两端的配置位置和本机身份（Qt 封装 RemotePaths）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_FILES_H
#define VISION_REMOTE_FILES_H

#include <QString>

namespace Vision {

/**
 * @class RemoteFiles
 * @brief 配置/日志/截图/录屏路径与本机用户名、计算机名，两端共用
 * @note 默认落在 exe旁\\exe名\\{config,log,image,video}；ensureLayout 会迁旧文件
 */
class RemoteFiles {
public:
    /** @brief 建好数据根与四类目录并迁移旧路径（可重复调用） */
    static void ensureLayout();

    /** @brief exe 所在目录 */
    static QString exeDir();
    /** @brief exe旁\\exe名（数据根，与 VP 目录隔离） */
    static QString dataRoot();
    /** @brief 数据根\\config */
    static QString configDir();
    /** @brief 数据根\\log */
    static QString logDir();
    /** @brief 数据根\\image */
    static QString imageDir();
    /** @brief 数据根\\video */
    static QString videoDir();

    /** @brief config\\name；兼容旧名 exeFile */
    static QString configFile(const QString& name);
    /** @brief 同 configFile（历史接口） */
    static QString exeFile(const QString& name);
    /** @brief log\\name */
    static QString logFile(const QString& name);

    /** @brief %APPDATA%\\app，不存在时建好（仅兼容旧审计等，新代码勿用） */
    static QString appDataDir(const QString& app);

    /** @brief 当前 Windows 登录用户名，取不到时为“用户” */
    static QString userName();
    /** @brief 本机 NetBIOS 计算机名，取不到时为“PC” */
    static QString computerName();

    /** @brief 中控台显示名，形如“用户 · 计算机” */
    static QString selfName();
};

}

#endif
