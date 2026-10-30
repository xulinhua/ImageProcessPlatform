/**
 * @file RemoteFiles.h
 * @brief 两端的配置位置和本机身份
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
 * @brief 配置文件路径与本机用户名、计算机名，两端共用
 */
class RemoteFiles {
public:
    /** @brief exe 所在目录下的文件 */
    static QString exeFile(const QString& name);

    /** @brief %APPDATA%\app，不存在时建好 */
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
