/**
 * @file main.cpp
 * @brief RemoteClient（被控端）入口：界面 / 会话代理 / Windows 服务
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AgentService.h"
#include "AgentWindow.h"
#include "RemoteCrash.h"
#include "RemoteTheme.h"
#include <QApplication>
#include <QMessageBox>
#include <windows.h>
#include <string>

namespace {

bool hasArg(int argc, char** argv, const char* key)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return true;
        }
    }
    return false;
}

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
    Vision::RemoteCrash::install("client");
    Vision::RemoteCrash::checkpoint("main.enter");
    fixWorkingDirectory();
    Vision::RemoteCrash::checkpoint("main.cwd");
    if (hasArg(argc, argv, "--install-service")) {
        std::string err;
        if (!Vision::AgentService::install(&err)) {
            MessageBoxA(0, err.c_str(), "RemoteClient", MB_ICONERROR);
            return 1;
        }
        MessageBoxA(0, "服务已安装并尝试启动。", "RemoteClient", MB_OK);
        return 0;
    }
    if (hasArg(argc, argv, "--uninstall-service")) {
        std::string err;
        Vision::AgentService::uninstall(&err);
        return 0;
    }
    if (hasArg(argc, argv, "--service")) {
        Vision::RemoteCrash::checkpoint("main.service");
        try {
            return Vision::AgentService::run();
        } catch (const std::exception& e) {
            Vision::RemoteCrash::alert("main.service", e.what(), true);
            return 1;
        } catch (...) {
            Vision::RemoteCrash::alert("main.service", "服务线程未知异常", true);
            return 1;
        }
    }

    HMODULE user = GetModuleHandleW(L"user32.dll");
    typedef BOOL (WINAPI *SetDpiContext)(HANDLE);
    SetDpiContext setDpi = user ? reinterpret_cast<SetDpiContext>(GetProcAddress(user, "SetProcessDpiAwarenessContext")) : 0;
    if (!setDpi || !setDpi(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4)))) {
        SetProcessDPIAware();
    }
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    Vision::RemoteCrash::checkpoint("main.before_qapp");

    int code = 1;
    try {
        QApplication app(argc, argv);
        app.setQuitOnLastWindowClosed(false);
        Vision::RemoteTheme::apply(&app);
        Vision::RemoteCrash::checkpoint("main.theme");

        const bool agentMode = hasArg(argc, argv, "--agent");
        SetLastError(ERROR_SUCCESS);
        HANDLE held = CreateMutexW(nullptr, TRUE, agentMode ? L"Local\\RemoteClient.Agent" : L"Local\\RemoteClient");
        if (!agentMode && held && GetLastError() == ERROR_ALREADY_EXISTS) {
            Vision::RemoteCrash::checkpoint("main.already_running");
            QMessageBox::information(0, QString::fromUtf8("被控端"), QString::fromUtf8("已经在任务栏运行。"));
            CloseHandle(held);
            return 0;
        }

        Vision::RemoteCrash::checkpoint("main.before_window");
        const bool built = Vision::RemoteCrash::run("main.build_window", [&]() {
            Vision::AgentWindow window;
            Vision::RemoteCrash::checkpoint("main.window_built");
            window.show();
            Vision::RemoteCrash::checkpoint("main.event_loop");
            code = app.exec();
            Vision::RemoteCrash::checkpoint("main.exit");
        });
        if (!built) {
            QMessageBox::critical(0, QString::fromUtf8("被控端"),
                QString::fromUtf8("主窗口启动失败，已写入日志：\n%1")
                    .arg(QString::fromUtf8(Vision::RemoteCrash::logPath())));
            code = 1;
        }
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
