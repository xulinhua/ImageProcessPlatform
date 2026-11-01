/**
 * @file CommMgrDlg.cpp
 * @brief CommMgrDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CommMgrDlg.h"
#include "CommMgr.h"
#include "Solution.h"
#include "QtConv.h"
#include "VpUtil.h"
#include "Session.h"
#include "Theme.h"
#include "UiText.h"
#include "Perm.h"
#include <cstdio>
#include <string>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QMessageBox>
#include <QRegExp>
#include <QVBoxLayout>

namespace Vision {

namespace {

// 界面字节转到通信缓冲
CommBytes toBytes(const QByteArray& a)
{
    return CommBytes(a.begin(), a.end());
}

// 通信缓冲转回本地编码文本
QString fromBytes(const CommBytes& b)
{
    if (b.empty()) {
        return QString();
    }
    return QString::fromUtf8(reinterpret_cast<const char*>(&b[0]), static_cast<int>(b.size()));
}

/** @brief 列表和类型标签：Modbus 按 RTU / TCP 显示，不跟传输层名字混在一起 */
QString channelKindText(CommChannelType type, bool modbus)
{
    if (modbus && type == CommSerial) {
        return T("MODBUS RTU");
    }
    if (modbus && (type == CommTcpClient || type == CommTcpServer)) {
        return T("MODBUS TCP");
    }
    return QString::fromUtf8(commChannelTypeTitle(type).c_str());
}

// 已连接绿色加粗，否则灰色
void paintConnStatus(QLabel* lab, const QString& text, bool open)
{
    lab->setText(text);
    if (open) {
        lab->setStyleSheet(QLatin1String("color:#237804; font-weight:bold;"));
    } else {
        lab->setStyleSheet(QLatin1String("color:#8C8C8C;"));
    }
}

} // namespace

