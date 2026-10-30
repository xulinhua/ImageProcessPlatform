/**
 * @file main.cpp
 * @brief RemoteMaster（中控台）入口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ControlWindow.h"
#include "RemoteCrash.h"
#include "RemoteTheme.h"
#include <QApplication>
#include <QMessageBox>
#include <windows.h>

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

/** @brief 把已在跑的中控台主窗拉到前台（含最小化、托盘隐藏后仍有 HWND 的情况） */
void activateRunningControl()
{
    HWND hwnd = FindWindowW(nullptr, L"中控台");
    if (!hwnd) {
        return;
    }
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }
    SetForegroundWindow(hwnd);
}

/** @brief 单实例互斥；Qt 初始化后 LastError 可能残留 ERROR_ALREADY_EXISTS，必须先清零再 CreateMutex */
HANDLE takeControlMutex(bool* already)
{
    *already = false;
    SetLastError(ERROR_SUCCESS);
    HANDLE held = CreateMutexW(nullptr, TRUE, L"Local\\RemoteMaster");
    if (!held) {
        return nullptr;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        *already = true;
    }
    return held;
}

}

int main(int argc, char* argv[])
{
    Vision::RemoteCrash::install("master");
    Vision::RemoteCrash::checkpoint("main.enter");
    fixWorkingDirectory();
    Vision::RemoteCrash::checkpoint("main.cwd");
    HMODULE user = GetModuleHandleW(L"user32.dll");
    typedef BOOL (WINAPI *SetDpiContext)(HANDLE);
    SetDpiContext setDpi = user ? reinterpret_cast<SetDpiContext>(GetProcAddress(user, "SetProcessDpiAwarenessContext")) : 0;
    if (setDpi) {
        setDpi(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4)));
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

        bool already = false;
        HANDLE held = takeControlMutex(&already);
        if (already) {
            Vision::RemoteCrash::checkpoint("main.already_running");
            activateRunningControl();
            QMessageBox::information(0, QString::fromUtf8("中控台"), QString::fromUtf8("已经在任务栏运行。"));
            if (held) {
                CloseHandle(held);
            }
            return 0;
        }
        if (!held) {
            Vision::RemoteCrash::alert("main.mutex", "无法创建单实例互斥，仍继续启动", true);
        }

        Vision::RemoteCrash::checkpoint("main.before_window");
        const bool built = Vision::RemoteCrash::run("main.build_window", [&]() {
            Vision::ControlWindow window;
#if defined(_DEBUG)
            window.setWindowTitle(QString::fromUtf8("中控台 [Debug]"));
#endif
            Vision::RemoteCrash::checkpoint("main.window_built");
            if (!window.startHidden()) {
                window.show();
            }
            Vision::RemoteCrash::checkpoint("main.event_loop");
            code = app.exec();
            Vision::RemoteCrash::checkpoint("main.exit");
        });
        if (!built) {
            QMessageBox::critical(0, QString::fromUtf8("中控台"),
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
