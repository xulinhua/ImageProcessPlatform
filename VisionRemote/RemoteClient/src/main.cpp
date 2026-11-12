/**
 * @file main.cpp
 * @brief Ubuntu 22.04 被控端入口。先出窗口，会话在后台线程里跑
 */

#include "AgentWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QMetaObject>
#include <QStringList>

#include "AgentApp.h"

#include <csignal>
#include <thread>

int main(int argc, char** argv)
{
    std::signal(SIGPIPE, SIG_IGN);
    QApplication app(argc, argv);
    Vision::AgentWindow window;
    Vision::AgentApp agent;
    agent.setStatusHandler([&window](const std::string& deviceId, const std::string& fingerprint, const std::string& desk, const std::string& state) {
        QMetaObject::invokeMethod(&window, "applyStatus", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromStdString(deviceId)),
            Q_ARG(QString, QString::fromStdString(fingerprint)),
            Q_ARG(QString, QString::fromStdString(desk)),
            Q_ARG(QString, QString::fromStdString(state)));
    });
    agent.setPreviewHandler([&window](const std::string& jpeg, int w, int h) {
        QMetaObject::invokeMethod(&window, "applyPreview", Qt::QueuedConnection,
            Q_ARG(QByteArray, QByteArray(jpeg.data(), static_cast<int>(jpeg.size()))),
            Q_ARG(int, w),
            Q_ARG(int, h));
    });
    agent.setWindowHandler([&window](const std::vector<std::string>& rows) {
        QStringList list;
        for (size_t i = 0; i < rows.size(); ++i) {
            list.push_back(QString::fromStdString(rows[i]));
        }
        QMetaObject::invokeMethod(&window, "applyWindows", Qt::QueuedConnection, Q_ARG(QStringList, list));
    });
    agent.setShareHandler([&window](bool desktop, const std::string& process, const std::string& title) {
        QMetaObject::invokeMethod(&window, "applyShare", Qt::QueuedConnection,
            Q_ARG(bool, desktop),
            Q_ARG(QString, QString::fromStdString(process)),
            Q_ARG(QString, QString::fromStdString(title)));
    });
    agent.setChatHandler([&window](const std::string& name, const std::string& text, int kind) {
        QMetaObject::invokeMethod(&window, "applyChat", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromStdString(name)),
            Q_ARG(QString, QString::fromStdString(text)),
            Q_ARG(int, kind));
    });
    agent.setFileHandler([&window](const std::string& note) {
        QMetaObject::invokeMethod(&window, "applyFile", Qt::QueuedConnection, Q_ARG(QString, QString::fromStdString(note)));
    });
    agent.setCodeHandler([&window](const std::string& code) {
        QMetaObject::invokeMethod(&window, "applyCode", Qt::QueuedConnection, Q_ARG(QString, QString::fromStdString(code)));
    });
    QObject::connect(&window, &Vision::AgentWindow::codeRefresh, [&agent]() { agent.refreshCode(); });
    QObject::connect(&window, &Vision::AgentWindow::shareChosen, [&agent](bool desktop, const QString& process, const QString& title) {
        agent.chooseShare(desktop, process.toStdString(), title.toStdString());
    });
    QObject::connect(&window, &Vision::AgentWindow::chatSend, [&agent](const QString& text) {
        agent.submitChat(text.toStdString());
    });
    window.show();
    int code = 0;
    std::thread worker([&]() {
        code = agent.run(argc, argv);
        QMetaObject::invokeMethod(&app, "quit", Qt::QueuedConnection);
    });
    const int ui = app.exec();
    worker.join();
    return code != 0 ? code : ui;
}