// 通信管理：通道列表、参数页与收发测试
CommMgrDlg::CommMgrDlg(Session* session, Solution* solution, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_sol(solution)
{
    setWindowTitle(T("通信管理"));
    resize(1080, 680);
    enableDialogResize(this);
    // 无编辑通信权限时只能连接、收发调试，不能增删改通道
    const bool can = session->has(PermEditComm);

    m_list = new QListWidget(this);
    m_name = new QLineEdit(this);
    m_type = new QLabel(this);
    m_type->setAlignment(Qt::AlignCenter);
    m_type->setMinimumWidth(108);
    m_type->setStyleSheet(QLatin1String(
        "background:#E6F7FF; color:#1890FF; border:1px solid #91D5FF;"
        " border-radius:4px; padding:4px 8px; font-weight:bold;"));

    m_protocol = new QComboBox(this);
    m_protocol->addItem(T("原始字节"), QLatin1String("raw"));
    m_protocol->addItem(T("MODBUS 主站"), QLatin1String("master"));
    m_protocol->addItem(T("MODBUS 从站"), QLatin1String("slave"));
    m_protocol->setEnabled(can);
    m_protocolLabel = new QLabel(T("协议"), this);

    // 各通道类型的参数控件
    m_serialPort = new QComboBox(this);
    m_baud = new QSpinBox(this);
    m_baud->setRange(1200, 921600);
    m_baud->setValue(9600);
    m_dataBits = new QSpinBox(this);
    m_dataBits->setRange(7, 8);
    m_dataBits->setValue(8);
    m_parity = new QComboBox(this);
    m_parity->addItem(T("无"), QLatin1String("none"));
    m_parity->addItem(T("偶"), QLatin1String("even"));
    m_parity->addItem(T("奇"), QLatin1String("odd"));
    m_stopBits = new QSpinBox(this);
    m_stopBits->setRange(1, 2);
    m_stopBits->setValue(1);

    m_host = new QLineEdit(this);
    m_tcpPort = new QSpinBox(this);
    m_tcpPort->setRange(1, 65535);
    m_tcpPort->setValue(9000);
    m_listenPort = new QSpinBox(this);
    m_listenPort->setRange(1, 65535);
    m_listenPort->setValue(9000);
    m_peers = new QComboBox(this);

    m_udpBind = new QSpinBox(this);
    m_udpBind->setRange(1, 65535);
    m_udpBind->setValue(9000);
    m_udpHost = new QLineEdit(this);
    m_udpRemote = new QSpinBox(this);
    m_udpRemote->setRange(1, 65535);
    m_udpRemote->setValue(9000);

    m_mbArea = new QComboBox(this);
    m_mbArea->addItem(T("线圈"), ModbusCoil);
    m_mbArea->addItem(T("离散输入"), ModbusDiscrete);
    m_mbArea->addItem(T("保持寄存器"), ModbusHolding);
    m_mbArea->addItem(T("输入寄存器"), ModbusInput);
    m_mbSlave = new QSpinBox(this);
    m_mbSlave->setRange(0, 255);
    m_mbSlave->setValue(1);
    m_mbAddr = new QSpinBox(this);
    m_mbAddr->setRange(0, 65535);
    m_mbCount = new QSpinBox(this);
    m_mbCount->setRange(1, 125);
    m_mbCount->setValue(1);
    m_mbValue = new QSpinBox(this);
    m_mbValue->setRange(0, 65535);

    // 参数页顺序：串口、TCP 客户端、TCP 服务端、UDP
    m_stack = new QStackedWidget(this);
    QWidget* serialPage = new QWidget(m_stack);
    QFormLayout* sf = new QFormLayout(serialPage);
    QPushButton* refreshPorts = new QPushButton(T("刷新串口"), serialPage);
    connect(refreshPorts, SIGNAL(clicked()), this, SLOT(onRefreshPorts()));
    sf->addRow(T("端口"), m_serialPort);
    sf->addRow(T(""), refreshPorts);
    sf->addRow(T("波特率"), m_baud);
    sf->addRow(T("数据位"), m_dataBits);
    sf->addRow(T("校验"), m_parity);
    sf->addRow(T("停止位"), m_stopBits);

    QWidget* clientPage = new QWidget(m_stack);
    QFormLayout* cf = new QFormLayout(clientPage);
    cf->addRow(T("主机"), m_host);
    cf->addRow(T("端口"), m_tcpPort);

    QWidget* serverPage = new QWidget(m_stack);
    QFormLayout* vf = new QFormLayout(serverPage);
    QPushButton* refreshPeers = new QPushButton(T("刷新客户端"), serverPage);
    connect(refreshPeers, SIGNAL(clicked()), this, SLOT(onRefreshPeers()));
    vf->addRow(T("监听端口"), m_listenPort);
    vf->addRow(T("客户端"), m_peers);
    vf->addRow(T(""), refreshPeers);

    QWidget* udpPage = new QWidget(m_stack);
    QFormLayout* uf = new QFormLayout(udpPage);
    uf->addRow(T("本机端口"), m_udpBind);
    uf->addRow(T("对端主机"), m_udpHost);
    uf->addRow(T("对端端口"), m_udpRemote);

    m_stack->addWidget(serialPage);
    m_stack->addWidget(clientPage);
    m_stack->addWidget(serverPage);
    m_stack->addWidget(udpPage);

    m_status = new QLabel(T("未连接"), this);
    m_io = new QPlainTextEdit(this);
    m_io->setReadOnly(true);
    m_io->setPlaceholderText(T("收发记录"));
    m_io->setStyleSheet(QLatin1String("font-family: Consolas, monospace;"));
    m_sendEdit = new QLineEdit(this);
    m_sendEdit->setPlaceholderText(T("要发送的文本"));

    // 按钮与信号
    QPushButton* addSerial = new QPushButton(T("串口"), this);
    QPushButton* addClient = new QPushButton(T("TCP客户端"), this);
    QPushButton* addServer = new QPushButton(T("TCP服务端"), this);
    QPushButton* addUdp = new QPushButton(T("UDP"), this);
    QPushButton* addMbRtu = new QPushButton(T("MODBUS RTU"), this);
    QPushButton* addMbTcp = new QPushButton(T("MODBUS TCP"), this);
    QPushButton* delBtn = new QPushButton(T("删除"), this);
    QPushButton* applyBtn = new QPushButton(T("应用参数"), this);
    QPushButton* connBtn = new QPushButton(T("连接"), this);
    QPushButton* discBtn = new QPushButton(T("断开"), this);
    QPushButton* sendBtn = new QPushButton(T("发送"), this);
    QPushButton* mbRead = new QPushButton(T("MODBUS 读"), this);
    QPushButton* mbWrite = new QPushButton(T("MODBUS 写"), this);
    setButtonRole(connBtn, "primary");

    addSerial->setEnabled(can);
    addClient->setEnabled(can);
    addServer->setEnabled(can);
    addUdp->setEnabled(can);
    addMbRtu->setEnabled(can);
    addMbTcp->setEnabled(can);
    delBtn->setEnabled(can);
    applyBtn->setEnabled(can);
    m_name->setReadOnly(!can);
    setButtonRole(delBtn, "danger");

    connect(addSerial, SIGNAL(clicked()), this, SLOT(onAddSerial()));
    connect(addClient, SIGNAL(clicked()), this, SLOT(onAddTcpClient()));
    connect(addServer, SIGNAL(clicked()), this, SLOT(onAddTcpServer()));
    connect(addUdp, SIGNAL(clicked()), this, SLOT(onAddUdp()));
    connect(addMbRtu, SIGNAL(clicked()), this, SLOT(onAddModbusRtu()));
    connect(addMbTcp, SIGNAL(clicked()), this, SLOT(onAddModbusTcp()));
    connect(delBtn, SIGNAL(clicked()), this, SLOT(onRemove()));
    connect(applyBtn, SIGNAL(clicked()), this, SLOT(onApplyFields()));
    connect(connBtn, SIGNAL(clicked()), this, SLOT(onConnect()));
    connect(discBtn, SIGNAL(clicked()), this, SLOT(onDisconnect()));
    connect(sendBtn, SIGNAL(clicked()), this, SLOT(onSend()));
    connect(mbRead, SIGNAL(clicked()), this, SLOT(onModbusRead()));
    connect(mbWrite, SIGNAL(clicked()), this, SLOT(onModbusWrite()));
    connect(m_list, SIGNAL(itemSelectionChanged()), this, SLOT(onSel()));
    connect(m_protocol, SIGNAL(currentIndexChanged(int)), this, SLOT(onProtocolChanged()));

    // 布局：左为通道列表与新增按钮；右上连接参数，右下收发调试（原始字节或 MODBUS）
    QFrame* leftCard = makeCard(this);
    leftCard->setMinimumWidth(260);
    leftCard->setMaximumWidth(320);
    QVBoxLayout* left = new QVBoxLayout(leftCard);
    QLabel* listTitle = new QLabel(T("通道"), leftCard);
    listTitle->setObjectName(QLatin1String("section"));
    left->addWidget(listTitle);
    left->addWidget(m_list, 1);
    QGridLayout* addGrid = new QGridLayout();
    addGrid->addWidget(addSerial, 0, 0);
    addGrid->addWidget(addClient, 0, 1);
    addGrid->addWidget(addServer, 1, 0);
    addGrid->addWidget(addUdp, 1, 1);
    addGrid->addWidget(addMbRtu, 2, 0);
    addGrid->addWidget(addMbTcp, 2, 1);
    left->addLayout(addGrid);
    left->addWidget(delBtn);

    QGridLayout* head = new QGridLayout();
    head->addWidget(new QLabel(T("名称"), this), 0, 0);
    head->addWidget(m_name, 0, 1);
    head->addWidget(m_type, 0, 2);
    head->addWidget(m_status, 0, 3);
    head->addWidget(m_protocolLabel, 1, 0);
    head->addWidget(m_protocol, 1, 1, 1, 3);
    head->setColumnStretch(1, 1);

    m_rawPanel = new QWidget(this);
    QHBoxLayout* rawLay = new QHBoxLayout(m_rawPanel);
    rawLay->setContentsMargins(0, 0, 0, 0);
    rawLay->addWidget(m_sendEdit, 1);
    rawLay->addWidget(sendBtn);

    m_modbusPanel = new QWidget(this);
    QGridLayout* mbGrid = new QGridLayout(m_modbusPanel);
    mbGrid->setContentsMargins(0, 0, 0, 0);
    mbGrid->addWidget(new QLabel(T("从站号"), m_modbusPanel), 0, 0);
    mbGrid->addWidget(m_mbSlave, 0, 1);
    mbGrid->addWidget(new QLabel(T("数据区"), m_modbusPanel), 0, 2);
    mbGrid->addWidget(m_mbArea, 0, 3);
    mbGrid->addWidget(new QLabel(T("地址"), m_modbusPanel), 1, 0);
    mbGrid->addWidget(m_mbAddr, 1, 1);
    mbGrid->addWidget(new QLabel(T("数量"), m_modbusPanel), 1, 2);
    mbGrid->addWidget(m_mbCount, 1, 3);
    mbGrid->addWidget(new QLabel(T("写值"), m_modbusPanel), 2, 0);
    mbGrid->addWidget(m_mbValue, 2, 1);
    mbGrid->addWidget(mbRead, 2, 2);
    mbGrid->addWidget(mbWrite, 2, 3);

    QFrame* connCard = makeCard(this);
    QVBoxLayout* connLay = new QVBoxLayout(connCard);
    QLabel* connTitle = new QLabel(T("连接"), connCard);
    connTitle->setObjectName(QLatin1String("section"));
    connLay->addWidget(connTitle);
    connLay->addLayout(head);
    connLay->addWidget(m_stack);
    QHBoxLayout* cb = new QHBoxLayout();
    cb->addStretch();
    cb->addWidget(applyBtn);
    cb->addWidget(discBtn);
    cb->addWidget(connBtn);
    connLay->addLayout(cb);

    QFrame* testCard = makeCard(this);
    QVBoxLayout* testLay = new QVBoxLayout(testCard);
    QLabel* testTitle = new QLabel(T("调试"), testCard);
    testTitle->setObjectName(QLatin1String("section"));
    testLay->addWidget(testTitle);
    testLay->addWidget(m_io, 1);
    testLay->addWidget(m_rawPanel);
    testLay->addWidget(m_modbusPanel);

    QVBoxLayout* right = new QVBoxLayout();
    right->addWidget(connCard);
    right->addWidget(testCard, 1);

    QHBoxLayout* body = new QHBoxLayout();
    body->addWidget(leftCard);
    body->addLayout(right, 1);

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(body, 1);

    onRefreshPorts();
    refreshList();
}

// 重列本机串口并尽量保持原选择
void CommMgrDlg::onRefreshPorts()
{
    const std::vector<std::string> ports = CommMgr::instance().listSerialPorts();
    const QString keep = m_serialPort->currentText();
    m_serialPort->clear();
    for (size_t i = 0; i < ports.size(); ++i) {
        m_serialPort->addItem(QString::fromStdString(ports[i]));
    }
    if (ports.empty()) {
        m_serialPort->addItem(QLatin1String("COM1"));
    }
    if (!keep.isEmpty()) {
        m_serialPort->setCurrentText(keep);
    }
}

// 重列 TCP 服务端已接入的对端
void CommMgrDlg::onRefreshPeers()
{
    CommChannel* ch = currentChannel();
    const QString keep = m_peers->currentData().toString();
    m_peers->clear();
    if (!ch) {
        return;
    }
    const std::vector<CommPeer> list = CommMgr::instance().peers(ch->id);
    for (size_t i = 0; i < list.size(); ++i) {
        const QString id = QString::fromStdString(list[i].id);
        QString text = id;
        if (!list[i].remote.empty()) {
            text += QLatin1String("  ") + QString::fromStdString(list[i].remote);
        }
        m_peers->addItem(text, id);
    }
    const int idx = m_peers->findData(keep);
    if (idx >= 0) {
        m_peers->setCurrentIndex(idx);
    }
}

// 按方案通道重建左侧列表并尽量保持选中
void CommMgrDlg::refreshList()
{
    const QString keep = currentId();
    m_list->clear();
    for (size_t i = 0; i < m_sol->commChannels.size(); ++i) {
        const CommChannel& c = m_sol->commChannels[i];
        const bool open = CommMgr::instance().isOpen(c.id);
        QString state = T("未连接");
        if (open && c.type == CommTcpServer) {
            state = T("监听中");
        } else if (open) {
            state = T("已连接");
        }
        QListWidgetItem* it = new QListWidgetItem(
            qs(c.name) + QLatin1String("    ")
                + channelKindText(c.type, c.params.value("protocol").toString() == "modbus")
                + QLatin1String("  ") + QChar(0x00B7) + QLatin1String("  ") + state,
            m_list);
        it->setData(Qt::UserRole, qs(c.id));
    }
    if (!keep.isEmpty()) {
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(Qt::UserRole).toString() == keep) {
                m_list->setCurrentRow(i);
                break;
            }
        }
    } else if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    onSel();
}

