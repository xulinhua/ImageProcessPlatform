/**
 * @file main.cpp
 * @brief 通讯调试器入口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "debug_theme.h"
#include "main_window.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    DebugTheme::apply(&app);
    MainWindow window;
    window.show();
    return app.exec();
}
