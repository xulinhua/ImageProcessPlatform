/**
 * @file WindowWatch.h
 * @brief 按标题或进程名找一个顶层窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_WINDOW_WATCH_H
#define VISION_CONTROLLED_WINDOW_WATCH_H

#include <windows.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Vision {

/**
 * @class WindowWatch
 * @brief 后台线程每 300 ms 枚举一次顶层窗口，在匹配的窗口里挑出目标，供抓屏和输入使用
 * @note 只看可见或最小化、且没有 owner 的窗口；找不到时 current() 为空，不改去抓桌面。
 *       上次的目标仍匹配时沿用，避免切换前台后目标在同进程几个窗口间来回跳
 */
class WindowWatch {
public:
    /** @brief 有顶层窗口的进程 */
    struct Process {
        std::wstring name;  ///< exe 文件名，如 Cursor.exe
        std::wstring path;  ///< exe 完整路径，取图标用
    };

    /**
     * @brief 列出当前有可见、未被系统隐藏、无 owner、有标题顶层窗口的进程，不含本进程
     * @return 按 exe 名去重（不区分大小写）并排序
     */
    static std::vector<Process> processes();

    /** @brief 未启动 */
    WindowWatch();
    /** @brief 停止查找线程 */
    ~WindowWatch();

    /**
     * @brief 先停旧线程，再按新条件起查找线程；两项都空时永远找不到
     * @param title 窗口标题子串，空为不限
     * @param processName 进程 exe 名（不区分大小写），空为不限；只给进程时取它的主窗口
     */
    void start(const std::wstring& title, const std::wstring& processName);
    /** @brief 停止并等待查找线程退出，current() 保留最后结果 */
    void stop();
    /** @brief 最近一次找到的窗口，没找到为空。线程安全 */
    HWND current() const;

private:
    /** @brief 一次枚举的条件与结果，传给 enumProc */
    struct Want {
        std::wstring title;         ///< 标题子串，空为不限
        std::wstring processName;   ///< 进程 exe 名，空为不限
        std::vector<HWND> hits;     ///< 所有匹配的窗口，按 Z 序
    };

    /** @brief 查找线程：按当前条件枚举一次、挑出目标写回、睡 300 ms */
    void loop();
    /** @brief EnumWindows 回调：跳过不可见、有 owner、不匹配的窗口，其余记入 hits */
    static BOOL CALLBACK enumProc(HWND hwnd, LPARAM param);
    /**
     * @brief 从匹配的窗口里挑目标
     * @param hits 匹配的窗口
     * @param last 上次的目标
     * @return last 仍在 hits 里且不比最优候选低一档时沿用 last；否则取主窗口：
     *         rank 高的优先，同档取面积最大（最小化的按还原后的大小）
     */
    static HWND pick(const std::vector<HWND>& hits, HWND last);
    /** @brief 主窗口档次：未被系统隐藏 +2，有标题且不是工具窗口 +1 */
    static int rank(HWND hwnd);
    /** @brief 窗口面积；最小化时按还原后的位置算 */
    static long long area(HWND hwnd);
    /** @brief 被 DWM 隐藏（如 UWP 后台窗口、其他虚拟桌面上的窗口） */
    static bool cloaked(HWND hwnd);
    /** @brief EnumWindows 回调：收集候选窗口所属进程，供 processes() 使用 */
    static BOOL CALLBACK listProc(HWND hwnd, LPARAM param);
    /** @brief 窗口所属进程的 exe 完整路径，取不到为空 */
    static std::wstring exePath(HWND hwnd);
    /** @brief 路径里最后一个分隔符之后的文件名 */
    static std::wstring fileName(const std::wstring& path);
    /** @brief 窗口所属进程的 exe 文件名是否等于 processName（不区分大小写） */
    static bool matchProcess(HWND hwnd, const std::wstring& processName);

    mutable std::mutex m_mu;    ///< 保护 m_title、m_process、m_hwnd
    std::thread m_thread;       ///< 查找线程
    std::atomic<bool> m_run;    ///< 查找线程是否继续
    std::wstring m_title;       ///< 当前标题条件
    std::wstring m_process;     ///< 当前进程名条件
    HWND m_hwnd;                ///< 最近一次找到的窗口，没找到为空
};

}

#endif