// 当前列表项上的通道 id
QString CommMgrDlg::currentId() const
{
    QListWidgetItem* it = m_list->currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString();
}

// TCP 服务端当前对端，其它类型为空
std::string CommMgrDlg::currentPeerId() const
{
    CommChannel* ch = const_cast<CommMgrDlg*>(this)->currentChannel();
    if (!ch || ch->type != CommTcpServer) {
        return std::string();
    }
    return ss(m_peers->currentData().toString());
}

// 按列表选中查找通道
CommChannel* CommMgrDlg::currentChannel()
{
    return m_sol->findCommChannel(oid(currentId()));
}

// 按通道类型切换右侧参数页
void CommMgrDlg::syncTypePage(CommChannelType t)
{
    if (t == CommTcpClient) {
        m_stack->setCurrentIndex(1);
    } else if (t == CommTcpServer) {
        m_stack->setCurrentIndex(2);
    } else if (t == CommUdp) {
        m_stack->setCurrentIndex(3);
    } else {
        m_stack->setCurrentIndex(0);
    }
}

// 按协议显隐原始收发或 Modbus 测试区
void CommMgrDlg::syncActionPanel()
{
    CommChannel* ch = currentChannel();
    const bool udp = ch && ch->type == CommUdp;
    m_protocolLabel->setVisible(!udp);
    m_protocol->setVisible(!udp);
    const QString proto = m_protocol->currentData().toString();
    const bool modbus = !udp && (proto == QLatin1String("master") || proto == QLatin1String("slave"));
    m_rawPanel->setVisible(!modbus);
    m_modbusPanel->setVisible(modbus);
}

