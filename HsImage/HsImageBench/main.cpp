/**
 * @file main.cpp
 * @brief main µœ÷
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "MainWindow.h"

#include <QApplication>

// ∆Ù∂Ø HsImageBench
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
