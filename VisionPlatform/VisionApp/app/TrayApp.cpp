/**
 * @file TrayApp.cpp
 * @brief 托盘菜单。工站启停不在界面进程里 tick
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TrayApp.h"
#include "MainWindow.h"
#include "ServiceCtl.h"
#include "SharedFrames.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QStringList>
#include <QTimer>

namespace {

const QColor kOn(30, 111, 224);
const QColor kOff(154, 163, 173);

QIcon loadAppMark()
{
    const QStringList candidates = QStringList()
        << (QApplication::applicationDirPath() + QLatin1String("/icons/app/app.png"))
        << (QApplication::applicationDirPath() + QLatin1String("/icons/app/app.ico"))
        << QLatin1String("icons/app/app.png")
        << QLatin1String("icons/app/app.ico");
    for (int i = 0; i < candidates.size(); ++i) {
        const QIcon ic(candidates.at(i));
        if (!ic.isNull() && !ic.availableSizes().isEmpty()) {
            return ic;
        }
    }
    return QIcon();
}

} // namespace

TrayApp::TrayApp(Vision::MainWindow* window, bool stationSvc, bool linkSvc, QObject* parent)
    : QObject(parent)
    , m_window(window)
    , m_tray(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
    , m_actService(0)
    , m_actUi(0)
    , m_actConsole(0)
    , m_timer(new QTimer(this))
    , m_stationSvc(stationSvc)
    , m_linkSvc(linkSvc)
    , m_console()
    , m_appIcon(loadAppMark())
{
    m_actService = m_menu->addAction(QString::fromUtf8("服务"));
    m_actUi = m_menu->addAction(QString::fromUtf8("主界面"));
    m_actConsole = m_menu->addAction(QString::fromUtf8("控制台"));
    connect(m_actService, &QAction::triggered, [this]() { toggleServices(); });
    connect(m_actUi, &QAction::triggered, [this]() { toggleUi(); });
    connect(m_actConsole, &QAction::triggered, [this]() { toggleConsole(); });
    m_menu->addSeparator();
    connect(m_menu->addAction(QString::fromUtf8("运行画面")), &QAction::triggered, [this]() { openRuntime(); });
    connect(m_menu->addAction(QString::fromUtf8("启动本机工站")), &QAction::triggered, [this]() { startOwned(); });
    connect(m_menu->addAction(QString::fromUtf8("停止本机工站")), &QAction::triggered, [this]() { stopOwned(); });
    m_menu->addSeparator();
    connect(m_menu->addAction(QString::fromUtf8("服务状态")), &QAction::triggered, [this]() { showServices(); });
    m_menu->addSeparator();
    connect(m_menu->addAction(QString::fromUtf8("退出")), &QAction::triggered, [this]() { quitApp(); });
    connect(m_menu->addAction(QString::fromUtf8("停止服务并退出")), &QAction::triggered, [this]() { stopAllAndQuit(); });
    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip(QString::fromUtf8("VisionPlatform"));
    m_tray->setIcon(makeTrayIcon(kOff));
    m_tray->show();
    connect(m_tray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) { onActivated(reason); });
    connect(m_timer, &QTimer::timeout, [this]() { refreshIcon(); });
    m_timer->start(1000);
    if (m_stationSvc || m_linkSvc) {
        startServices();
    }
    openMain();
    refreshIcon();
}

TrayApp::~TrayApp()
{
    delete m_menu;
}

QIcon TrayApp::makeTrayIcon(const QColor& status) const
{
    QIcon icon;
    const int sizes[] = { 16, 20, 24, 32, 48 };
    for (int i = 0; i < 5; ++i) {
        const int n = sizes[i];
        QPixmap pix(n, n);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        if (!m_appIcon.isNull()) {
            painter.drawPixmap(0, 0, m_appIcon.pixmap(n, n));
        } else {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(26, 95, 212));
            const int inset = n / 16;
            painter.drawRoundedRect(inset, inset, n - inset * 2, n - inset * 2, n / 5, n / 5);
            painter.setBrush(Qt::white);
            const int eyeW = n * 5 / 8;
            const int eyeH = n * 5 / 16;
            painter.drawEllipse((n - eyeW) / 2, (n - eyeH) / 2, eyeW, eyeH);
            painter.setBrush(QColor(26, 95, 212));
            const int iris = n / 5;
            painter.drawEllipse((n - iris) / 2 + n / 16, (n - iris) / 2 - n / 24, iris, iris);
        }
        const int badge = n >= 32 ? n / 3 : n * 2 / 5;
        const int x = n - badge;
        const int y = n - badge;
        painter.setPen(QPen(Qt::white, n >= 24 ? 2.0 : 1.0));
        painter.setBrush(status);
        painter.drawEllipse(x, y, badge - 1, badge - 1);
        icon.addPixmap(pix);
    }
    return icon;
}

QIcon TrayApp::makeMenuIcon(int kind, const QColor& color) const
{
    QPixmap pix(20, 20);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (kind == 0) {
        painter.drawRoundedRect(QRectF(4, 3, 12, 5.5), 1.2, 1.2);
        painter.drawRoundedRect(QRectF(4, 11, 12, 5.5), 1.2, 1.2);
        painter.drawLine(QPointF(6.2, 5.7), QPointF(8.2, 5.7));
        painter.drawLine(QPointF(6.2, 13.7), QPointF(8.2, 13.7));
    } else if (kind == 1) {
        painter.drawRoundedRect(QRectF(3, 3, 14, 9.5), 1.4, 1.4);
        painter.drawLine(QPointF(10, 12.5), QPointF(10, 15.5));
        painter.drawLine(QPointF(7, 15.5), QPointF(13, 15.5));
    } else {
        painter.drawRoundedRect(QRectF(3, 3, 14, 14), 1.6, 1.6);
        painter.drawLine(QPointF(6, 8), QPointF(8.2, 10.2));
        painter.drawLine(QPointF(6, 12.4), QPointF(8.2, 10.2));
        painter.drawLine(QPointF(10, 12.2), QPointF(14, 12.2));
    }
    return QIcon(pix);
}

void TrayApp::refreshIcon()
{
    Vision::SharedFrames& frames = Vision::SharedFrames::instance();
    const int stationAge = frames.stationBeatAgeMs();
    const int linkAge = frames.linkBeatAgeMs();
    QColor color = kOff;
    if (frames.fault() == Vision::IpcFaultRejected || frames.fault() == Vision::IpcFaultClaim) {
        color = QColor(200, 40, 40);
    } else if (stationAge >= 0 && stationAge < 3000) {
        color = QColor(40, 170, 70);
    } else if ((linkAge >= 0 && linkAge < 3000) || frames.fault() == Vision::IpcFaultLink) {
        color = QColor(210, 170, 30);
    }
    m_tray->setIcon(makeTrayIcon(color));
    bool serviceOn = false;
    if (m_stationSvc || m_linkSvc) {
        serviceOn = true;
        if (m_stationSvc) {
            serviceOn = serviceOn && Vision::ServiceCtl::isRunning(L"Vision.Station");
        }
        if (m_linkSvc) {
            serviceOn = serviceOn && Vision::ServiceCtl::isRunning(L"Vision.Link");
        }
    }
    const bool uiOn = m_window && m_window->isVisible();
    const bool consoleOn = m_console.visible();
    if (m_actService) {
        m_actService->setIcon(makeMenuIcon(0, serviceOn ? kOn : kOff));
    }
    if (m_actUi) {
        m_actUi->setIcon(makeMenuIcon(1, uiOn ? kOn : kOff));
    }
    if (m_actConsole) {
        m_actConsole->setIcon(makeMenuIcon(2, consoleOn ? kOn : kOff));
    }
    const std::string text = frames.faultText();
    if (!text.empty()) {
        m_tray->setToolTip(QString::fromUtf8(text.c_str()));
    } else {
        m_tray->setToolTip(QString::fromUtf8("VisionPlatform"));
    }
}

void TrayApp::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
        openMain();
    }
}

void TrayApp::openMain()
{
    if (!m_window) {
        return;
    }
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}

void TrayApp::openRuntime()
{
    openMain();
    if (m_window) {
        m_window->showRuntimePage();
    }
}

void TrayApp::startOwned()
{
    Vision::SharedFrames::instance().writeCommand(Vision::IpcCmdStartOwned, 0);
}

void TrayApp::stopOwned()
{
    Vision::SharedFrames::instance().writeCommand(Vision::IpcCmdStopOwned, 0);
}

void TrayApp::toggleServices()
{
    if (!m_stationSvc && !m_linkSvc) {
        return;
    }
    const bool stationOn = !m_stationSvc || Vision::ServiceCtl::isRunning(L"Vision.Station");
    const bool linkOn = !m_linkSvc || Vision::ServiceCtl::isRunning(L"Vision.Link");
    if (stationOn && linkOn) {
        stopServices();
    } else {
        startServices();
    }
    refreshIcon();
}

void TrayApp::toggleUi()
{
    if (m_window && m_window->isVisible() && !m_window->isMinimized()) {
        m_window->hide();
    } else {
        openMain();
    }
    refreshIcon();
}

void TrayApp::toggleConsole()
{
    if (m_console.visible()) {
        m_console.hideWindow();
    } else {
        m_console.showWindow();
    }
    refreshIcon();
}

void TrayApp::showServices()
{
    const bool station = Vision::ServiceCtl::isRunning(L"Vision.Station");
    const bool link = Vision::ServiceCtl::isRunning(L"Vision.Link");
    QMessageBox::information(m_window, QString::fromUtf8("服务状态"),
                             QString::fromUtf8("Vision.Station：%1\nVision.Link：%2")
                                 .arg(station ? QString::fromUtf8("运行中") : QString::fromUtf8("未运行"))
                                 .arg(link ? QString::fromUtf8("运行中") : QString::fromUtf8("未运行")));
}

void TrayApp::startServices()
{
    std::wstring err;
    if (m_stationSvc) {
        Vision::ServiceCtl::start(L"Vision.Station", L"Vision.Station.exe", &err);
    }
    if (m_linkSvc) {
        Vision::ServiceCtl::start(L"Vision.Link", L"Vision.Link.exe", &err);
    }
}

void TrayApp::stopServices()
{
    std::wstring err;
    if (m_stationSvc) {
        Vision::ServiceCtl::stop(L"Vision.Station", L"Vision.Station.exe", &err);
    }
    if (m_linkSvc) {
        Vision::ServiceCtl::stop(L"Vision.Link", L"Vision.Link.exe", &err);
    }
}

void TrayApp::quitApp()
{
    if (m_stationSvc || m_linkSvc) {
        Vision::SharedFrames::instance().writeCommand(Vision::IpcCmdStopOwned, 0);
        stopServices();
    }
    if (m_console.visible()) {
        m_console.hideWindow();
    }
    if (m_window) {
        m_window->setForceQuit(true);
    }
    QApplication::quit();
}

void TrayApp::stopAllAndQuit()
{
    quitApp();
}