// 协议变化后更新类型标签和测试区
void CommMgrDlg::onProtocolChanged()
{
    CommChannel* ch = currentChannel();
    if (ch) {
        const QString proto = m_protocol->currentData().toString();
        const bool modbus = proto == QLatin1String("master") || proto == QLatin1String("slave");
        m_type->setText(channelKindText(ch->type, modbus && ch->type != CommUdp));
    }
    syncActionPanel();
}

// 把通道参数灌进右侧控件
void CommMgrDlg::loadFields(const CommChannel& ch)
{
    m_name->setText(qs(ch.name));
    syncTypePage(ch.type);
    const std::string protocol = ch.params.value("protocol").toString();
    const std::string role = ch.params.value("role").toString();
    QString proto = QLatin1String("raw");
    if (protocol == "modbus") {
        proto = (role == "slave") ? QLatin1String("slave") : QLatin1String("master");
    }
    m_protocol->setCurrentIndex(m_protocol->findData(proto));
    m_type->setText(channelKindText(ch.type, protocol == "modbus"));
    m_serialPort->setCurrentText(QString::fromStdString(ch.params.value("port").toString()));
    m_baud->setValue(ch.params.value("baud").toInt(9600));
    m_dataBits->setValue(ch.params.value("dataBits").toInt(8));
    const QString parity = QString::fromStdString(ch.params.value("parity").toString());
    m_parity->setCurrentIndex(m_parity->findData(parity.isEmpty() ? QLatin1String("none") : parity));
    m_stopBits->setValue(ch.params.value("stopBits").toInt(1));
    m_host->setText(qs(ch.params.value("host").toString()));
    m_tcpPort->setValue(ch.params.value("port").toInt(9000));
    m_listenPort->setValue(ch.params.value("port").toInt(9000));
    m_udpBind->setValue(ch.params.value("port").toInt(9000));
    m_udpHost->setText(qs(ch.params.value("host").toString()));
    m_udpRemote->setValue(ch.params.value("remotePort").toInt(9000));
    m_mbSlave->setValue(ch.params.value("slaveId").toInt(1));
    const bool open = CommMgr::instance().isOpen(ch.id);
    if (open && ch.type == CommTcpServer) {
        paintConnStatus(m_status, T("监听中"), true);
    } else {
        paintConnStatus(m_status, open ? T("已连接") : T("未连接"), open);
    }
    if (ch.type == CommTcpServer) {
        onRefreshPeers();
    }
    syncActionPanel();
}

