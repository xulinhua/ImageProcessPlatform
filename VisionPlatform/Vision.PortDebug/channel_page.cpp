/**
 * @file channel_page.cpp
 * @brief 各通道参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "channel_page.h"
#include "CommMgr.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

namespace {

void addSerialRows(QFormLayout* form, QComboBox** port, QSpinBox** baud, QComboBox** bits, QComboBox** parity, QComboBox** stop, QWidget* parent)
{
    *port = new QComboBox(parent);
    *baud = new QSpinBox(parent);
    (*baud)->setRange(300, 921600);
    (*baud)->setValue(9600);
    *bits = new QComboBox(parent);
    (*bits)->addItems(QStringList() << "8" << "7" << "6" << "5");
    *parity = new QComboBox(parent);
    (*parity)->addItems(QStringList() << "none" << "even" << "odd");
    *stop = new QComboBox(parent);
    (*stop)->addItems(QStringList() << "1" << "2");
    QPushButton* refresh = new QPushButton(QString::fromUtf8("刷新"), parent);
    QObject::connect(refresh, &QPushButton::clicked, parent, [port]() {
        (*port)->clear();
        const std::vector<std::string> names = Vision::CommMgr::instance().listSerialPorts();
        for (size_t i = 0; i < names.size(); ++i) {
            (*port)->addItem(QString::fromUtf8(names[i].c_str()));
        }
    });
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(*port, 1);
    row->addWidget(refresh);
    form->addRow(QString::fromUtf8("串口"), row);
    form->addRow(QString::fromUtf8("波特率"), *baud);
    form->addRow(QString::fromUtf8("数据位"), *bits);
    form->addRow(QString::fromUtf8("校验"), *parity);
    form->addRow(QString::fromUtf8("停止位"), *stop);
    refresh->click();
}

void writeSerial(Vision::CommChannel* channel, QComboBox* port, QSpinBox* baud, QComboBox* bits, QComboBox* parity, QComboBox* stop)
{
    channel->params.insert("port", port->currentText().toUtf8().constData());
    channel->params.insert("baud", baud->value());
    channel->params.insert("dataBits", bits->currentText().toInt());
    channel->params.insert("parity", parity->currentText().toUtf8().constData());
    channel->params.insert("stopBits", stop->currentText().toInt());
}

class SerialPage : public ChannelPage {
public:
    explicit SerialPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        QFormLayout* form = new QFormLayout(this);
        addSerialRows(form, &m_port, &m_baud, &m_bits, &m_parity, &m_stop, this);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommSerial;
        channel->params.insert("protocol", "raw");
        writeSerial(channel, m_port, m_baud, m_bits, m_parity, m_stop);
    }
private:
    QComboBox* m_port;
    QSpinBox* m_baud;
    QComboBox* m_bits;
    QComboBox* m_parity;
    QComboBox* m_stop;
};

class TcpClientPage : public ChannelPage {
public:
    explicit TcpClientPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_host = new QLineEdit("127.0.0.1", this);
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(9000);
        QFormLayout* form = new QFormLayout(this);
        form->addRow(QString::fromUtf8("主机"), m_host);
        form->addRow(QString::fromUtf8("端口"), m_port);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommTcpClient;
        channel->params.insert("protocol", "raw");
        channel->params.insert("host", m_host->text().trimmed().toUtf8().constData());
        channel->params.insert("port", m_port->value());
    }
private:
    QLineEdit* m_host;
    QSpinBox* m_port;
};

class TcpServerPage : public ChannelPage {
public:
    explicit TcpServerPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(9000);
        QFormLayout* form = new QFormLayout(this);
        form->addRow(QString::fromUtf8("监听端口"), m_port);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommTcpServer;
        channel->params.insert("protocol", "raw");
        channel->params.insert("port", m_port->value());
    }
private:
    QSpinBox* m_port;
};

class UdpPage : public ChannelPage {
public:
    explicit UdpPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(9000);
        m_host = new QLineEdit("127.0.0.1", this);
        m_remote = new QSpinBox(this);
        m_remote->setRange(1, 65535);
        m_remote->setValue(9001);
        QFormLayout* form = new QFormLayout(this);
        form->addRow(QString::fromUtf8("本机端口"), m_port);
        form->addRow(QString::fromUtf8("目标主机"), m_host);
        form->addRow(QString::fromUtf8("目标端口"), m_remote);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommUdp;
        channel->params.insert("protocol", "raw");
        channel->params.insert("port", m_port->value());
        channel->params.insert("host", m_host->text().trimmed().toUtf8().constData());
        channel->params.insert("remotePort", m_remote->value());
    }
private:
    QSpinBox* m_port;
    QLineEdit* m_host;
    QSpinBox* m_remote;
};

class HttpPage : public ChannelPage {
public:
    explicit HttpPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_url = new QLineEdit("http://127.0.0.1/", this);
        QFormLayout* form = new QFormLayout(this);
        form->addRow("URL", m_url);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommHttp;
        channel->params.insert("url", m_url->text().trimmed().toUtf8().constData());
        channel->params.insert("timeoutMs", 5000);
    }
    bool stream() const override { return false; }
    bool http() const override { return true; }
    QString url() const { return m_url->text().trimmed(); }
private:
    QLineEdit* m_url;
};

class WebSocketPage : public ChannelPage {
public:
    explicit WebSocketPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_url = new QLineEdit("ws://127.0.0.1/", this);
        QFormLayout* form = new QFormLayout(this);
        form->addRow("URL", m_url);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommWebSocket;
        channel->params.insert("url", m_url->text().trimmed().toUtf8().constData());
        channel->params.insert("timeoutMs", 5000);
    }
    bool stream() const override { return false; }
    bool webSocket() const override { return true; }
private:
    QLineEdit* m_url;
};

class ModbusRtuPage : public ChannelPage {
public:
    explicit ModbusRtuPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        QFormLayout* form = new QFormLayout(this);
        addSerialRows(form, &m_port, &m_baud, &m_bits, &m_parity, &m_stop, this);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommSerial;
        channel->params.insert("protocol", "modbus");
        channel->params.insert("role", "master");
        writeSerial(channel, m_port, m_baud, m_bits, m_parity, m_stop);
    }
    bool modbus() const override { return true; }
private:
    QComboBox* m_port;
    QSpinBox* m_baud;
    QComboBox* m_bits;
    QComboBox* m_parity;
    QComboBox* m_stop;
};

class ModbusTcpPage : public ChannelPage {
public:
    explicit ModbusTcpPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_host = new QLineEdit("127.0.0.1", this);
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(502);
        QFormLayout* form = new QFormLayout(this);
        form->addRow(QString::fromUtf8("主机"), m_host);
        form->addRow(QString::fromUtf8("端口"), m_port);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommTcpClient;
        channel->params.insert("protocol", "modbus");
        channel->params.insert("role", "master");
        channel->params.insert("host", m_host->text().trimmed().toUtf8().constData());
        channel->params.insert("port", m_port->value());
    }
    bool modbus() const override { return true; }
private:
    QLineEdit* m_host;
    QSpinBox* m_port;
};

}

ChannelPage* createSerialPage(QWidget* parent) { return new SerialPage(parent); }
ChannelPage* createTcpClientPage(QWidget* parent) { return new TcpClientPage(parent); }
ChannelPage* createTcpServerPage(QWidget* parent) { return new TcpServerPage(parent); }
ChannelPage* createUdpPage(QWidget* parent) { return new UdpPage(parent); }
ChannelPage* createHttpPage(QWidget* parent) { return new HttpPage(parent); }
ChannelPage* createWebSocketPage(QWidget* parent) { return new WebSocketPage(parent); }
ChannelPage* createModbusRtuPage(QWidget* parent) { return new ModbusRtuPage(parent); }
ChannelPage* createModbusTcpPage(QWidget* parent) { return new ModbusTcpPage(parent); }
