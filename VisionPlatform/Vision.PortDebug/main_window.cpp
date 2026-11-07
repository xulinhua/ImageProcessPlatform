/**
 * @file main_window.cpp
 * @brief 主窗口布局。样式只属于本调试器
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "main_window.h"
#include "byte_text.h"
#include "channel_page.h"
#include "channel_session.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextCodec>
#include <QTextStream>
#include <QToolButton>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_cycleIndex(0)
    , m_waitOpen(false)
{
    setWindowTitle(QString::fromUtf8("通讯调试器"));

    m_tabs = new QTabBar(this);
    const char* titles[] = { "串口", "TCP客户端", "TCP服务端", "UDP", "HTTP", "WebSocket", "Modbus RTU", "Modbus TCP" };
    for (int i = 0; i < 8; ++i) {
        m_tabs->addTab(QString::fromUtf8(titles[i]));
    }
    m_pages = new QStackedWidget(this);
    m_pages->addWidget(createSerialPage(this));
    m_pages->addWidget(createTcpClientPage(this));
    m_pages->addWidget(createTcpServerPage(this));
    m_pages->addWidget(createUdpPage(this));
    m_pages->addWidget(createHttpPage(this));
    m_pages->addWidget(createWebSocketPage(this));
    m_pages->addWidget(createModbusRtuPage(this));
    m_pages->addWidget(createModbusTcpPage(this));

    m_open = new QPushButton(QString::fromUtf8("打开"), this);
    m_open->setObjectName("primary");
    m_close = new QPushButton(QString::fromUtf8("关闭"), this);
    m_close->setEnabled(false);
    m_status = new QLabel(QString::fromUtf8("未连接"), this);
    m_status->setObjectName("statusOff");

    QHBoxLayout* tool = new QHBoxLayout();
    tool->addWidget(m_tabs, 1);
    tool->addWidget(m_open);
    tool->addWidget(m_close);
    tool->addWidget(m_status);

    m_recv = new QPlainTextEdit(this);
    m_recv->setObjectName("log");
    m_recv->setReadOnly(true);
    m_hexView = new QCheckBox(QString::fromUtf8("HEX显示"), this);
    m_hexView->setChecked(true);
    m_stamp = new QCheckBox(QString::fromUtf8("时间戳"), this);
    m_stamp->setChecked(true);
    m_pause = new QCheckBox(QString::fromUtf8("暂停显示"), this);
    QPushButton* clear = new QPushButton(QString::fromUtf8("清空"), this);
    QPushButton* saveLog = new QPushButton(QString::fromUtf8("保存记录"), this);

    QFrame* recvBox = new QFrame(this);
    recvBox->setObjectName("panel");
    QVBoxLayout* recvLay = new QVBoxLayout(recvBox);
    recvLay->addWidget(new QLabel(QString::fromUtf8("接收"), recvBox));
    recvLay->addWidget(m_recv, 1);
    QHBoxLayout* recvOpts = new QHBoxLayout();
    recvOpts->addWidget(m_hexView);
    recvOpts->addWidget(m_stamp);
    recvOpts->addWidget(m_pause);
    recvOpts->addStretch(1);
    recvOpts->addWidget(saveLog);
    recvOpts->addWidget(clear);
    recvLay->addLayout(recvOpts);
    m_rawToggle = new QToolButton(recvBox);
    m_rawToggle->setText(QString::fromUtf8("原始字节"));
    m_rawToggle->setCheckable(true);
    m_rawToggle->setChecked(true);
    m_raw = new QPlainTextEdit(recvBox);
    m_raw->setObjectName("log");
    m_raw->setReadOnly(true);
    m_raw->setFixedHeight(72);
    m_raw->setPlaceholderText(QString::fromUtf8("Modbus 发出和收到的整帧"));
    recvLay->addWidget(m_rawToggle);
    recvLay->addWidget(m_raw);

    m_send = new QPlainTextEdit(this);
    m_send->setObjectName("log");
    m_send->setFixedHeight(72);
    m_hexSend = new QCheckBox(QString::fromUtf8("HEX发送"), this);
    m_hexSend->setChecked(true);
    m_crlf = new QCheckBox(QString::fromUtf8("加回车换行"), this);
    m_escape = new QCheckBox(QString::fromUtf8("转义"), this);
    m_escape->setToolTip(QString::fromUtf8("\\\\r \\\\n \\\\t \\\\0 \\\\xHH \\\\\\\\"));
    m_check = new QComboBox(this);
    m_check->addItem(QString::fromUtf8("不加校验"), 0);
    m_check->addItem(QString::fromUtf8("累加和"), 1);
    m_check->addItem(QString::fromUtf8("异或"), 2);
    m_check->addItem(QString::fromUtf8("CRC16"), 3);
    m_codec = new QComboBox(this);
    m_codec->addItem("UTF-8");
    m_codec->addItem("GBK");
    m_history = new QComboBox(this);
    m_history->setMinimumWidth(160);
    m_history->addItem(QString::fromUtf8("历史发送"));
    m_auto = new QCheckBox(QString::fromUtf8("自动发送"), this);
    m_interval = new QSpinBox(this);
    m_interval->setRange(20, 60000);
    m_interval->setValue(100);
    m_interval->setSuffix(" ms");
    QPushButton* send = new QPushButton(QString::fromUtf8("发送"), this);
    send->setObjectName("primary");
    m_disconnect = new QPushButton(QString::fromUtf8("断开"), this);

    m_modbusBar = new QWidget(this);
    m_slave = new QSpinBox(m_modbusBar);
    m_slave->setRange(1, 247);
    m_fn = new QComboBox(m_modbusBar);
    m_fn->addItem(QString::fromUtf8("读保持寄存器"), 3);
    m_fn->addItem(QString::fromUtf8("读输入寄存器"), 4);
    m_fn->addItem(QString::fromUtf8("写单寄存器"), 6);
    m_fn->addItem(QString::fromUtf8("写多寄存器"), 16);
    m_address = new QSpinBox(m_modbusBar);
    m_address->setRange(0, 65535);
    m_count = new QSpinBox(m_modbusBar);
    m_count->setRange(1, 125);
    QHBoxLayout* modbusLay = new QHBoxLayout(m_modbusBar);
    modbusLay->setContentsMargins(0, 0, 0, 0);
    modbusLay->addWidget(new QLabel(QString::fromUtf8("从站"), m_modbusBar));
    modbusLay->addWidget(m_slave);
    modbusLay->addWidget(m_fn);
    modbusLay->addWidget(new QLabel(QString::fromUtf8("地址"), m_modbusBar));
    modbusLay->addWidget(m_address);
    modbusLay->addWidget(new QLabel(QString::fromUtf8("数量"), m_modbusBar));
    modbusLay->addWidget(m_count);
    modbusLay->addStretch(1);

    m_httpBar = new QWidget(this);
    m_method = new QComboBox(m_httpBar);
    m_method->addItems(QStringList() << "GET" << "POST" << "PUT");
    m_headers = new QPlainTextEdit(m_httpBar);
    m_headers->setObjectName("log");
    m_headers->setFixedHeight(56);
    m_headers->setPlaceholderText(QString::fromUtf8("Content-Type: text/plain\nAuthorization: Bearer token"));
    QVBoxLayout* httpLay = new QVBoxLayout(m_httpBar);
    httpLay->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout* methodRow = new QHBoxLayout();
    methodRow->addWidget(new QLabel(QString::fromUtf8("方法"), m_httpBar));
    methodRow->addWidget(m_method);
    methodRow->addStretch(1);
    httpLay->addLayout(methodRow);
    httpLay->addWidget(new QLabel(QString::fromUtf8("请求头"), m_httpBar));
    httpLay->addWidget(m_headers);

    QFrame* sendBox = new QFrame(this);
    sendBox->setObjectName("panel");
    QVBoxLayout* sendLay = new QVBoxLayout(sendBox);
    sendLay->addWidget(new QLabel(QString::fromUtf8("发送"), sendBox));
    sendLay->addWidget(m_modbusBar);
    sendLay->addWidget(m_httpBar);
    sendLay->addWidget(m_send);
    QHBoxLayout* sendOpts = new QHBoxLayout();
    sendOpts->addWidget(m_hexSend);
    sendOpts->addWidget(m_crlf);
    sendOpts->addWidget(m_escape);
    sendOpts->addWidget(m_codec);
    sendOpts->addWidget(m_check);
    sendOpts->addWidget(m_history);
    sendOpts->addWidget(m_auto);
    sendOpts->addWidget(m_interval);
    sendOpts->addStretch(1);
    sendOpts->addWidget(m_disconnect);
    sendOpts->addWidget(send);
    sendLay->addLayout(sendOpts);

    QFrame* presetBox = new QFrame(this);
    presetBox->setObjectName("panel");
    QVBoxLayout* presetLay = new QVBoxLayout(presetBox);
    presetLay->addWidget(new QLabel(QString::fromUtf8("多条发送"), presetBox));
    for (int i = 0; i < kPresets; ++i) {
        m_presetHex[i] = new QCheckBox(QString::fromUtf8("HEX"), presetBox);
        m_presetEdit[i] = new QLineEdit(presetBox);
        QPushButton* one = new QPushButton(QString::fromUtf8("发送"), presetBox);
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(m_presetHex[i]);
        row->addWidget(m_presetEdit[i], 1);
        row->addWidget(one);
        presetLay->addLayout(row);
        connect(one, &QPushButton::clicked, this, [this, i]() { sendPreset(i); });
    }
    m_cycle = new QCheckBox(QString::fromUtf8("循环多条"), presetBox);
    m_cycleMs = new QSpinBox(presetBox);
    m_cycleMs->setRange(20, 60000);
    m_cycleMs->setValue(500);
    m_cycleMs->setSuffix(" ms");
    QHBoxLayout* cycleRow = new QHBoxLayout();
    cycleRow->addWidget(m_cycle);
    cycleRow->addWidget(m_cycleMs);
    cycleRow->addStretch(1);
    presetLay->addLayout(cycleRow);

    m_peers = new QListWidget(this);
    QFrame* peerBox = new QFrame(this);
    peerBox->setObjectName("panel");
    peerBox->setMinimumWidth(220);
    QVBoxLayout* peerLay = new QVBoxLayout(peerBox);
    peerLay->addWidget(new QLabel(QString::fromUtf8("对端"), peerBox));
    peerLay->addWidget(m_peers, 1);

    QWidget* center = new QWidget(this);
    QVBoxLayout* centerLay = new QVBoxLayout(center);
    centerLay->setContentsMargins(0, 0, 0, 0);
    centerLay->addLayout(tool);
    QFrame* pageBox = new QFrame(center);
    pageBox->setObjectName("panel");
    QVBoxLayout* pageLay = new QVBoxLayout(pageBox);
    pageLay->addWidget(m_pages);
    centerLay->addWidget(pageBox);
    centerLay->addWidget(recvBox, 1);
    centerLay->addWidget(sendBox);
    centerLay->addWidget(presetBox);

    QSplitter* split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(center);
    split->addWidget(peerBox);
    split->setStretchFactor(0, 1);
    setCentralWidget(split);

    m_session = new ChannelSession();
    m_session->start();
    m_timer = new QTimer(this);
    m_autoTimer = new QTimer(this);
    m_cycleTimer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() { drain(); });
    m_timer->start(40);
    connect(m_autoTimer, &QTimer::timeout, this, [this]() { onSend(); });
    connect(m_cycleTimer, &QTimer::timeout, this, [this]() { cyclePresets(); });
    connect(m_tabs, &QTabBar::currentChanged, this, [this](int i) { showPage(i); });
    connect(m_open, &QPushButton::clicked, this, [this]() { onOpen(); });
    connect(m_close, &QPushButton::clicked, this, [this]() { onClose(); });
    connect(m_disconnect, &QPushButton::clicked, this, [this]() { onClose(); });
    connect(m_rawToggle, &QToolButton::toggled, m_raw, &QWidget::setVisible);
    connect(send, &QPushButton::clicked, this, [this]() { onSend(); });
    connect(clear, &QPushButton::clicked, this, [this]() {
        m_recv->clear();
        m_raw->clear();
    });
    connect(saveLog, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("保存记录"), QString(), QString::fromUtf8("文本 (*.txt)"));
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            statusBar()->showMessage(QString::fromUtf8("保存失败"));
            return;
        }
        QTextStream out(&file);
        out.setCodec("UTF-8");
        out << m_recv->toPlainText();
    });
    QShortcut* sendKey = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return), this);
    connect(sendKey, &QShortcut::activated, this, [this]() { onSend(); });
    QShortcut* sendKey2 = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Enter), this);
    connect(sendKey2, &QShortcut::activated, this, [this]() { onSend(); });
    connect(m_hexView, &QCheckBox::toggled, this, [this](bool) {
        m_session->setView(m_hexView->isChecked(), m_stamp->isChecked());
    });
    connect(m_stamp, &QCheckBox::toggled, this, [this](bool) {
        m_session->setView(m_hexView->isChecked(), m_stamp->isChecked());
    });
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
    connect(m_peers, &QListWidget::currentRowChanged, this, [this](int) {
        m_session->setPeer(selectedPeer());
    });
    connect(m_hexSend, &QCheckBox::toggled, this, [this](bool) { showPage(m_tabs->currentIndex()); });
    connect(m_history, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index > 0) {
            m_send->setPlainText(m_history->itemText(index));
        }
    });
    connect(m_cycle, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            m_cycleIndex = 0;
            m_cycleTimer->start(m_cycleMs->value());
        } else {
            m_cycleTimer->stop();
        }
    });
    connect(m_cycleMs, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_cycle->isChecked()) {
            m_cycleTimer->start(v);
        }
    });
    for (int i = 0; i < kPresets; ++i) {
        connect(m_presetEdit[i], &QLineEdit::editingFinished, this, [this]() { saveDesk(); });
        connect(m_presetHex[i], &QCheckBox::toggled, this, [this](bool) { saveDesk(); });
    }

    m_counts = new QLabel(this);
    QPushButton* zero = new QPushButton(QString::fromUtf8("计数清零"), this);
    statusBar()->addPermanentWidget(m_counts);
    statusBar()->addPermanentWidget(zero);
    connect(zero, &QPushButton::clicked, this, [this]() {
        m_session->resetStats();
        refreshCounts();
    });
    loadDesk();
    refreshCounts();
    showPage(0);
    statusBar()->showMessage(QString::fromUtf8("Ctrl+Enter 发送。一次只打开一条通道"));
    resize(1100, 860);
}

MainWindow::~MainWindow()
{
    saveDesk();
    m_autoTimer->stop();
    m_cycleTimer->stop();
    m_session->requestClose();
    delete m_session;
}

ChannelPage* MainWindow::currentPage() const
{
    return static_cast<ChannelPage*>(m_pages->currentWidget());
}

QString MainWindow::selectedPeer() const
{
    QListWidgetItem* item = m_peers->currentItem();
    if (!item) {
        return QString();
    }
    return item->data(Qt::UserRole).toString();
}

void MainWindow::showPage(int index)
{
    m_pages->setCurrentIndex(index);
    ChannelPage* page = currentPage();
    const bool modbus = page && page->modbus();
    const bool http = page && page->http();
    const bool socket = page && page->webSocket();
    m_modbusBar->setVisible(modbus);
    m_httpBar->setVisible(http);
    m_rawToggle->setVisible(modbus);
    m_raw->setVisible(modbus && m_rawToggle->isChecked());
    m_disconnect->setVisible(socket);
    const bool bytes = !modbus && !http;
    const bool textSend = bytes && !m_hexSend->isChecked();
    m_hexSend->setVisible(bytes);
    m_crlf->setVisible(textSend);
    m_escape->setVisible(textSend);
    m_codec->setVisible(textSend);
    m_check->setVisible(bytes);
    m_history->setVisible(bytes);
    m_auto->setVisible(bytes);
    m_send->setPlaceholderText(modbus
        ? QString::fromUtf8("写寄存器时填写数值，逗号分隔，例如 1,2,3")
        : (http ? QString::fromUtf8("请求体") : QString::fromUtf8("01 03 00 00 00 0A")));
}

void MainWindow::onOpen()
{
    ChannelPage* page = currentPage();
    if (!page) {
        return;
    }
    Vision::CommChannel channel;
    channel.enabled = true;
    page->fill(&channel);
    m_waitOpen = true;
    m_session->setView(m_hexView->isChecked(), m_stamp->isChecked());
    m_session->requestOpen(channel, page->stream(), page->webSocket());
    statusBar()->showMessage(QString::fromUtf8("正在打开"));
}

void MainWindow::onClose()
{
    m_auto->setChecked(false);
    m_cycle->setChecked(false);
    m_session->requestClose();
}

void MainWindow::onSend()
{
    ChannelPage* page = currentPage();
    if (!page) {
        return;
    }
    if (page->modbus()) {
        std::vector<uint16_t> regs;
        const QStringList parts = m_send->toPlainText().split(',', Qt::SkipEmptyParts);
        for (int i = 0; i < parts.size(); ++i) {
            bool ok = false;
            const int v = parts[i].trimmed().toInt(&ok);
            if (ok && v >= 0 && v <= 65535) {
                regs.push_back((uint16_t)v);
            }
        }
        const int fn = m_fn->currentData().toInt();
        if (fn == 6 && regs.size() != 1) {
            statusBar()->showMessage(QString::fromUtf8("写单寄存器只填一个数值"));
            return;
        }
        if (fn == 16 && regs.size() < 2) {
            statusBar()->showMessage(QString::fromUtf8("写多寄存器至少两个数值，逗号分隔"));
            return;
        }
        m_session->requestModbus(m_slave->value(), fn, m_address->value(),
                                 (fn == 6 || fn == 16) ? (int)regs.size() : m_count->value(), regs, selectedPeer());
        return;
    }
    if (page->http()) {
        QString contentType = QString::fromUtf8("text/plain");
        const QStringList lines = m_headers->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (int i = 0; i < lines.size(); ++i) {
            const QString line = lines[i].trimmed();
            if (line.startsWith(QString::fromUtf8("Content-Type:"), Qt::CaseInsensitive)) {
                contentType = line.mid(line.indexOf(':') + 1).trimmed();
            }
        }
        m_session->requestHttp(m_method->currentText(), QString(), contentType, m_send->toPlainText(), m_headers->toPlainText());
        return;
    }
    QByteArray bytes;
    QString err;
    if (!buildPayload(m_send->toPlainText(), m_hexSend->isChecked(), &bytes, &err)) {
        statusBar()->showMessage(err);
        return;
    }
    if (sender() != m_autoTimer) {
        noteHistory(m_send->toPlainText());
    }
    dispatchBytes(bytes, !m_hexSend->isChecked(), m_send->toPlainText());
}

void MainWindow::drain()
{
    const bool opened = m_session->opened();
    const QString error = m_session->lastError();
    if (opened) {
        m_status->setObjectName("statusOn");
        m_status->setText(QString::fromUtf8("已连接"));
        m_open->setEnabled(false);
        m_close->setEnabled(true);
        m_tabs->setEnabled(false);
        m_pages->setEnabled(false);
        m_waitOpen = false;
    } else if (m_waitOpen && !error.isEmpty()) {
        m_status->setObjectName("statusOff");
        m_status->setText(error);
        statusBar()->showMessage(error);
        m_waitOpen = false;
    } else if (!m_waitOpen) {
        m_status->setObjectName("statusOff");
        m_status->setText(QString::fromUtf8("未连接"));
        m_open->setEnabled(true);
        m_close->setEnabled(false);
        m_tabs->setEnabled(true);
        m_pages->setEnabled(true);
    }
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);

    const QString current = selectedPeer();
    const QStringList peers = m_session->peers();
    if (m_peers->count() != peers.size()) {
        m_peers->clear();
        for (int i = 0; i < peers.size(); ++i) {
            const QString itemText = peers[i];
            const int bar = itemText.lastIndexOf('|');
            QListWidgetItem* item = new QListWidgetItem(bar < 0 ? itemText : itemText.left(bar));
            item->setData(Qt::UserRole, bar < 0 ? itemText : itemText.mid(bar + 1));
            m_peers->addItem(item);
            if (item->data(Qt::UserRole).toString() == current) {
                m_peers->setCurrentItem(item);
            }
        }
    }

    CommLine line;
    while (m_session->takeLine(&line)) {
        if (m_pause->isChecked()) {
            continue;
        }
        if (!line.text.isEmpty()) {
            m_recv->appendPlainText(line.text);
        }
        if (!line.raw.isEmpty()) {
            m_raw->appendPlainText(line.raw);
        }
    }
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
        QString plain = m_escape->isChecked() ? ByteText::unescape(text) : text;
        if (m_crlf->isChecked()) {
            plain += QString::fromUtf8("\r\n");
        }
        QTextCodec* codec = QTextCodec::codecForName(m_codec->currentText().toLatin1());
        *out = codec ? codec->fromUnicode(plain) : plain.toUtf8();
    }
    ByteText::appendCheck(out, m_check->currentData().toInt());
    return true;
}

void MainWindow::dispatchBytes(const QByteArray& bytes, bool asText, const QString& shown)
{
    if (!m_session->opened()) {
        statusBar()->showMessage(QString::fromUtf8("通道未打开"));
        return;
    }
    if (m_pages->currentIndex() == 2 && selectedPeer().isEmpty()) {
        statusBar()->showMessage(QString::fromUtf8("请先在右侧选择客户端"));
        return;
    }
    ChannelPage* page = currentPage();
    if (page && page->webSocket()) {
        m_session->requestWs(bytes, asText);
    } else {
        m_session->requestWrite(bytes, selectedPeer());
    }
    if (!m_pause->isChecked()) {
        const QString body = m_hexView->isChecked() ? ByteText::toHex(bytes) : shown;
        m_recv->appendPlainText(ByteText::stamp() + QString::fromUtf8("  发→ ") + body);
    }
}

void MainWindow::noteHistory(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    const int found = m_history->findText(trimmed);
    if (found > 0) {
        m_history->removeItem(found);
    }
    m_history->insertItem(1, trimmed);
    while (m_history->count() > 17) {
        m_history->removeItem(m_history->count() - 1);
    }
    m_history->setCurrentIndex(0);
    saveDesk();
}

void MainWindow::sendPreset(int index)
{
    if (index < 0 || index >= kPresets) {
        return;
    }
    ChannelPage* page = currentPage();
    if (!page || page->modbus() || page->http()) {
        statusBar()->showMessage(QString::fromUtf8("多条发送用于字节通道和 WebSocket"));
        return;
    }
    QByteArray bytes;
    QString err;
    if (!buildPayload(m_presetEdit[index]->text(), m_presetHex[index]->isChecked(), &bytes, &err)) {
        statusBar()->showMessage(err);
        return;
    }
    dispatchBytes(bytes, !m_presetHex[index]->isChecked(), m_presetEdit[index]->text());
}

void MainWindow::cyclePresets()
{
    for (int n = 0; n < kPresets; ++n) {
        const int index = (m_cycleIndex + n) % kPresets;
        if (m_presetEdit[index]->text().trimmed().isEmpty()) {
            continue;
        }
        m_cycleIndex = (index + 1) % kPresets;
        sendPreset(index);
        return;
    }
}

void MainWindow::loadDesk()
{
    QSettings desk(QString::fromUtf8("Hoson"), QString::fromUtf8("Vision.PortDebug"));
    const int rows = desk.beginReadArray("presets");
    for (int i = 0; i < rows && i < kPresets; ++i) {
        desk.setArrayIndex(i);
        m_presetEdit[i]->blockSignals(true);
        m_presetHex[i]->blockSignals(true);
        m_presetEdit[i]->setText(desk.value("text").toString());
        m_presetHex[i]->setChecked(desk.value("hex").toBool());
        m_presetEdit[i]->blockSignals(false);
        m_presetHex[i]->blockSignals(false);
    }
    desk.endArray();
    const QStringList history = desk.value("history").toStringList();
    for (int i = 0; i < history.size() && i < 16; ++i) {
        m_history->addItem(history[i]);
    }
    const int check = desk.value("check", 0).toInt();
    const int checkIndex = m_check->findData(check);
    if (checkIndex >= 0) {
        m_check->setCurrentIndex(checkIndex);
    }
    const QString codec = desk.value("codec", "UTF-8").toString();
    const int codecIndex = m_codec->findText(codec);
    if (codecIndex >= 0) {
        m_codec->setCurrentIndex(codecIndex);
    }
}

void MainWindow::saveDesk()
{
    QSettings desk(QString::fromUtf8("Hoson"), QString::fromUtf8("Vision.PortDebug"));
    desk.beginWriteArray("presets");
    for (int i = 0; i < kPresets; ++i) {
        desk.setArrayIndex(i);
        desk.setValue("text", m_presetEdit[i]->text());
        desk.setValue("hex", m_presetHex[i]->isChecked());
    }
    desk.endArray();
    QStringList history;
    for (int i = 1; i < m_history->count(); ++i) {
        history << m_history->itemText(i);
    }
    desk.setValue("history", history);
    desk.setValue("check", m_check->currentData().toInt());
    desk.setValue("codec", m_codec->currentText());
}

void MainWindow::refreshCounts()
{
    quint64 rxBytes = 0;
    quint64 txBytes = 0;
    quint64 rxFrames = 0;
    quint64 txFrames = 0;
    m_session->stats(&rxBytes, &txBytes, &rxFrames, &txFrames);
    m_counts->setText(QString::fromUtf8("收 %1 字节 / %2 帧    发 %3 字节 / %4 帧")
                          .arg(rxBytes)
                          .arg(rxFrames)
                          .arg(txBytes)
                          .arg(txFrames));
}