// 把右侧控件写回通道参数
void CommMgrDlg::saveFieldsTo(CommChannel* ch)
{
    if (!ch) {
        return;
    }
    ch->name = ss(m_name->text().trimmed());
    if (ch->type == CommUdp) {
        ch->params.insert("protocol", std::string("raw"));
        ch->params.insert("role", std::string("master"));
    } else {
        const QString proto = m_protocol->currentData().toString();
        if (proto == QLatin1String("master") || proto == QLatin1String("slave")) {
            ch->params.insert("protocol", std::string("modbus"));
            ch->params.insert("role", ss(proto));
        } else {
            ch->params.insert("protocol", std::string("raw"));
            ch->params.insert("role", std::string("master"));
        }
    }
    ch->params.insert("slaveId", m_mbSlave->value());
    if (ch->type == CommSerial) {
        ch->params.insert("port", ss(m_serialPort->currentText()));
        ch->params.insert("baud", m_baud->value());
        ch->params.insert("dataBits", m_dataBits->value());
        ch->params.insert("parity", ss(m_parity->currentData().toString()));
        ch->params.insert("stopBits", m_stopBits->value());
    } else if (ch->type == CommTcpClient) {
        ch->params.insert("host", ss(m_host->text().trimmed()));
        ch->params.insert("port", m_tcpPort->value());
    } else if (ch->type == CommTcpServer) {
        ch->params.insert("port", m_listenPort->value());
    } else {
        ch->params.insert("port", m_udpBind->value());
        ch->params.insert("host", ss(m_udpHost->text().trimmed()));
        ch->params.insert("remotePort", m_udpRemote->value());
    }
}

