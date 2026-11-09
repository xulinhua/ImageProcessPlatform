/**
 * @file RemoteFiles.cpp
 * @brief 路径与本机名
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteFiles.h"
#include "RemotePaths.h"

#include <QCoreApplication>
#include <QDir>

#include <windows.h>

namespace Vision {

void RemoteFiles::ensureLayout()
{
    RemotePaths::ensureLayout();
}

QString RemoteFiles::exeDir()
{
    return QString::fromStdWString(RemotePaths::exeDirW());
}

QString RemoteFiles::dataRoot()
{
    RemotePaths::ensureLayout();
    return QString::fromStdWString(RemotePaths::dataRootW());
}

QString RemoteFiles::configDir()
{
    RemotePaths::ensureLayout();
    return QString::fromStdWString(RemotePaths::configDirW());
}

QString RemoteFiles::logDir()
{
    RemotePaths::ensureLayout();
    return QString::fromStdWString(RemotePaths::logDirW());
}

QString RemoteFiles::imageDir()
{
    RemotePaths::ensureLayout();
    return QString::fromStdWString(RemotePaths::imageDirW());
}

QString RemoteFiles::videoDir()
{
    RemotePaths::ensureLayout();
    return QString::fromStdWString(RemotePaths::videoDirW());
}

QString RemoteFiles::configFile(const QString& name)
{
    RemotePaths::ensureLayout();
    return QDir(configDir()).filePath(name);
}

QString RemoteFiles::exeFile(const QString& name)
{
    return configFile(name);
}

QString RemoteFiles::logFile(const QString& name)
{
    RemotePaths::ensureLayout();
    return QDir(logDir()).filePath(name);
}

QString RemoteFiles::appDataDir(const QString& app)
{
    QString base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty()) {
        base = QCoreApplication::applicationDirPath();
    }
    const QString dir = QDir(base).filePath(app);
    QDir().mkpath(dir);
    return dir;
}

QString RemoteFiles::userName()
{
    wchar_t name[256];
    DWORD count = 256;
    if (!GetUserNameW(name, &count) || count == 0) {
        return QString::fromUtf8("用户");
    }
    return QString::fromWCharArray(name, static_cast<int>(count - 1));
}

QString RemoteFiles::computerName()
{
    wchar_t name[256];
    DWORD count = 256;
    if (!GetComputerNameW(name, &count)) {
        return QString("PC");
    }
    return QString::fromWCharArray(name, static_cast<int>(count));
}

QString RemoteFiles::selfName()
{
    return userName() + QString::fromUtf8(" · ") + computerName();
}

}
