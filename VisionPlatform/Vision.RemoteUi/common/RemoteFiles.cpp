/**
 * @file RemoteFiles.cpp
 * @brief 路径与本机名
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteFiles.h"

#include <QCoreApplication>
#include <QDir>

#include <windows.h>

namespace Vision {

QString RemoteFiles::exeFile(const QString& name)
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
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