// 选中通道后加载其参数
void CommMgrDlg::onSel()
{
    CommChannel* ch = currentChannel();
    if (!ch) {
        return;
    }
    loadFields(*ch);
}

// 追加一条通道并选中
void CommMgrDlg::addChannel(CommChannelType type, const char* nameKey, bool modbus)
{
    CommChannel c;
    c.id = ObjectId::create();
    c.type = type;
    c.enabled = true;
    std::vector<std::string> nums;
    nums.push_back(std::to_string(static_cast<int>(m_sol->commChannels.size()) + 1));
    c.name = Translate::format(nameKey, nums);
    c.params.insert("protocol", modbus ? std::string("modbus") : std::string("raw"));
    c.params.insert("role", std::string("master"));
    c.params.insert("slaveId", 1);
    if (type == CommSerial) {
        c.params.insert("port", std::string("COM1"));
        c.params.insert("baud", 9600);
        c.params.insert("dataBits", 8);
        c.params.insert("parity", modbus ? std::string("even") : std::string("none"));
        c.params.insert("stopBits", 1);
    } else if (type == CommTcpClient) {
        c.params.insert("host", std::string("127.0.0.1"));
        c.params.insert("port", modbus ? 502 : 9000);
    } else if (type == CommTcpServer) {
        c.params.insert("port", modbus ? 502 : 9000);
    } else {
        c.params.insert("port", 9000);
        c.params.insert("host", std::string("127.0.0.1"));
        c.params.insert("remotePort", 9000);
    }
    m_sol->commChannels.push_back(c);
    m_sol->dirty = true;
    refreshList();
    m_list->setCurrentRow(m_list->count() - 1);
}

