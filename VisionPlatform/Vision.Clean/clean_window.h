/**
 * @file clean_window.h
 * @brief 清理窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CLEAN_WINDOW_H
#define VISION_CLEAN_WINDOW_H

#include "solution_procs.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/** @brief 列出解决方案进程，并结束选中的或全部 */
class CleanWindow {
public:
    CleanWindow();
    int run(HINSTANCE instance);

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT onMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void createChild(HWND hwnd);
    void fill();
    void killIds(const std::vector<unsigned long>& pids, bool ask);
    std::vector<unsigned long> selected() const;

    SolutionProcs m_procs;
    HWND m_list;
    HWND m_status;
    HFONT m_font;
};

#endif
