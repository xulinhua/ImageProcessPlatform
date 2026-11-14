/**
 * @file main.cpp
 * @brief RemoteDesk 入口。一个进程里可选控制端或被控端
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeskWindow.h"
#include "RemotePorts.h"
#include "RemoteCrash.h"
#include "RemoteLog.h"
#include "RemotePaths.h"
#include "RemoteTheme.h"
#include "StartupSweep.h"
#include <QApplication>
#include <QMessageBox>
#include <windows.h>
#include <vector>

namespace {

/** @brief 管理员右键启动时 cwd 常是 System32，先切回 exe 目录再找 Qt 插件 */
void fixWorkingDirectory()
{
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) {
        return;
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) {
        return;
    }
    *slash = L'\0';
    SetCurrentDirectoryW(path);
    SetDllDirectoryW(path);
}

}

int main(int argc, char* argv[])
{
    fixWorkingDirectory();
    Vision::RemotePorts::useDesk();
    Vision::RemotePaths::ensureLayout();
    Vision::RemoteCrash::install("desk");
    Vision::RemoteLog::install("desk");
    Vision::RemoteLog::writef(Vision::RemoteLog::Info, "启动", "远程启动");
    HMODULE user = GetModuleHandleW(L"user32.dll");
    typedef BOOL (WINAPI *SetDpiContext)(HANDLE);
    SetDpiContext setDpi = user ? reinterpret_cast<SetDpiContext>(GetProcAddress(user, "SetProcessDpiAwarenessContext")) : 0;
    if (setDpi) {
        setDpi(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4)));
    }
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    int code = 1;
    try {
        QApplication app(argc, argv);
        app.setQuitOnLastWindowClosed(false);
        Vision::RemoteTheme::apply(&app);

        std::vector<int> tcp;
        std::vector<int> udp;
        Vision::DeskWindow::collectPorts(&tcp, &udp);
        const Vision::StartupSweep::Report swept = Vision::StartupSweep::run(
            Vision::StartupSweep::RolePlain, tcp, udp);
        if (swept.any()) {
            Vision::RemoteLog::writef(Vision::RemoteLog::Info, "启动", "%s", swept.text.c_str());
        }

        SetLastError(ERROR_SUCCESS);
        HANDLE held = CreateMutexW(nullptr, TRUE, L"Local\\RemoteDesk");
        if (held && GetLastError() == ERROR_ALREADY_EXISTS) {
            QMessageBox::information(0, QString::fromUtf8("远程"), QString::fromUtf8("已经在运行。"));
            CloseHandle(held);
            return 0;
        }

        Vision::DeskWindow window;
        window.show();
        code = app.exec();
        if (held) {
            CloseHandle(held);
        }
    } catch (const std::exception& e) {
        Vision::RemoteCrash::alert("main", e.what(), true);
        code = 1;
    } catch (...) {
        Vision::RemoteCrash::alert("main", "未知异常，进程退出", true);
        code = 1;
    }
    return code;
}