// 新增原始串口通道
void CommMgrDlg::onAddSerial()
{
    addChannel(CommSerial, "串口{0}", false);
}

// 新增 TCP 客户端通道
void CommMgrDlg::onAddTcpClient()
{
    addChannel(CommTcpClient, "TCP客户端{0}", false);
}

// 新增 TCP 服务端通道
void CommMgrDlg::onAddTcpServer()
{
    addChannel(CommTcpServer, "TCP服务端{0}", false);
}

// 新增 UDP 通道
void CommMgrDlg::onAddUdp()
{
    addChannel(CommUdp, "UDP{0}", false);
}

// 新增 Modbus RTU 通道
void CommMgrDlg::onAddModbusRtu()
{
    addChannel(CommSerial, "MODBUS RTU{0}", true);
}

// 新增 Modbus TCP 客户端通道
void CommMgrDlg::onAddModbusTcp()
{
    addChannel(CommTcpClient, "MODBUS TCP{0}", true);
}

// 确认后删除当前通道并断开
void CommMgrDlg::onRemove()
{
    const QString id = currentId();
    if (id.isEmpty()) {
        return;
    }
    if (QMessageBox::question(this, T("删除"), T("删除该通信通道？")) != QMessageBox::Yes) {
        return;
    }
    CommMgr::instance().closeChannel(oid(id));
    for (size_t i = 0; i < m_sol->commChannels.size(); ++i) {
        if (qs(m_sol->commChannels[i].id) == id) {
            vpRemoveAt(m_sol->commChannels, static_cast<int>(i));
            m_sol->dirty = true;
            break;
        }
    }
    refreshList();
}

// 把界面参数写回方案
void CommMgrDlg::onApplyFields()
{
    CommChannel* ch = currentChannel();
    if (!ch || !m_session->has(PermEditComm)) {
        return;
    }
    saveFieldsTo(ch);
    m_sol->dirty = true;
    refreshList();
    m_io->appendPlainText(T("参数已应用。已连接时请重新连接。"));
}

// 保存参数并打开当前通道
void CommMgrDlg::onConnect()
{
    CommChannel* ch = currentChannel();
    if (!ch) {
        return;
    }
    saveFieldsTo(ch);
    std::string err;
    if (!CommMgr::instance().openChannel(*ch, &err)) {
        QMessageBox::warning(this, T("连接"), qs(err));
        paintConnStatus(m_status, T("未连接"), false);
        return;
    }
    m_sol->dirty = true;
    if (ch->type == CommTcpServer) {
        paintConnStatus(m_status, T("监听中"), true);
        onRefreshPeers();
    } else {
        paintConnStatus(m_status, T("已连接"), true);
    }
    refreshList();
    m_io->appendPlainText(T("已连接：") + qs(ch->name));
}

// 关闭当前通道
void CommMgrDlg::onDisconnect()
{
    CommChannel* ch = currentChannel();
    if (!ch) {
        return;
    }
    CommMgr::instance().closeChannel(ch->id);
    paintConnStatus(m_status, T("未连接"), false);
    m_peers->clear();
    refreshList();
    m_io->appendPlainText(T("已断开。"));
}

// 发送原始字节并尝试读回
void CommMgrDlg::onSend()
{
    CommChannel* ch = currentChannel();
    if (!ch) {
        return;
    }
    if (ch->params.value("protocol").toString() == "modbus") {
        m_io->appendPlainText(T("MODBUS 通道请用下面的读/写。"));
        return;
    }
    std::string err;
    const std::string peer = currentPeerId();
    const CommBytes data = toBytes(m_sendEdit->text().toUtf8());
    if (!CommMgr::instance().writeBytes(ch->id, peer, data, &err)) {
        m_io->appendPlainText(T("发送失败：") + qs(err));
        return;
    }
    m_io->appendPlainText(T(">> ") + m_sendEdit->text());
    // 发送行若为 SetGlobalValue: 则写入方案全局变量
    if (m_sol) {
        m_sol->globalVars.applyCommLine(ss(m_sendEdit->text()));
    }
    const CommBytes resp = CommMgr::instance().readBytes(ch->id, peer, 4096, 300, &err);
    if (!resp.empty()) {
        const QString text = fromBytes(resp);
        m_io->appendPlainText(T("<< ") + text);
        // 回包按行尝试 SetGlobalValue:
        if (m_sol) {
            const QStringList lines = text.split(QRegExp(QString::fromLatin1("[\r\n]+")), QString::SkipEmptyParts);
            for (int i = 0; i < lines.size(); ++i) {
                m_sol->globalVars.applyCommLine(ss(lines[i]));
            }
        }
    }
}

