/**
 * @file DeskWindow.cpp
 * @brief 单进程远程的首页
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeskWindow.h"
#include "AgentSession.h"
#include "ControlSession.h"
#include "DeskView.h"
#include "FirewallRule.h"
#include "Messages.h"
#include "RelayHub.h"
#include "RemoteBeacon.h"
#include "RemoteFiles.h"
#include "RemotePorts.h"
#include "RemoteTheme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QShowEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Vision {

namespace {

const char* kCardStyle =
    "QFrame#deskCard{background:#FFFFFF;border:1px solid #E8E8E8;border-radius:8px;}"
    "QFrame#deskCard QLabel{background:transparent;}";

}

DeskWindow::DeskWindow(QWidget* parent)
    : QWidget(parent)
    , m_agent(0)
    , m_control(0)
    , m_relay(0)
    , m_view(0)
    , m_agentOn(false)
    , m_lanRule(false)
    , m_timer(0)
    , m_openAgent(0)
    , m_connect(0)
    , m_peerId(0)
    , m_peerCode(0)
    , m_localId(0)
    , m_code(0)
    , m_agentChip(0)
    , m_controlChip(0)
    , m_hint(0)
    , m_recentLay(0)
{
    setWindowTitle(QString::fromUtf8("远程"));
    setObjectName("deskHome");
    setStyleSheet("#deskHome{background:#F5F7FA;}");
    resize(980, 640);
    setMinimumSize(880, 560);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 22, 28, 16);
    root->setSpacing(16);

    const QIcon icon = loadIcon();
    if (!icon.isNull()) {
        setWindowIcon(icon);
        QApplication::setWindowIcon(icon);
    }
    QLabel* logo = new QLabel(this);
    logo->setFixedSize(40, 40);
    logo->setAlignment(Qt::AlignCenter);
    if (icon.isNull()) {
        logo->setPixmap(RemoteTheme::pixmap("monitor", QColor("#FFFFFF"), 22, devicePixelRatioF()));
        logo->setStyleSheet("background:#1677FF;border-radius:8px;");
    } else {
        logo->setPixmap(icon.pixmap(QSize(40, 40)));
        logo->setStyleSheet("background:transparent;");
    }
    QLabel* title = new QLabel(QString::fromUtf8("远程"), this);
    title->setStyleSheet("font-size:22px;font-weight:600;color:#262626;background:transparent;");
    QLabel* sub = new QLabel(QString::fromUtf8("本机代码给对方，对方代码填在右边。画面按原分辨率差分。"), this);
    sub->setStyleSheet("color:#8C8C8C;font-size:12px;background:transparent;");
    QVBoxLayout* titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    titleCol->addWidget(title);
    titleCol->addWidget(sub);
    m_agentChip = new QLabel(this);
    m_controlChip = new QLabel(this);
    m_agentChip->setStyleSheet("color:#8C8C8C;font-size:12px;background:transparent;");
    m_controlChip->setStyleSheet("color:#8C8C8C;font-size:12px;background:transparent;");
    QVBoxLayout* chipCol = new QVBoxLayout();
    chipCol->setSpacing(4);
    chipCol->addWidget(m_agentChip);
    chipCol->addWidget(m_controlChip);
    QHBoxLayout* head = new QHBoxLayout();
    head->setSpacing(12);
    head->addWidget(logo);
    head->addLayout(titleCol);
    head->addStretch(1);
    head->addLayout(chipCol);
    root->addLayout(head);

    QFrame* localCard = qobject_cast<QFrame*>(makeCard());
    QVBoxLayout* localCol = new QVBoxLayout(localCard);
    localCol->setContentsMargins(24, 22, 24, 22);
    localCol->setSpacing(12);
    QLabel* localTitle = new QLabel(QString::fromUtf8("本机"), localCard);
    localTitle->setStyleSheet("font-size:16px;font-weight:600;color:#262626;");
    QLabel* localNote = new QLabel(QString::fromUtf8("开启后把设备代码和下面的验证码发给对方。"), localCard);
    localNote->setWordWrap(true);
    localNote->setStyleSheet("color:#8C8C8C;font-size:12px;");
    QLabel* idKey = new QLabel(QString::fromUtf8("设备代码"), localCard);
    idKey->setStyleSheet("color:#8C8C8C;font-size:12px;");
    const QString localId = QString::fromUtf8(AgentSession::localDeviceId().c_str());
    m_localId = new QLabel(localId, localCard);
    m_localId->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_localId->setStyleSheet("font-size:32px;font-weight:600;letter-spacing:2px;color:#262626;");
    QPushButton* copy = RemoteTheme::button(QString(), "copy", "icon", localCard);
    copy->setToolTip(QString::fromUtf8("复制设备代码"));
    QObject::connect(copy, &QPushButton::clicked, [this]() { copyLocalId(); });
    QHBoxLayout* idRow = new QHBoxLayout();
    idRow->setSpacing(8);
    idRow->addWidget(m_localId);
    idRow->addWidget(copy, 0, Qt::AlignVCenter);
    idRow->addStretch(1);
    QFrame* line = new QFrame(localCard);
    line->setFixedHeight(1);
    line->setStyleSheet("background:#F0F0F0;border:none;");
    m_openAgent = RemoteTheme::button(QString::fromUtf8("开启本机被控"), "app", "primary", localCard);
    m_openAgent->setMinimumHeight(40);
    QObject::connect(m_openAgent, &QPushButton::clicked, [this]() { toggleAgent(); });
    QLabel* codeKey = new QLabel(QString::fromUtf8("一次性验证码"), localCard);
    codeKey->setStyleSheet("color:#8C8C8C;font-size:12px;");
    m_code = new QLabel(QString::fromUtf8("未开启"), localCard);
    m_code->setStyleSheet("font-size:22px;font-weight:600;letter-spacing:3px;color:#262626;");
    localCol->addWidget(localTitle);
    localCol->addWidget(localNote);
    localCol->addSpacing(8);
    localCol->addWidget(idKey);
    localCol->addLayout(idRow);
    localCol->addWidget(codeKey);
    localCol->addWidget(m_code);
    localCol->addWidget(line);
    localCol->addStretch(1);
    localCol->addWidget(m_openAgent);

    QFrame* remoteCard = qobject_cast<QFrame*>(makeCard());
    QVBoxLayout* remoteCol = new QVBoxLayout(remoteCard);
    remoteCol->setContentsMargins(24, 22, 24, 22);
    remoteCol->setSpacing(10);
    QLabel* remoteTitle = new QLabel(QString::fromUtf8("控制远端"), remoteCard);
    remoteTitle->setStyleSheet("font-size:16px;font-weight:600;color:#262626;");
    QLabel* remoteNote = new QLabel(QString::fromUtf8("对方开着本机被控。同一局域网填设备代码就能连，外网还要填验证码。"), remoteCard);
    remoteNote->setWordWrap(true);
    remoteNote->setStyleSheet("color:#8C8C8C;font-size:12px;");
    m_peerId = new QLineEdit(remoteCard);
    m_peerId->setPlaceholderText(QString::fromUtf8("对方设备代码"));
    m_peerId->setMinimumHeight(36);
    m_peerCode = new QLineEdit(remoteCard);
    m_peerCode->setPlaceholderText(QString::fromUtf8("一次性验证码"));
    m_peerCode->setMaxLength(8);
    m_peerCode->setMinimumHeight(36);
    m_connect = RemoteTheme::button(QString::fromUtf8("连接"), "link", "primary", remoteCard);
    m_connect->setMinimumHeight(40);
    QObject::connect(m_connect, &QPushButton::clicked, [this]() { connectPeer(); });
    QObject::connect(m_peerCode, &QLineEdit::returnPressed, [this]() { connectPeer(); });
    QObject::connect(m_peerId, &QLineEdit::returnPressed, [this]() { m_peerCode->setFocus(); });
    QLabel* recentKey = new QLabel(QString::fromUtf8("最近连接"), remoteCard);
    recentKey->setStyleSheet("color:#8C8C8C;font-size:12px;");
    QWidget* recentBox = new QWidget(remoteCard);
    m_recentLay = new QVBoxLayout(recentBox);
    m_recentLay->setContentsMargins(0, 0, 0, 0);
    m_recentLay->setSpacing(6);
    QHBoxLayout* recentHead = new QHBoxLayout();
    recentHead->addWidget(recentKey);
    remoteCol->addWidget(remoteTitle);
    remoteCol->addWidget(remoteNote);
    remoteCol->addWidget(m_peerId);
    remoteCol->addWidget(m_peerCode);
    remoteCol->addWidget(m_connect);
    remoteCol->addSpacing(6);
    remoteCol->addLayout(recentHead);
    remoteCol->addWidget(recentBox, 1);

    QHBoxLayout* body = new QHBoxLayout();
    body->setSpacing(16);
    body->addWidget(localCard, 1);
    body->addWidget(remoteCard, 1);
    root->addLayout(body, 1);

    m_hint = new QLabel(this);
    m_hint->setStyleSheet("color:#8C8C8C;font-size:12px;background:transparent;");
    root->addWidget(m_hint);

    m_timer = new QTimer(this);
    QObject::connect(m_timer, &QTimer::timeout, [this]() { refreshTitles(); });
    m_timer->start(1000);
    refreshTitles();
    refreshRecent();
}

DeskWindow::~DeskWindow()
{
    if (m_agent) {
        m_agent->stop();
        delete m_agent;
    }
    if (m_control) {
        m_control->stop();
        delete m_control;
    }
    if (m_relay) {
        m_relay->stop();
        delete m_relay;
    }
}

QWidget* DeskWindow::makeCard()
{
    QFrame* card = new QFrame(this);
    card->setObjectName("deskCard");
    card->setStyleSheet(kCardStyle);
    return card;
}

void DeskWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshRecent();
    ensureTunnel();
}

void DeskWindow::closeEvent(QCloseEvent* event)
{
    if (m_view) {
        m_view->close();
    }
    QWidget::closeEvent(event);
    QApplication::quit();
}

void DeskWindow::collectPorts(std::vector<int>* tcp, std::vector<int>* udp)
{
    if (!tcp || !udp) {
        return;
    }
    tcp->push_back(RemotePorts::listen());
    tcp->push_back(RemotePorts::direct());
    tcp->push_back(19280);
    udp->push_back(RemotePorts::discover());
}

QIcon DeskWindow::loadIcon()
{
    QIcon icon;
    const int sizes[] = { 16, 24, 32, 48, 64, 128, 256 };
    for (int i = 0; i < 7; ++i) {
        const int side = sizes[i];
        HICON handle = reinterpret_cast<HICON>(LoadImageW(GetModuleHandleW(0), MAKEINTRESOURCEW(1), IMAGE_ICON, side, side, LR_DEFAULTCOLOR));
        if (!handle) {
            continue;
        }
        BITMAPINFO info;
        ZeroMemory(&info, sizeof(info));
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = side;
        info.bmiHeader.biHeight = -side;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = 0;
        HDC screen = GetDC(0);
        HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, 0, 0);
        if (!dib || !bits) {
            if (dib) {
                DeleteObject(dib);
            }
            ReleaseDC(0, screen);
            DestroyIcon(handle);
            continue;
        }
        HDC mem = CreateCompatibleDC(screen);
        HGDIOBJ old = SelectObject(mem, dib);
        DrawIconEx(mem, 0, 0, handle, side, side, 0, 0, DI_NORMAL);
        const QImage image(static_cast<const uchar*>(bits), side, side, side * 4, QImage::Format_ARGB32_Premultiplied);
        icon.addPixmap(QPixmap::fromImage(image.copy()));
        SelectObject(mem, old);
        DeleteDC(mem);
        DeleteObject(dib);
        ReleaseDC(0, screen);
        DestroyIcon(handle);
    }
    return icon;
}

void DeskWindow::copyLocalId()
{
    QApplication::clipboard()->setText(m_localId->text());
    m_hint->setText(QString::fromUtf8("设备代码已复制。"));
}

void DeskWindow::toggleAgent()
{
    if (m_agentOn && m_agent) {
        m_agent->stop();
        m_agentOn = false;
        refreshTitles();
        return;
    }
    if (!m_agent) {
        m_agent = new AgentSession();
    }
    m_agent->setPreferCodec(4);
    m_agent->setAccess(true, "remote.subapi.fun", 443);
    ensureTunnel();
    std::string err;
    if (!m_agent->start("", RemotePorts::direct(), std::wstring(), std::wstring(), true, 0, &err)) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8(err.c_str()));
        return;
    }
    if (!m_agent->setDirect(true, RemotePorts::direct(), &err)) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8(err.c_str()));
        m_agent->stop();
        return;
    }
    m_agentOn = true;
    refreshTitles();
    openLan();
}

void DeskWindow::ensureControl()
{
    if (!m_control) {
        m_control = new ControlSession();
    }
    if (m_control->listening()) {
        return;
    }
    std::string err;
    if (!m_control->start("0.0.0.0", RemotePorts::listen(), &err)) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8(err.c_str()));
    }
}

void DeskWindow::ensureTunnel()
{
    if (!m_relay) {
        m_relay = new RelayHub();
    }
    if (m_relay->wsPort() > 0) {
        return;
    }
    if (!m_relay->startWs(19280)) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8("识别码入口没有听上，外网会连不上。同一局域网仍可以连。"));
    }
}

void DeskWindow::openLan()
{
    if (m_lanRule) {
        return;
    }
    m_lanRule = true;
    FirewallRule rule;
    const std::wstring exe = QCoreApplication::applicationFilePath().toStdWString();
    std::string err;
    const bool tcp = rule.allowTcp(L"RemoteDesk Direct", exe, RemotePorts::direct(), &err);
    const bool udp = rule.allowUdp(L"RemoteDesk Discover", exe, RemotePorts::discover(), &err);
    if ((!tcp || !udp) && m_hint) {
        m_hint->setText(QString::fromUtf8("局域网入站没有放开。用管理员打开一次后，同一局域网才能连进来。外网不受影响。"));
    }
}

bool DeskWindow::openWan(const QString& deviceId, const QString& code, std::string* err)
{
    ensureTunnel();
    for (int attempt = 0; attempt < 5; ++attempt) {
        std::string once;
        if (m_control->dialAccess("remote.subapi.fun", 443, deviceId.toStdString(), code.toStdString(), &once)) {
            return true;
        }
        if (err) {
            *err = once;
        }
        const bool retry = once.find("没挂") != std::string::npos
            || once.find("超时") != std::string::npos
            || once.find("配对") != std::string::npos;
        if (!retry || attempt + 1 >= 5) {
            break;
        }
        QThread::msleep(800);
    }
    return false;
}

void DeskWindow::openView(const QString& deviceId, const QString& code)
{
    if (m_view) {
        m_view->close();
        m_view = 0;
    }
    m_view = new DeskView(m_control, deviceId, code);
    m_view->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(m_view, &QObject::destroyed, [this]() { m_view = 0; });
    m_view->show();
}

void DeskWindow::connectPeer()
{
    QString deviceId = m_peerId->text().trimmed();
    deviceId.remove(' ');
    QString code = m_peerCode->text().trimmed();
    code.remove(' ');
    if (deviceId.isEmpty()) {
        QMessageBox::information(this, windowTitle(), QString::fromUtf8("先填写对方设备代码"));
        return;
    }
    ensureControl();
    if (!m_control || !m_control->listening()) {
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const std::vector<HelloMsg> found = RemoteBeacon::scan(1200);
    std::string address;
    for (size_t i = 0; i < found.size(); ++i) {
        QString id = QString::fromUtf8(found[i].deviceId.c_str());
        id.remove(' ');
        if (id == deviceId && !found[i].address.empty() && found[i].address != "127.0.0.1") {
            address = found[i].address;
            break;
        }
    }
    std::string err;
    bool opened = false;
    if (!address.empty()) {
        std::vector<std::string> order;
        order.push_back("direct");
        opened = m_control->dialDevice(address, deviceId.toStdString(), "", 0, "", false, order, &err);
    }
    if (!opened && !code.isEmpty()) {
        opened = openWan(deviceId, code, &err);
    }
    QApplication::restoreOverrideCursor();
    if (!opened && code.isEmpty()) {
        QMessageBox::information(this, windowTitle(), QString::fromUtf8("局域网没有这台设备。外网连接还要填验证码。"));
        return;
    }
    if (!opened) {
        QMessageBox::warning(this, windowTitle(),
            QString::fromUtf8("没有连上：%1").arg(QString::fromUtf8(err.c_str())));
        return;
    }
    rememberRecent(deviceId);
    openView(deviceId, code);
    refreshTitles();
}

void DeskWindow::refreshTitles()
{
    const bool controlOn = m_control && m_control->listening();
    m_agentChip->setText(m_agentOn ? QString::fromUtf8("● 本机被控已开启") : QString::fromUtf8("● 本机被控未开启"));
    m_agentChip->setStyleSheet(m_agentOn
        ? "color:#52C41A;font-size:12px;background:transparent;"
        : "color:#8C8C8C;font-size:12px;background:transparent;");
    m_controlChip->setText(controlOn ? QString::fromUtf8("● 正在等待远端") : QString::fromUtf8("● 还没有连接"));
    m_controlChip->setStyleSheet(controlOn
        ? "color:#52C41A;font-size:12px;background:transparent;"
        : "color:#8C8C8C;font-size:12px;background:transparent;");
    m_openAgent->setText(m_agentOn ? QString::fromUtf8("停止本机被控") : QString::fromUtf8("开启本机被控"));
    if (m_agentOn && m_agent) {
        QString code = QString::fromUtf8(m_agent->currentCode().c_str());
        if (code.size() == 6) {
            code.insert(3, ' ');
        }
        m_code->setText(code);
    } else if (m_code) {
        m_code->setText(QString::fromUtf8("未开启"));
    }
    if (m_agentOn) {
        m_hint->setText(QString::fromUtf8("本机被控已开。同一局域网填设备代码即可，外网把验证码也发给对方。"));
    } else if (controlOn) {
        m_hint->setText(QString::fromUtf8("正在连接远端，画面在远程自己的窗口里。"));
    } else {
        m_hint->setText(QString::fromUtf8("这一套和原来的中控台、被控端不是同一个程序。"));
    }
}

void DeskWindow::rememberRecent(const QString& deviceId)
{
    QStringList ids;
    ids << deviceId;
    QFile file(RemoteFiles::exeFile("recent.json"));
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray old = QJsonDocument::fromJson(file.readAll()).array();
        file.close();
        for (int i = 0; i < old.size(); ++i) {
            const QString id = old.at(i).toString();
            if (!id.isEmpty() && id != deviceId && !ids.contains(id)) {
                ids << id;
            }
        }
    }
    while (ids.size() > 6) {
        ids.removeLast();
    }
    QJsonArray arr;
    for (int i = 0; i < ids.size(); ++i) {
        arr.append(ids.at(i));
    }
    QSaveFile out(RemoteFiles::exeFile("recent.json"));
    if (!out.open(QIODevice::WriteOnly)) {
        return;
    }
    out.write(QJsonDocument(arr).toJson());
    out.commit();
    refreshRecent();
}

void DeskWindow::refreshRecent()
{
    while (QLayoutItem* child = m_recentLay->takeAt(0)) {
        delete child->widget();
        delete child;
    }
    QStringList ids;
    QFile file(RemoteFiles::exeFile("recent.json"));
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
        file.close();
        for (int i = 0; i < arr.size() && ids.size() < 6; ++i) {
            const QString id = arr.at(i).toString();
            if (!id.isEmpty()) {
                ids << id;
            }
        }
    }
    if (ids.isEmpty()) {
        QLabel* empty = new QLabel(QString::fromUtf8("还没有连接记录。连上之后会出现在这里。"), m_recentLay->parentWidget());
        empty->setStyleSheet("color:#BFBFBF;font-size:12px;");
        empty->setWordWrap(true);
        m_recentLay->addWidget(empty);
        m_recentLay->addStretch(1);
        return;
    }
    for (int i = 0; i < ids.size(); ++i) {
        const QString id = ids.at(i);
        QPushButton* row = new QPushButton(id, m_recentLay->parentWidget());
        row->setCursor(Qt::PointingHandCursor);
        row->setStyleSheet(
            "QPushButton{text-align:left;padding:8px 10px;border:1px solid #F0F0F0;border-radius:6px;background:#FAFAFA;color:#262626;}"
            "QPushButton:hover{border-color:#91CAFF;background:#F0F7FF;}");
        QObject::connect(row, &QPushButton::clicked, [this, id]() {
            m_peerId->setText(id);
            m_peerCode->setFocus();
        });
        m_recentLay->addWidget(row);
    }
    m_recentLay->addStretch(1);
}

} // namespace Vision
