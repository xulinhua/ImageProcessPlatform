/**
 * @file main.cpp
 * @brief 清理本解决方案下仍在运行的程序
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "clean_window.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    CleanWindow window;
    return window.run(instance);
}
