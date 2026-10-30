/**
 * @file main.cpp
 * @brief HsImageBench 程序入口：创建应用并显示评测主窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "MainWindow.h"

#include <QApplication>

// 启动 HsImageBench
int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("HsImageBench"));
    app.setOrganizationName(QStringLiteral("HsVision"));

    MainWindow w;
    w.show();
    return app.exec();
}
