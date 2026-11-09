/**
 * @file main_window.cpp
 * @brief 通讯调试器主窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "main_window.h"
#include "byte_text.h"
#include "channel_page.h"
#include "channel_session.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextCodec>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QPixmap>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

void MainWindow::useExeIcon()
{
    QIcon icon;
    const int sizes[] = { 16, 24, 32, 48, 64, 256 };
    for (int i = 0; i < 6; ++i) {
        const int side = sizes[i];
        HICON handle = reinterpret_cast<HICON>(LoadImageW(
            GetModuleHandleW(0), MAKEINTRESOURCEW(1), IMAGE_ICON, side, side, LR_DEFAULTCOLOR));
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
        HDC mem = CreateCompatibleDC(screen);
        HGDIOBJ old = SelectObject(mem, dib);
        DrawIconEx(mem, 0, 0, handle, side, side, 0, 0, DI_NORMAL);
        const QImage image(static_cast<const uchar*>(bits), side, side, side * 4,
            QImage::Format_ARGB32_Premultiplied);
        icon.addPixmap(QPixmap::fromImage(image.copy()));
        SelectObject(mem, old);
        DeleteDC(mem);
        DeleteObject(dib);
        ReleaseDC(0, screen);
        DestroyIcon(handle);
    }
    if (icon.isNull()) {
        return;
    }
    QApplication::setWindowIcon(icon);
    setWindowIcon(icon);
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_kindGuard(false)
    , m_cycleIndex(0)
    , m_dump(0)
    , m_waitOpen(false)
    , m_wasOpen(false)
    , m_fileBusy(false)
    , m_filePos(0)
    , m_openedAt(0)
    , m_lastDtr(true)
    , m_lastRts(true)
    , m_httpCode(0)
    , m_httpMs(0)
{
    useExeIcon();
    setWindowTitle(QString::fromUtf8("通讯调试器"));
    resize(1180, 860);

    m_kind = new QComboBox(this);
    m_kind->setMinimumWidth(180);
    m_kind->addItems(QStringList()
        << QString::fromUtf8("串口")
        << QString::fromUtf8("TCP客户端")
        << QString::fromUtf8("TCP服务端")
        << QString::fromUtf8("UDP")
        << QString::fromUtf8("HTTP")
        << QString::fromUtf8("WebSocket")
        << QString::fromUtf8("Modbus RTU")
        << QString::fromUtf8("Modbus TCP"));
    m_pages = new QStackedWidget(this);
    m_pages->addWidget(createSerialPage(m_pages));
    m_pages->addWidget(createTcpClientPage(m_pages));
    m_pages->addWidget(createTcpServerPage(m_pages));
    m_pages->addWidget(createUdpPage(m_pages));
    m_pages->addWidget(createHttpPage(m_pages));
    m_pages->addWidget(createWebSocketPage(m_pages));
    m_pages->addWidget(createModbusRtuPage(m_pages));
    m_pages->addWidget(createModbusTcpPage(m_pages));

    m_open = new QPushButton(QString::fromUtf8("打开"), this);
    m_open->setObjectName("primary");
    m_close = new QPushButton(QString::fromUtf8("关闭"), this);
    m_close->setEnabled(false);
    m_chip = new QLabel(QString::fromUtf8("未连接"), this);
    m_chip->setObjectName("chipOff");

    m_recv = new QPlainTextEdit(this);
    m_recv->setObjectName("message");
    m_recv->setReadOnly(true);
    m_recv->setPlaceholderText(QString::fromUtf8("报文"));
    m_recvTitle = new QLabel(QString::fromUtf8("报文"), this);
    m_recvTitle->setObjectName("panelTitle");
    m_hexView = new QCheckBox(QString::fromUtf8("HEX显示"), this);
    m_hexView->setChecked(true);
    m_stamp = new QCheckBox(QString::fromUtf8("时间戳"), this);
    m_stamp->setChecked(true);
    m_pause = new QCheckBox(QString::fromUtf8("暂停显示"), this);
    m_scroll = new QCheckBox(QString::fromUtf8("自动滚屏"), this);
    m_scroll->setChecked(true);
    QSpinBox* gap = new QSpinBox(this);
    gap->setRange(1, 1000);
    gap->setValue(20);
    gap->setSuffix(" ms");

    m_log = new QPlainTextEdit(this);
    m_log->setObjectName("log");
    m_log->setReadOnly(true);
    m_log->setFixedHeight(72);

    m_send = new QPlainTextEdit(this);
    m_send->setObjectName("message");
    m_send->setFixedHeight(56);
    m_send->setPlaceholderText("01 03 00 00 00 0A");
    m_hexSend = new QCheckBox(QString::fromUtf8("HEX发送"), this);
    m_hexSend->setChecked(true);
    m_crlf = new QCheckBox(QString::fromUtf8("加回车换行"), this);
    m_at = new QCheckBox(QString::fromUtf8("AT自动回车"), this);
    m_escape = new QCheckBox(QString::fromUtf8("转义"), this);
    m_check = new QComboBox(this);
    m_check->addItems(QStringList() << QString::fromUtf8("不加") << QString::fromUtf8("累加和") << QString::fromUtf8("异或") << "CRC16");
    m_checkFrom = new QSpinBox(this);
    m_checkFrom->setRange(1, 4096);
    m_checkTail = new QSpinBox(this);
    m_checkTail->setRange(0, 64);
    m_codec = new QComboBox(this);
    m_codec->addItems(QStringList() << "UTF-8" << "GBK");
    m_history = new QComboBox(this);
    m_history->setEditable(false);
    m_auto = new QCheckBox(QString::fromUtf8("定时发送"), this);
    m_interval = new QSpinBox(this);
    m_interval->setRange(20, 60000);
    m_interval->setValue(1000);
    m_interval->setSuffix(" ms");
    m_fileSend = new QPushButton(QString::fromUtf8("发送文件"), this);
    m_fileGap = new QComboBox(this);
    m_fileGap->addItem("0 ms", 0);
    m_fileGap->addItem("1 ms", 1);
    m_fileGap->addItem("10 ms", 10);
    m_fileGap->addItem("50 ms", 50);
    m_fileGap->addItem("100 ms", 100);
    m_cycle = new QCheckBox(QString::fromUtf8("循环多条"), this);
    m_wsDrop = new QPushButton(QString::fromUtf8("断开"), this);

    m_presetCount = new QSpinBox(this);
    m_presetCount->setRange(1, 64);
    m_presetCount->setValue(1);
    m_presetCount->setSuffix(QString::fromUtf8(" 条"));

    m_method = new QComboBox(this);
    m_method->addItems(QStringList() << "GET" << "POST" << "PUT" << "DELETE");
    m_headers = new QTableWidget(0, 2, this);
    m_headers->setHorizontalHeaderLabels(QStringList() << QString::fromUtf8("名字") << QString::fromUtf8("值"));
    m_headers->horizontalHeader()->setStretchLastSection(true);
    m_headers->setFixedHeight(120);
    m_body = new QPlainTextEdit(this);
    m_body->setObjectName("message");
    m_body->setFixedHeight(72);
    m_httpHistory = new QComboBox(this);

    m_slave = new QSpinBox(this);
    m_slave->setRange(0, 247);
    m_slave->setValue(1);
    m_fn = new QComboBox(this);
    m_fn->addItem(QString::fromUtf8("01 读线圈"), 1);
    m_fn->addItem(QString::fromUtf8("02 读离散"), 2);
    m_fn->addItem(QString::fromUtf8("03 读保持寄存器"), 3);
    m_fn->addItem(QString::fromUtf8("04 读输入"), 4);
    m_fn->setCurrentIndex(2);
    m_address = new QSpinBox(this);
    m_address->setRange(0, 65535);
    m_count = new QSpinBox(this);
    m_count->setRange(1, 125);
    m_count->setValue(10);
    m_period = new QSpinBox(this);
    m_period->setRange(0, 60000);
    m_period->setValue(1000);
    m_period->setSuffix(" ms");
    m_display = new QComboBox(this);
    m_display->addItems(QStringList() << QString::fromUtf8("无符号") << QString::fromUtf8("有符号") << QString::fromUtf8("十六进制"));
    m_poll = new QPushButton(QString::fromUtf8("轮询"), this);
    m_poll->setObjectName("primary");
    m_regs = new QTableWidget(0, 3, this);
    m_regs->setHorizontalHeaderLabels(QStringList() << QString::fromUtf8("地址") << QString::fromUtf8("名称") << QString::fromUtf8("值"));
    m_regs->horizontalHeader()->setStretchLastSection(true);
    m_regs->setMinimumHeight(160);

    m_peers = new QTableWidget(0, 3, this);
    m_peers->setHorizontalHeaderLabels(QStringList() << QString::fromUtf8("地址") << QString::fromUtf8("连上时间") << QString::fromUtf8("最后收到"));
    m_peers->horizontalHeader()->setStretchLastSection(true);
    m_peers->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_peers->setSelectionMode(QAbstractItemView::SingleSelection);
    m_target = new QComboBox(this);

    m_session = new ChannelSession();
    m_session->start();
    m_timer = new QTimer(this);
    m_autoTimer = new QTimer(this);
    m_cycleTimer = new QTimer(this);
    m_pollTimer = new QTimer(this);

    QWidget* central = new QWidget(this);
    QVBoxLayout* leftLay = new QVBoxLayout(central);
    leftLay->setContentsMargins(8, 8, 8, 8);
    leftLay->setSpacing(6);
    QHBoxLayout* openRow = new QHBoxLayout();
    QLabel* kindLabel = new QLabel(QString::fromUtf8("通讯方式"), central);
    openRow->addWidget(kindLabel);
    openRow->addWidget(m_kind);
    openRow->addSpacing(16);
    openRow->addWidget(m_open);
    openRow->addWidget(m_close);
    openRow->addWidget(m_chip);
    openRow->addStretch(1);
    leftLay->addLayout(openRow);

    QFrame* paramBox = new QFrame(central);
    paramBox->setObjectName("panel");
    QVBoxLayout* paramLay = new QVBoxLayout(paramBox);
    paramLay->setContentsMargins(0, 0, 0, 0);
    paramLay->addWidget(m_pages);
    leftLay->addWidget(paramBox);

    m_modbusBar = new QFrame(central);
    m_modbusBar->setObjectName("panel");
    QVBoxLayout* modLay = new QVBoxLayout(m_modbusBar);
    modLay->setContentsMargins(8, 8, 8, 8);
    modLay->setSpacing(6);
    QGridLayout* poll = new QGridLayout();
    poll->setHorizontalSpacing(8);
    poll->setVerticalSpacing(2);
    const QString pollLabels[] = { QString::fromUtf8("从站"), QString::fromUtf8("功能"), QString::fromUtf8("地址"), QString::fromUtf8("数量"), QString::fromUtf8("周期"), QString::fromUtf8("显示") };
    QWidget* pollFields[] = { m_slave, m_fn, m_address, m_count, m_period, m_display };
    for (int i = 0; i < 6; ++i) {
        QLabel* lab = new QLabel(pollLabels[i], m_modbusBar);
        lab->setObjectName("hint");
        poll->addWidget(lab, 0, i);
        poll->addWidget(pollFields[i], 1, i);
    }
    QPushButton* once = new QPushButton(QString::fromUtf8("读一次"), m_modbusBar);
    QPushButton* writeAll = new QPushButton(QString::fromUtf8("写入"), m_modbusBar);
    poll->addWidget(once, 1, 6);
    poll->addWidget(m_poll, 1, 7);
    poll->addWidget(writeAll, 1, 8);
    QLabel* addrHint = new QLabel(QString::fromUtf8("协议地址从 0 起。设备地址 40001 填 0"), m_modbusBar);
    addrHint->setObjectName("hint");
    modLay->addLayout(poll);
    modLay->addWidget(addrHint);
    modLay->addWidget(m_regs, 1);
    m_modbusBar->setVisible(false);
    leftLay->addWidget(m_modbusBar, 1);

    QFrame* msgBox = new QFrame(central);
    msgBox->setObjectName("panel");
    QVBoxLayout* msgLay = new QVBoxLayout(msgBox);
    msgLay->setContentsMargins(8, 8, 8, 8);
    msgLay->setSpacing(6);
    msgLay->addWidget(m_recvTitle);
    msgLay->addWidget(m_recv, 1);
    QHBoxLayout* viewRow = new QHBoxLayout();
    viewRow->addWidget(m_hexView);
    viewRow->addWidget(m_stamp);
    viewRow->addWidget(m_pause);
    viewRow->addWidget(m_scroll);
    viewRow->addWidget(new QLabel(QString::fromUtf8("分包"), msgBox));
    viewRow->addWidget(gap);
    viewRow->addStretch(1);
    QPushButton* clearRecv = new QPushButton(QString::fromUtf8("清空"), msgBox);
    viewRow->addWidget(clearRecv);
    msgLay->addLayout(viewRow);
    leftLay->addWidget(msgBox, 2);

    QFrame* logBox = makePanel(QString::fromUtf8("日志"), m_log, central);
    QPushButton* clearLog = new QPushButton(QString::fromUtf8("清空日志"), logBox);
    static_cast<QVBoxLayout*>(logBox->layout())->addWidget(clearLog, 0, Qt::AlignRight);
    leftLay->addWidget(logBox);

    m_byteSend = new QFrame(central);
    m_byteSend->setObjectName("panel");
    QVBoxLayout* sendLay = new QVBoxLayout(m_byteSend);
    sendLay->setContentsMargins(8, 8, 8, 8);
    sendLay->setSpacing(6);
    QLabel* sendTitle = new QLabel(QString::fromUtf8("发送"), m_byteSend);
    sendTitle->setObjectName("panelTitle");
    sendLay->addWidget(sendTitle);
    QHBoxLayout* opt = new QHBoxLayout();
    opt->addWidget(m_hexSend);
    opt->addWidget(m_crlf);
    opt->addWidget(m_at);
    opt->addWidget(m_escape);
    opt->addWidget(m_codec);
    opt->addWidget(new QLabel(QString::fromUtf8("校验"), m_byteSend));
    opt->addWidget(m_check);
    opt->addWidget(new QLabel(QString::fromUtf8("从第"), m_byteSend));
    opt->addWidget(m_checkFrom);
    opt->addWidget(new QLabel(QString::fromUtf8("字节，末尾跳过"), m_byteSend));
    opt->addWidget(m_checkTail);
    opt->addStretch(1);
    sendLay->addLayout(opt);
    QHBoxLayout* sendRow = new QHBoxLayout();
    sendRow->addWidget(m_send, 1);
    QVBoxLayout* sendBtns = new QVBoxLayout();
    QPushButton* sendBtn = new QPushButton(QString::fromUtf8("发送"), m_byteSend);
    sendBtn->setObjectName("primary");
    sendBtns->addWidget(m_history);
    sendBtns->addWidget(m_auto);
    sendBtns->addWidget(m_interval);
    sendBtns->addWidget(m_fileSend);
    sendBtns->addWidget(m_fileGap);
    sendBtns->addWidget(m_wsDrop);
    sendBtns->addWidget(sendBtn);
    sendRow->addLayout(sendBtns);
    sendLay->addLayout(sendRow);
    QScrollArea* presetScroll = new QScrollArea(m_byteSend);
    presetScroll->setWidgetResizable(true);
    presetScroll->setFixedHeight(132);
    presetScroll->setFrameShape(QFrame::NoFrame);
    m_presetBody = new QWidget(presetScroll);
    m_presetGrid = new QGridLayout(m_presetBody);
    m_presetGrid->setContentsMargins(0, 0, 0, 0);
    m_presetGrid->setHorizontalSpacing(6);
    m_presetGrid->setColumnStretch(2, 1);
    presetScroll->setWidget(m_presetBody);
    QHBoxLayout* cycleRow = new QHBoxLayout();
    cycleRow->addWidget(new QLabel(QString::fromUtf8("多条发送"), m_byteSend));
    cycleRow->addWidget(m_cycle);
    cycleRow->addSpacing(12);
    cycleRow->addWidget(new QLabel(QString::fromUtf8("数量"), m_byteSend));
    cycleRow->addWidget(m_presetCount);
    cycleRow->addStretch(1);
    sendLay->addLayout(cycleRow);
    sendLay->addWidget(presetScroll);
    leftLay->addWidget(m_byteSend);

    m_httpSend = new QFrame(central);
    m_httpSend->setObjectName("panel");
    QVBoxLayout* httpLay = new QVBoxLayout(m_httpSend);
    httpLay->setContentsMargins(8, 8, 8, 8);
    httpLay->setSpacing(6);
    QLabel* httpTitle = new QLabel(QString::fromUtf8("请求"), m_httpSend);
    httpTitle->setObjectName("panelTitle");
    httpLay->addWidget(httpTitle);
    QHBoxLayout* httpTop = new QHBoxLayout();
    httpTop->addWidget(m_method);
    httpTop->addWidget(m_httpHistory, 1);
    QPushButton* addHeader = new QPushButton(QString::fromUtf8("加请求头"), m_httpSend);
    QPushButton* delHeader = new QPushButton(QString::fromUtf8("删请求头"), m_httpSend);
    QPushButton* httpBtn = new QPushButton(QString::fromUtf8("发送"), m_httpSend);
    httpBtn->setObjectName("primary");
    httpTop->addWidget(addHeader);
    httpTop->addWidget(delHeader);
    httpTop->addWidget(httpBtn);
    httpLay->addLayout(httpTop);
    httpLay->addWidget(m_headers);
    httpLay->addWidget(m_body);
    m_httpSend->setVisible(false);
    leftLay->addWidget(m_httpSend);

    m_peerBox = makePanel(QString::fromUtf8("客户端"), m_peers, this);
    QHBoxLayout* peerBtns = new QHBoxLayout();
    QPushButton* kick = new QPushButton(QString::fromUtf8("断开选中"), m_peerBox);
    peerBtns->addStretch(1);
    peerBtns->addWidget(kick);
    static_cast<QVBoxLayout*>(m_peerBox->layout())->addLayout(peerBtns);
    QHBoxLayout* targetRow = new QHBoxLayout();
    targetRow->addWidget(new QLabel(QString::fromUtf8("发送目标"), m_peerBox));
    targetRow->addWidget(m_target, 1);
    static_cast<QVBoxLayout*>(m_peerBox->layout())->insertLayout(1, targetRow);
    m_peerBox->setVisible(false);
    m_peerBox->setMinimumWidth(280);

    QSplitter* split = new QSplitter(this);
    split->addWidget(central);
    split->addWidget(m_peerBox);
    split->setStretchFactor(0, 1);
    setCentralWidget(split);

    m_where = new QLabel(this);
    m_counts = new QLabel(this);
    statusBar()->addWidget(m_where, 1);
    statusBar()->addPermanentWidget(m_counts);
    QPushButton* zero = new QPushButton(QString::fromUtf8("计数清零"), this);
    statusBar()->addPermanentWidget(zero);

    buildMenu();
    loadDesk();
    showPage(0);
    refreshCounts();

    connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) { onKind(i); });
    connect(m_open, &QPushButton::clicked, this, [this]() { onOpen(); });
    connect(m_close, &QPushButton::clicked, this, [this]() { onClose(); });
    connect(sendBtn, &QPushButton::clicked, this, [this]() { onSend(); });
    connect(httpBtn, &QPushButton::clicked, this, [this]() { onHttp(); });
    connect(clearRecv, &QPushButton::clicked, m_recv, &QPlainTextEdit::clear);
    connect(clearLog, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);
    connect(zero, &QPushButton::clicked, this, [this]() { m_session->resetStats(); refreshCounts(); });
    connect(m_timer, &QTimer::timeout, this, [this]() { drain(); });
    connect(m_auto, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            m_autoTimer->start(m_interval->value());
        } else {
            m_autoTimer->stop();
        }
    });
    connect(m_interval, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_auto->isChecked()) {
            m_autoTimer->start(v);
        }
    });
    connect(m_autoTimer, &QTimer::timeout, this, [this]() { onSend(); });
    connect(m_cycle, &QCheckBox::toggled, this, [this](bool on) {
        m_cycleIndex = 0;
        if (on) {
            cyclePresets();
        } else {
            m_cycleTimer->stop();
        }
    });
    connect(m_cycleTimer, &QTimer::timeout, this, [this]() { cyclePresets(); });
    connect(m_presetCount, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int n) { setPresetCount(n); });
    connect(m_history, QOverload<int>::of(&QComboBox::activated), this, [this](int i) {
        m_send->setPlainText(m_history->itemText(i));
    });
    connect(m_hexView, &QCheckBox::toggled, this, [this](bool on) { m_session->setView(on, m_stamp->isChecked()); });
    connect(m_stamp, &QCheckBox::toggled, this, [this](bool on) { m_session->setView(m_hexView->isChecked(), on); });
    connect(gap, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) { m_session->setGap(v); });
    connect(m_fileSend, &QPushButton::clicked, this, [this]() {
        if (m_fileBusy) {
            m_fileBusy = false;
            m_fileSend->setText(QString::fromUtf8("发送文件"));
            return;
        }
        const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("发送文件"));
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            writeLog(QString::fromUtf8("读文件失败"));
            return;
        }
        m_fileBytes = file.readAll();
        m_filePos = 0;
        m_fileBusy = true;
        m_fileSend->setText(QString::fromUtf8("停止"));
        sendFileChunk();
    });
    connect(m_wsDrop, &QPushButton::clicked, this, [this]() {
        m_session->requestKick(QString());
        m_chip->setText(QString::fromUtf8("已断开"));
        m_chip->setObjectName("chipOff");
        polish(m_chip);
        writeLog(QString::fromUtf8("正在断开"));
    });
    connect(once, &QPushButton::clicked, this, [this]() { readOnce(); });
    connect(m_poll, &QPushButton::clicked, this, [this]() { pollModbus(); });
    connect(writeAll, &QPushButton::clicked, this, [this]() { writeTable(false, -1); });
    connect(m_pollTimer, &QTimer::timeout, this, [this]() { readOnce(); });
    connect(m_fn, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const int fn = m_fn->currentData().toInt();
        m_count->setMaximum(fn <= 2 ? 2000 : 125);
        m_display->setEnabled(fn >= 3);
    });
    connect(m_method, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString method = m_method->currentText();
        m_body->setEnabled(method == "POST" || method == "PUT");
    });
    connect(addHeader, &QPushButton::clicked, this, [this]() {
        const int row = m_headers->rowCount();
        m_headers->insertRow(row);
        m_headers->setItem(row, 0, new QTableWidgetItem());
        m_headers->setItem(row, 1, new QTableWidgetItem());
    });
    connect(delHeader, &QPushButton::clicked, this, [this]() { m_headers->removeRow(m_headers->currentRow()); });
    connect(m_regs, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (col == 2) {
            writeTable(true, row);
        }
    });
    connect(m_peers, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int row = m_peers->currentRow();
        if (row < 0) {
            return;
        }
        const QString id = m_peers->item(row, 0)->data(Qt::UserRole).toString();
        const int at = m_target->findData(id);
        if (at >= 0) {
            m_target->setCurrentIndex(at);
        }
    });
    connect(kick, &QPushButton::clicked, this, [this]() {
        const int row = m_peers->currentRow();
        if (row < 0) {
            writeLog(QString::fromUtf8("没有选中客户端"));
            return;
        }
        m_session->requestKick(m_peers->item(row, 0)->data(Qt::UserRole).toString());
    });
    QShortcut* key = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return), this);
    connect(key, &QShortcut::activated, this, [this]() {
        if (currentPage() && currentPage()->http()) {
            onHttp();
        } else if (currentPage() && !currentPage()->modbus()) {
            onSend();
        }
    });
    m_session->setView(true, true);
    m_session->setGap(20);
    m_timer->start(50);
}

MainWindow::~MainWindow()
{
    saveDesk();
    m_autoTimer->stop();
    m_cycleTimer->stop();
    m_pollTimer->stop();
    m_fileBusy = false;
    m_session->requestClose();
    delete m_dump;
    delete m_session;
}

QFrame* MainWindow::makePanel(const QString& title, QWidget* body, QWidget* parent)
{
    QFrame* box = new QFrame(parent);
    box->setObjectName("panel");
    QVBoxLayout* lay = new QVBoxLayout(box);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);
    if (!title.isEmpty()) {
        QLabel* lab = new QLabel(title, box);
        lab->setObjectName("panelTitle");
        lay->addWidget(lab);
    }
    if (body) {
        lay->addWidget(body, 1);
    }
    return box;
}

void MainWindow::polish(QLabel* lab)
{
    if (!lab) {
        return;
    }
    lab->style()->unpolish(lab);
    lab->style()->polish(lab);
}

void MainWindow::buildMenu()
{
    QMenu* file = menuBar()->addMenu(QString::fromUtf8("文件"));
    file->addAction(QString::fromUtf8("保存报文"), this, [this]() { saveText(m_recv, QString::fromUtf8("保存报文")); });
    file->addAction(QString::fromUtf8("保存日志"), this, [this]() { saveText(m_log, QString::fromUtf8("保存日志")); });
    QAction* dump = file->addAction(QString::fromUtf8("接收另存为文件"));
    dump->setCheckable(true);
    connect(dump, &QAction::toggled, this, [this](bool on) {
        delete m_dump;
        m_dump = 0;
        if (!on) {
            return;
        }
        const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("接收另存为文件"));
        if (path.isEmpty()) {
            return;
        }
        m_dump = new QFile(path);
        if (!m_dump->open(QIODevice::WriteOnly | QIODevice::Append)) {
            delete m_dump;
            m_dump = 0;
            writeLog(QString::fromUtf8("无法写入接收文件"));
        }
    });
    file->addSeparator();
    file->addAction(QString::fromUtf8("退出"), this, [this]() { close(); });
    QMenu* edit = menuBar()->addMenu(QString::fromUtf8("编辑"));
    edit->addAction(QString::fromUtf8("清空报文"), m_recv, &QPlainTextEdit::clear);
    edit->addAction(QString::fromUtf8("清空日志"), m_log, &QPlainTextEdit::clear);
    edit->addAction(QString::fromUtf8("计数清零"), this, [this]() { m_session->resetStats(); refreshCounts(); });
    QMenu* view = menuBar()->addMenu(QString::fromUtf8("查看"));
    QAction* hex = view->addAction(QString::fromUtf8("HEX显示"));
    hex->setCheckable(true);
    hex->setChecked(true);
    connect(hex, &QAction::toggled, m_hexView, &QCheckBox::setChecked);
    connect(m_hexView, &QCheckBox::toggled, hex, &QAction::setChecked);
    QAction* stamp = view->addAction(QString::fromUtf8("时间戳"));
    stamp->setCheckable(true);
    stamp->setChecked(true);
    connect(stamp, &QAction::toggled, m_stamp, &QCheckBox::setChecked);
    connect(m_stamp, &QCheckBox::toggled, stamp, &QAction::setChecked);
    QAction* pause = view->addAction(QString::fromUtf8("暂停显示"));
    pause->setCheckable(true);
    connect(pause, &QAction::toggled, m_pause, &QCheckBox::setChecked);
    connect(m_pause, &QCheckBox::toggled, pause, &QAction::setChecked);
    QAction* scroll = view->addAction(QString::fromUtf8("自动滚屏"));
    scroll->setCheckable(true);
    scroll->setChecked(true);
    connect(scroll, &QAction::toggled, m_scroll, &QCheckBox::setChecked);
    connect(m_scroll, &QCheckBox::toggled, scroll, &QAction::setChecked);
    QMenu* help = menuBar()->addMenu(QString::fromUtf8("帮助"));
    help->addAction(QString::fromUtf8("关于"), this, [this]() {
        QMessageBox::about(this, QString::fromUtf8("通讯调试器"),
            QString::fromUtf8("用来核对串口、网络和 Modbus。图标来自 Lucide，ISC 许可。"));
    });
}

void MainWindow::onKind(int index)
{
    if (m_kindGuard || index < 0 || index == m_pages->currentIndex()) {
        return;
    }
    if (m_close->isEnabled()) {
        const int answer = QMessageBox::question(this, QString::fromUtf8("切换通讯方式"),
            QString::fromUtf8("当前通讯还没有关闭。确认关闭后再切换到「%1」？").arg(m_kind->itemText(index)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            m_kindGuard = true;
            m_kind->setCurrentIndex(m_pages->currentIndex());
            m_kindGuard = false;
            return;
        }
        onClose();
    }
    showPage(index);
    clearSessionView();
}

void MainWindow::clearSessionView()
{
    m_recv->clear();
    m_log->clear();
    m_send->clear();
    m_body->clear();
    m_history->clear();
    m_httpHistory->clear();
    m_regs->setRowCount(0);
    m_peers->setRowCount(0);
    m_target->clear();
    m_target->addItem(QString::fromUtf8("全部"), "*");
    m_fileBusy = false;
    m_fileBytes.clear();
    m_filePos = 0;
    m_fileSend->setText(QString::fromUtf8("发送文件"));
    m_httpCode = 0;
    m_httpMs = 0;
    m_session->resetStats();
    refreshCounts();
}

void MainWindow::showPage(int index)
{
    m_pages->setCurrentIndex(index);
    ChannelPage* page = currentPage();
    const bool http = page && page->http();
    const bool modbus = page && page->modbus();
    const bool ws = page && page->webSocket();
    m_byteSend->setVisible(!http && !modbus);
    m_httpSend->setVisible(http);
    m_modbusBar->setVisible(modbus);
    m_fileSend->setVisible(!ws);
    m_fileGap->setVisible(!ws);
    m_wsDrop->setVisible(ws);
    m_peerBox->setVisible(page && page->server());
    m_recvTitle->setText(modbus ? QString::fromUtf8("通信帧") : QString::fromUtf8("报文"));
    m_hexView->setVisible(!http && !modbus);
    refreshCounts();
}

ChannelPage* MainWindow::currentPage() const
{
    return static_cast<ChannelPage*>(m_pages->currentWidget());
}

void MainWindow::onOpen()
{
    ChannelPage* page = currentPage();
    if (!page) {
        return;
    }
    Vision::CommChannel channel;
    page->fill(&channel);
    m_session->requestOpen(channel, page->stream(), page->webSocket());
    m_waitOpen = true;
    m_openedAt = 0;
    m_httpCode = 0;
    m_lastUdp = page->udp() ? page->udpHost() + ":" + QString::number(page->udpPort()) : QString();
    m_lastDtr = page->dtr();
    m_lastRts = page->rts();
    page->setLocked(true);
    m_open->setEnabled(false);
    m_close->setEnabled(true);
    writeLog(QString::fromUtf8("正在打开 ") + m_kind->currentText());
}

void MainWindow::onClose()
{
    m_auto->setChecked(false);
    m_cycle->setChecked(false);
    m_pollTimer->stop();
    m_poll->setText(QString::fromUtf8("轮询"));
    m_fileBusy = false;
    m_fileSend->setText(QString::fromUtf8("发送文件"));
    m_session->requestClose();
    m_waitOpen = false;
    m_wasOpen = false;
    if (ChannelPage* page = currentPage()) {
        page->setLocked(false);
    }
    m_open->setEnabled(true);
    m_close->setEnabled(false);
    m_chip->setText(QString::fromUtf8("未连接"));
    m_chip->setObjectName("chipOff");
    polish(m_chip);
    writeLog(QString::fromUtf8("正在关闭"));
    refreshCounts();
}

bool MainWindow::buildPayload(const QString& text, bool hex, QByteArray* out, QString* err) const
{
    if (!out) {
        return false;
    }
    if (hex) {
        if (!ByteText::fromHex(text, out, err)) {
            return false;
        }
    } else {
        QString body = m_escape->isChecked() ? ByteText::unescape(text) : text;
        if (m_crlf->isChecked() && !body.endsWith("\r\n")) {
            body += "\r\n";
        }
        if (m_at->isChecked() && body.startsWith("AT") && !body.endsWith("\r\n")) {
            body += "\r\n";
        }
        QTextCodec* codec = QTextCodec::codecForName(m_codec->currentText().toUtf8());
        *out = codec ? codec->fromUnicode(body) : body.toUtf8();
    }
    ByteText::appendCheck(out, m_check->currentIndex(), m_checkFrom->value(), m_checkTail->value());
    return true;
}

void MainWindow::appendMessage(const QString& text)
{
    if (m_pause->isChecked()) {
        return;
    }
    m_recv->appendPlainText(text);
    if (m_recv->toPlainText().size() > 200000) {
        QString left = m_recv->toPlainText().right(100000);
        m_recv->setPlainText(left);
        writeLog(QString::fromUtf8("报文超过 200KB，已丢掉最旧的内容"));
    }
    if (m_scroll->isChecked()) {
        m_recv->moveCursor(QTextCursor::End);
    }
}

void MainWindow::writeLog(const QString& text)
{
    m_log->appendPlainText(ByteText::stamp() + "  " + text);
}

void MainWindow::dispatchBytes(const QByteArray& bytes, const QString& shown)
{
    ChannelPage* page = currentPage();
    if (!page) {
        return;
    }
    if (page->server()) {
        const QString target = m_target->currentData().toString();
        if (m_target->currentIndex() < 0) {
            writeLog(QString::fromUtf8("没有选中发送目标"));
            return;
        }
        if (target == "*") {
            for (int i = 0; i < m_peers->rowCount(); ++i) {
                const QString id = m_peers->item(i, 0)->data(Qt::UserRole).toString();
                const QString remote = m_peers->item(i, 0)->text();
                m_session->requestWrite(bytes, id);
                appendMessage(ByteText::stamp() + QString::fromUtf8("  发→ ") + remote + "  " + shown);
            }
        } else {
            m_session->requestWrite(bytes, target);
            appendMessage(ByteText::stamp() + QString::fromUtf8("  发→ ") + m_target->currentText() + "  " + shown);
        }
    } else if (page->webSocket()) {
        const bool text = !m_hexSend->isChecked();
        m_session->requestWs(bytes, text);
        const QString kind = text ? QString::fromUtf8("文本") : QString::fromUtf8("二进制");
        appendMessage(ByteText::stamp() + QString::fromUtf8("  发→ ") + kind
            + QString::fromUtf8(" %1 字节 ").arg(bytes.size()) + shown);
    } else {
        QString peer;
        if (page->udp()) {
            peer = page->udpHost() + ":" + QString::number(page->udpPort());
        } else {
            peer = page->summary();
        }
        m_session->requestWrite(bytes, QString());
        appendMessage(ByteText::stamp() + QString::fromUtf8("  发→ ") + peer + "  " + shown);
    }
    if (m_dump && !bytes.isEmpty()) {
        m_dump->write(bytes);
    }
    writeLog(QString::fromUtf8("发送 %1 字节").arg(bytes.size()));
}

void MainWindow::onSend()
{
    QByteArray bytes;
    QString err;
    if (!buildPayload(m_send->toPlainText(), m_hexSend->isChecked(), &bytes, &err)) {
        writeLog(err);
        return;
    }
    if (sender() != m_autoTimer && sender() != m_cycleTimer) {
        noteHistory(m_send->toPlainText());
    }
    const QString shown = m_hexSend->isChecked() ? ByteText::toHex(bytes) : m_send->toPlainText();
    dispatchBytes(bytes, shown);
}

void MainWindow::noteHistory(const QString& text)
{
    if (text.trimmed().isEmpty()) {
        return;
    }
    const int at = m_history->findText(text);
    if (at >= 0) {
        m_history->removeItem(at);
    }
    m_history->insertItem(0, text);
    while (m_history->count() > 32) {
        m_history->removeItem(m_history->count() - 1);
    }
}

void MainWindow::setPresetCount(int count)
{
    if (count < 1) {
        count = 1;
    }
    if (count > 64) {
        count = 64;
    }
    while (m_presetName.size() > count) {
        const int i = m_presetName.size() - 1;
        delete m_presetName[i];
        delete m_presetHex[i];
        delete m_presetEdit[i];
        delete m_presetMs[i];
        delete m_presetSend[i];
        m_presetName.removeLast();
        m_presetHex.removeLast();
        m_presetEdit.removeLast();
        m_presetMs.removeLast();
        m_presetSend.removeLast();
    }
    while (m_presetName.size() < count) {
        const int i = m_presetName.size();
        QLineEdit* name = new QLineEdit(m_presetBody);
        name->setPlaceholderText(QString::fromUtf8("名称"));
        QCheckBox* hex = new QCheckBox("HEX", m_presetBody);
        QLineEdit* edit = new QLineEdit(m_presetBody);
        QSpinBox* ms = new QSpinBox(m_presetBody);
        ms->setRange(0, 60000);
        ms->setValue(200);
        ms->setSuffix(" ms");
        QPushButton* one = new QPushButton(QString::fromUtf8("发送"), m_presetBody);
        m_presetGrid->addWidget(name, i, 0);
        m_presetGrid->addWidget(hex, i, 1);
        m_presetGrid->addWidget(edit, i, 2);
        m_presetGrid->addWidget(ms, i, 3);
        m_presetGrid->addWidget(one, i, 4);
        connect(one, &QPushButton::clicked, this, [this, i]() { sendPreset(i); });
        m_presetName.append(name);
        m_presetHex.append(hex);
        m_presetEdit.append(edit);
        m_presetMs.append(ms);
        m_presetSend.append(one);
    }
    if (m_cycleIndex >= m_presetName.size()) {
        m_cycleIndex = 0;
    }
}

void MainWindow::sendPreset(int index)
{
    if (index < 0 || index >= m_presetEdit.size()) {
        return;
    }
    QByteArray bytes;
    QString err;
    if (!buildPayload(m_presetEdit[index]->text(), m_presetHex[index]->isChecked(), &bytes, &err)) {
        writeLog(err);
        return;
    }
    const QString shown = m_presetHex[index]->isChecked() ? ByteText::toHex(bytes) : m_presetEdit[index]->text();
    dispatchBytes(bytes, shown);
}

void MainWindow::cyclePresets()
{
    const int total = m_presetEdit.size();
    if (total <= 0) {
        m_cycle->setChecked(false);
        return;
    }
    for (int n = 0; n < total; ++n) {
        const int i = (m_cycleIndex + n) % total;
        if (m_presetEdit[i]->text().trimmed().isEmpty()) {
            continue;
        }
        m_cycleIndex = (i + 1) % total;
        sendPreset(i);
        m_cycleTimer->start(m_presetMs[i]->value() < 20 ? 20 : m_presetMs[i]->value());
        return;
    }
    m_cycle->setChecked(false);
}

void MainWindow::sendFileChunk()
{
    if (!m_fileBusy) {
        return;
    }
    if (m_filePos >= m_fileBytes.size()) {
        m_fileBusy = false;
        m_fileSend->setText(QString::fromUtf8("发送文件"));
        writeLog(QString::fromUtf8("文件发送完成"));
        return;
    }
    const QByteArray chunk = m_fileBytes.mid(m_filePos, 256);
    m_filePos += chunk.size();
    dispatchBytes(chunk, ByteText::toHex(chunk));
    QTimer::singleShot(m_fileGap->currentData().toInt(), this, [this]() { sendFileChunk(); });
}

void MainWindow::onHttp()
{
    QString headers;
    QString contentType = "application/octet-stream";
    for (int i = 0; i < m_headers->rowCount(); ++i) {
        QTableWidgetItem* key = m_headers->item(i, 0);
        QTableWidgetItem* val = m_headers->item(i, 1);
        if (!key || key->text().trimmed().isEmpty()) {
            continue;
        }
        if (key->text().trimmed().compare("Content-Type", Qt::CaseInsensitive) == 0) {
            contentType = val ? val->text().trimmed() : contentType;
            continue;
        }
        headers += key->text().trimmed() + ": " + (val ? val->text() : QString()) + "\r\n";
    }
    const QString method = m_method->currentText();
    m_session->requestHttp(method, QString(), contentType, m_body->toPlainText(), headers);
    const QString item = method + " " + currentPage()->summary();
    m_httpHistory->insertItem(0, item);
    while (m_httpHistory->count() > 16) {
        m_httpHistory->removeItem(m_httpHistory->count() - 1);
    }
}

void MainWindow::readOnce()
{
    const int fn = m_fn->currentData().toInt();
    if (fn <= 2) {
        m_session->requestModbus(m_slave->value(), fn, m_address->value(), m_count->value(), std::vector<uint16_t>(), QString());
    } else {
        m_session->requestModbus(m_slave->value(), fn, m_address->value(), m_count->value(), std::vector<uint16_t>(), QString());
    }
}

void MainWindow::pollModbus()
{
    if (m_pollTimer->isActive()) {
        m_pollTimer->stop();
        m_poll->setText(QString::fromUtf8("轮询"));
        return;
    }
    if (m_period->value() <= 0) {
        readOnce();
        return;
    }
    m_poll->setText(QString::fromUtf8("停止"));
    m_pollTimer->start(m_period->value());
    readOnce();
}

void MainWindow::fillModbusTable(int fn, int address, const QString& values)
{
    const QStringList parts = values.split(' ', Qt::SkipEmptyParts);
    const bool bits = fn <= 2 || fn == 5 || fn == 15;
    const bool sign = m_display->currentIndex() == 1;
    const bool hex = m_display->currentIndex() == 2;
    if (m_regs->rowCount() != parts.size()) {
        m_regs->setRowCount(parts.size());
    }
    QSettings desk("Hoson", "Vision.PortDebug");
    for (int i = 0; i < parts.size(); ++i) {
        const int addr = address + i;
        QTableWidgetItem* addrItem = new QTableWidgetItem(QString::number(addr));
        addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsEditable);
        const QString name = desk.value(QString("modbus/%1").arg(addr)).toString();
        QTableWidgetItem* nameItem = new QTableWidgetItem(name);
        QString shown = parts[i];
        if (!bits && hex) {
            shown = QString("%1").arg(parts[i].toUInt(), 4, 16, QChar('0')).toUpper();
        } else if (!bits && sign) {
            shown = QString::number((int)(qint16)parts[i].toUInt());
        }
        QTableWidgetItem* value = new QTableWidgetItem(shown);
        value->setData(Qt::UserRole, parts[i].toUInt());
        const bool writable = fn == 1 || fn == 3;
        if (!writable) {
            value->setFlags(value->flags() & ~Qt::ItemIsEditable);
        }
        m_regs->setItem(i, 0, addrItem);
        m_regs->setItem(i, 1, nameItem);
        m_regs->setItem(i, 2, value);
    }
}

void MainWindow::writeTable(bool oneRow, int row)
{
    const int fn = m_fn->currentData().toInt();
    if (fn == 2 || fn == 4) {
        return;
    }
    if (oneRow) {
        if (row < 0 || !m_regs->item(row, 2)) {
            return;
        }
        bool ok = false;
        const int current = m_regs->item(row, 2)->data(Qt::UserRole).toInt();
        const int value = QInputDialog::getInt(this, QString::fromUtf8("写入"), QString::fromUtf8("值"), current, 0, 65535, 1, &ok);
        if (!ok) {
            return;
        }
        const int addr = m_regs->item(row, 0)->text().toInt();
        if (fn <= 2) {
            std::vector<unsigned char> bits(1, value ? 1 : 0);
            m_session->requestModbusBits(m_slave->value(), 5, addr, bits, QString());
        } else {
            std::vector<uint16_t> regs(1, (uint16_t)value);
            m_session->requestModbus(m_slave->value(), 6, addr, 1, regs, QString());
        }
        return;
    }
    if (fn <= 2) {
        std::vector<unsigned char> bits;
        for (int i = 0; i < m_regs->rowCount(); ++i) {
            bits.push_back(m_regs->item(i, 2) && m_regs->item(i, 2)->text().toInt() ? 1 : 0);
        }
        m_session->requestModbusBits(m_slave->value(), bits.size() == 1 ? 5 : 15, m_address->value(), bits, QString());
    } else {
        std::vector<uint16_t> regs;
        for (int i = 0; i < m_regs->rowCount(); ++i) {
            regs.push_back((uint16_t)(m_regs->item(i, 2) ? m_regs->item(i, 2)->data(Qt::UserRole).toUInt() : 0));
        }
        m_session->requestModbus(m_slave->value(), regs.size() == 1 ? 6 : 16, m_address->value(), (int)regs.size(), regs, QString());
    }
}

void MainWindow::refreshPeers()
{
    const QStringList list = m_session->peers();
    QStringList ids;
    for (int i = 0; i < list.size(); ++i) {
        const int bar = list[i].lastIndexOf('|');
        ids << (bar >= 0 ? list[i].mid(bar + 1) : list[i]);
    }
    for (int row = m_peers->rowCount() - 1; row >= 0; --row) {
        const QString id = m_peers->item(row, 0)->data(Qt::UserRole).toString();
        if (!ids.contains(id)) {
            m_peers->removeRow(row);
        }
    }
    for (int i = 0; i < list.size(); ++i) {
        const int bar = list[i].lastIndexOf('|');
        const QString remote = bar >= 0 ? list[i].left(bar) : list[i];
        const QString id = bar >= 0 ? list[i].mid(bar + 1) : list[i];
        int row = -1;
        for (int r = 0; r < m_peers->rowCount(); ++r) {
            if (m_peers->item(r, 0)->data(Qt::UserRole).toString() == id) {
                row = r;
                break;
            }
        }
        if (row < 0) {
            row = m_peers->rowCount();
            m_peers->insertRow(row);
            QTableWidgetItem* addr = new QTableWidgetItem(remote);
            addr->setData(Qt::UserRole, id);
            addr->setData(Qt::UserRole + 1, QDateTime::currentDateTime().toString("HH:mm:ss"));
            m_peers->setItem(row, 0, addr);
            m_peers->setItem(row, 1, new QTableWidgetItem(addr->data(Qt::UserRole + 1).toString()));
            m_peers->setItem(row, 2, new QTableWidgetItem(QString()));
        } else {
            m_peers->item(row, 0)->setText(remote);
        }
    }
    const QString keep = m_target->currentData().toString();
    m_target->blockSignals(true);
    m_target->clear();
    m_target->addItem(QString::fromUtf8("全部"), "*");
    for (int r = 0; r < m_peers->rowCount(); ++r) {
        m_target->addItem(m_peers->item(r, 0)->text(), m_peers->item(r, 0)->data(Qt::UserRole).toString());
    }
    const int at = m_target->findData(keep);
    m_target->setCurrentIndex(at >= 0 ? at : 0);
    m_target->blockSignals(false);
}

void MainWindow::refreshCounts()
{
    quint64 rx = 0, tx = 0, rf = 0, tf = 0, err = 0;
    m_session->stats(&rx, &tx, &rf, &tf, &err);
    ChannelPage* page = currentPage();
    QString where = page ? page->summary() : QString();
    if (page && page->dtr()) {
        where += QString::fromUtf8("  DTR有效");
    } else if (page && page->serialLines()) {
        where += QString::fromUtf8("  DTR无效");
    }
    if (page && page->serialLines() && page->rts()) {
        where += QString::fromUtf8("  RTS有效");
    }
    if (m_wasOpen && m_openedAt > 0 && page && !page->server() && !page->udp() && !page->http() && !page->modbus()) {
        const qint64 sec = (QDateTime::currentMSecsSinceEpoch() - m_openedAt) / 1000;
        where += QString::fromUtf8("  时长 %1:%2")
            .arg(sec / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0'));
    }
    if (page && page->server()) {
        where += QString::fromUtf8("  客户端 %1").arg(m_peers->rowCount());
    }
    if (page && page->http()) {
        where = m_httpCode == 0 ? QString::fromUtf8("HTTP  尚未发送")
            : QString::fromUtf8("HTTP  上次 %1  %2 ms").arg(m_httpCode).arg(m_httpMs);
    }
    if (page && page->modbus()) {
        where += QString::fromUtf8("  Tx %1  Err %2  ID %3  F %4  周期 %5 ms")
            .arg(tf).arg(err).arg(m_slave->value()).arg(m_fn->currentData().toInt()).arg(m_period->value());
    }
    m_where->setText(where);
    m_counts->setText(QString::fromUtf8("收 %1 字节 / %2 帧    发 %3 字节 / %4 帧    错误 %5")
        .arg(rx).arg(rf).arg(tx).arg(tf).arg(err));
}

void MainWindow::drain()
{
    ChannelPage* page = currentPage();
    if (m_waitOpen) {
        if (m_session->opened()) {
            m_waitOpen = false;
            m_wasOpen = true;
            m_openedAt = QDateTime::currentMSecsSinceEpoch();
            m_chip->setText(page && (page->server() || page->udp()) ? QString::fromUtf8("收听中") : QString::fromUtf8("已连接"));
            m_chip->setObjectName("chipOn");
            polish(m_chip);
            writeLog(m_chip->text());
        } else if (!m_session->lastError().isEmpty()) {
            m_waitOpen = false;
            writeLog(m_session->lastError());
            onClose();
        }
    }
    if (m_wasOpen && page && page->serialLines()) {
        if (page->dtr() != m_lastDtr || page->rts() != m_lastRts) {
            m_lastDtr = page->dtr();
            m_lastRts = page->rts();
            m_session->requestLines(m_lastDtr, m_lastRts);
        }
    }
    if (m_wasOpen && page && page->udp()) {
        const QString now = page->udpHost() + ":" + QString::number(page->udpPort());
        if (now != m_lastUdp) {
            m_lastUdp = now;
            m_session->requestUdp(page->udpHost(), page->udpPort());
        }
    }
    if (page && page->server()) {
        refreshPeers();
    }
    CommLine line;
    while (m_session->takeLine(&line)) {
        if (line.op) {
            m_log->appendPlainText(line.text);
            continue;
        }
        if (line.fn != 0 && !line.values.isEmpty()) {
            fillModbusTable(line.fn, line.address, line.values);
        }
        if (!m_pause->isChecked()) {
            appendMessage(line.raw.isEmpty() ? line.text : line.raw);
        }
        if (m_dump && !line.bytes.isEmpty()) {
            m_dump->write(line.bytes);
        }
        if (!line.peer.isEmpty()) {
            for (int r = 0; r < m_peers->rowCount(); ++r) {
                if (m_peers->item(r, 0)->text() == line.peer && m_peers->item(r, 2)) {
                    m_peers->item(r, 2)->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
                    m_peers->item(r, 2)->setData(Qt::UserRole, QDateTime::currentMSecsSinceEpoch());
                }
            }
        }
        if (line.text.startsWith(QString::fromUtf8("HTTP")) == false && page && page->http() && line.text.contains(" ms")) {
            const QStringList parts = line.text.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                m_httpCode = parts[0].toInt();
                m_httpMs = parts[1].toInt();
            }
        }
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int r = 0; r < m_peers->rowCount(); ++r) {
        QTableWidgetItem* last = m_peers->item(r, 2);
        const bool stale = last && last->data(Qt::UserRole).toLongLong() > 0
            && now - last->data(Qt::UserRole).toLongLong() > 30000;
        const QColor color = stale ? QColor("#BFBFBF") : QColor("#1F1F1F");
        for (int c = 0; c < 3; ++c) {
            if (m_peers->item(r, c)) {
                m_peers->item(r, c)->setForeground(color);
            }
        }
    }
    refreshCounts();
}

void MainWindow::saveText(QPlainTextEdit* edit, const QString& title)
{
    const QString path = QFileDialog::getSaveFileName(this, title);
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        writeLog(QString::fromUtf8("保存失败"));
        return;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << edit->toPlainText();
}

void MainWindow::loadDesk()
{
    QSettings desk("Hoson", "Vision.PortDebug");
    const int n = desk.beginReadArray("history");
    for (int i = 0; i < n; ++i) {
        desk.setArrayIndex(i);
        m_history->addItem(desk.value("text").toString());
    }
    desk.endArray();
    int count = desk.value("presetCount", 16).toInt();
    if (count < 1) {
        count = 1;
    }
    if (count > 64) {
        count = 64;
    }
    m_presetCount->setValue(count);
    const int p = desk.beginReadArray("presets");
    for (int i = 0; i < p && i < m_presetEdit.size(); ++i) {
        desk.setArrayIndex(i);
        m_presetName[i]->setText(desk.value("name").toString());
        m_presetHex[i]->setChecked(desk.value("hex").toBool());
        m_presetEdit[i]->setText(desk.value("text").toString());
        m_presetMs[i]->setValue(desk.value("ms", 200).toInt());
    }
    desk.endArray();
}

void MainWindow::saveDesk()
{
    QSettings desk("Hoson", "Vision.PortDebug");
    desk.beginWriteArray("history");
    for (int i = 0; i < m_history->count(); ++i) {
        desk.setArrayIndex(i);
        desk.setValue("text", m_history->itemText(i));
    }
    desk.endArray();
    desk.setValue("presetCount", m_presetName.size());
    desk.beginWriteArray("presets");
    for (int i = 0; i < m_presetName.size(); ++i) {
        desk.setArrayIndex(i);
        desk.setValue("name", m_presetName[i]->text());
        desk.setValue("hex", m_presetHex[i]->isChecked());
        desk.setValue("text", m_presetEdit[i]->text());
        desk.setValue("ms", m_presetMs[i]->value());
    }
    desk.endArray();
    for (int r = 0; r < m_regs->rowCount(); ++r) {
        if (m_regs->item(r, 0) && m_regs->item(r, 1)) {
            desk.setValue(QString("modbus/%1").arg(m_regs->item(r, 0)->text()), m_regs->item(r, 1)->text());
        }
    }
}