// 按当前区与地址读 Modbus
void CommMgrDlg::onModbusRead()
{
    CommChannel* ch = currentChannel();
    if (!ch) {
        return;
    }
    saveFieldsTo(ch);
    if (ch->params.value("protocol").toString() != "modbus") {
        QMessageBox::information(this, T("MODBUS"), T("请把协议选成 MODBUS 并重新连接。"));
        return;
    }
    const int area = m_mbArea->currentData().toInt();
    const std::string peer = currentPeerId();
    std::string err;
    if (area == ModbusHolding || area == ModbusInput) {
        std::vector<uint16_t> vals;
        if (!CommMgr::instance().modbusReadRegs(ch->id, peer, m_mbSlave->value(), static_cast<ModbusTable>(area),
                                                m_mbAddr->value(), m_mbCount->value(), &vals, &err)) {
            m_io->appendPlainText(T("读失败：") + qs(err));
            return;
        }
        QString line = T("读寄存器：");
        for (size_t i = 0; i < vals.size(); ++i) {
            if (i) {
                line += QLatin1Char(',');
            }
            line += QString::number(vals[i]);
        }
        m_io->appendPlainText(line);
        return;
    }
    std::vector<unsigned char> bits;
    if (!CommMgr::instance().modbusReadBits(ch->id, peer, m_mbSlave->value(), static_cast<ModbusTable>(area),
                                            m_mbAddr->value(), m_mbCount->value(), &bits, &err)) {
        m_io->appendPlainText(T("读失败：") + qs(err));
        return;
    }
    QString line = T("读位：");
    for (size_t i = 0; i < bits.size(); ++i) {
        if (i) {
            line += QLatin1Char(',');
        }
        line += QString::number(bits[i] ? 1 : 0);
    }
    m_io->appendPlainText(line);
}

// 按当前区与地址写 Modbus
void CommMgrDlg::onModbusWrite()
{
    CommChannel* ch = currentChannel();
    if (!ch) {
        return;
    }
    saveFieldsTo(ch);
    if (ch->params.value("protocol").toString() != "modbus") {
        QMessageBox::information(this, T("MODBUS"), T("请把协议选成 MODBUS 并重新连接。"));
        return;
    }
    const int area = m_mbArea->currentData().toInt();
    const std::string peer = currentPeerId();
    std::string err;
    const int count = m_mbCount->value();
    if (area == ModbusHolding || area == ModbusInput) {
        std::vector<uint16_t> regs(static_cast<size_t>(count), static_cast<uint16_t>(m_mbValue->value()));
        if (!CommMgr::instance().modbusWriteRegs(ch->id, peer, m_mbSlave->value(), static_cast<ModbusTable>(area),
                                                 m_mbAddr->value(), regs, &err)) {
            m_io->appendPlainText(T("写失败：") + qs(err));
            return;
        }
    } else {
        std::vector<unsigned char> bits(static_cast<size_t>(count), m_mbValue->value() ? 1 : 0);
        if (!CommMgr::instance().modbusWriteBits(ch->id, peer, m_mbSlave->value(), static_cast<ModbusTable>(area),
                                                 m_mbAddr->value(), bits, &err)) {
            m_io->appendPlainText(T("写失败：") + qs(err));
            return;
        }
    }
    m_io->appendPlainText(T("写成功。"));
}

}

#include "moc_CommMgrDlg.cpp"
