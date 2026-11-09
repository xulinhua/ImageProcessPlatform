/**
 * @file channel_page.cpp
 * @brief 各通道参数，八页同一套标签在上、输入在下
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "channel_page.h"
#include "CommMgr.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

class FieldRow : public QWidget {
public:
    explicit FieldRow(QWidget* parent = 0)
        : QWidget(parent)
        , m_grid(new QGridLayout(this))
        , m_col(0)
    {
        m_grid->setContentsMargins(0, 0, 0, 0);
        m_grid->setHorizontalSpacing(8);
        m_grid->setVerticalSpacing(2);
    }

    void add(const QString& label, QWidget* field, int stretch = 1)
    {
        QLabel* lab = new QLabel(label, this);
        lab->setObjectName("hint");
        m_grid->addWidget(lab, 0, m_col);
        m_grid->addWidget(field, 1, m_col);
        m_grid->setColumnStretch(m_col, stretch);
        ++m_col;
    }

    void addWidget(QWidget* field)
    {
        m_grid->addWidget(field, 1, m_col, Qt::AlignBottom);
        ++m_col;
    }

private:
    QGridLayout* m_grid;
    int m_col;
};

class SerialFields : public FieldRow {
public:
    explicit SerialFields(QWidget* parent = 0)
        : FieldRow(parent)
    {
        m_port = new QComboBox(this);
        m_baud = new QComboBox(this);
        m_baud->setEditable(true);
        m_baud->addItems(QStringList() << "9600" << "19200" << "38400" << "57600" << "115200" << "921600");
        m_baud->setCurrentText("9600");
        m_bits = new QComboBox(this);
        m_bits->addItems(QStringList() << "8" << "7");
        m_parity = new QComboBox(this);
        m_parity->addItem(QString::fromUtf8("无"), "none");
        m_parity->addItem(QString::fromUtf8("偶"), "even");
        m_parity->addItem(QString::fromUtf8("奇"), "odd");
        m_stop = new QComboBox(this);
        m_stop->addItems(QStringList() << "1" << "2");
        m_flow = new QComboBox(this);
        m_flow->addItem(QString::fromUtf8("无"), "none");
        m_flow->addItem("RTS/CTS", "rtscts");
        m_dtr = new QCheckBox("DTR", this);
        m_dtr->setChecked(true);
        m_rts = new QCheckBox("RTS", this);
        m_rts->setChecked(true);
        QPushButton* refresh = new QPushButton(QString::fromUtf8("刷新"), this);
        connect(refresh, &QPushButton::clicked, this, [this]() { reloadPorts(); });
        connect(m_flow, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
            m_rts->setEnabled(m_flow->currentData().toString() != "rtscts");
        });
        QWidget* portBox = new QWidget(this);
        QHBoxLayout* portLay = new QHBoxLayout(portBox);
        portLay->setContentsMargins(0, 0, 0, 0);
        portLay->addWidget(m_port, 1);
        portLay->addWidget(refresh);
        add(QString::fromUtf8("端口"), portBox, 2);
        add(QString::fromUtf8("波特率"), m_baud);
        add(QString::fromUtf8("数据位"), m_bits);
        add(QString::fromUtf8("校验"), m_parity);
        add(QString::fromUtf8("停止位"), m_stop);
        add(QString::fromUtf8("流控"), m_flow);
        addWidget(m_dtr);
        addWidget(m_rts);
        m_lock << m_port << m_baud << m_bits << m_parity << m_stop << m_flow << refresh;
        reloadPorts();
    }

    void write(Vision::CommChannel* channel) const
    {
        channel->params.insert("port", m_port->currentText().toUtf8().constData());
        channel->params.insert("baud", m_baud->currentText().toInt());
        channel->params.insert("dataBits", m_bits->currentText().toInt());
        channel->params.insert("parity", m_parity->currentData().toString().toUtf8().constData());
        channel->params.insert("stopBits", m_stop->currentText().toInt());
        channel->params.insert("flow", m_flow->currentData().toString().toUtf8().constData());
        channel->params.insert("dtr", m_dtr->isChecked() ? 1 : 0);
        channel->params.insert("rts", m_rts->isChecked() ? 1 : 0);
    }

    void setLocked(bool locked)
    {
        for (int i = 0; i < m_lock.size(); ++i) {
            m_lock[i]->setEnabled(!locked);
        }
    }

    bool dtr() const { return m_dtr->isChecked(); }
    bool rts() const { return m_rts->isChecked(); }
    QCheckBox* dtrBox() const { return m_dtr; }
    QCheckBox* rtsBox() const { return m_rts; }

    QString summary() const
    {
        const QString parity = m_parity->currentData().toString() == "even" ? "E"
            : (m_parity->currentData().toString() == "odd" ? "O" : "N");
        return m_port->currentText() + "  " + m_baud->currentText() + " " + m_bits->currentText()
            + parity + m_stop->currentText();
    }

private:
    void reloadPorts()
    {
        const QString keep = m_port->currentText();
        m_port->clear();
        const std::vector<std::string> names = Vision::CommMgr::instance().listSerialPorts();
        for (size_t i = 0; i < names.size(); ++i) {
            m_port->addItem(QString::fromUtf8(names[i].c_str()));
        }
        const int at = m_port->findText(keep);
        if (at >= 0) {
            m_port->setCurrentIndex(at);
        }
    }

    QComboBox* m_port;
    QComboBox* m_baud;
    QComboBox* m_bits;
    QComboBox* m_parity;
    QComboBox* m_stop;
    QComboBox* m_flow;
    QCheckBox* m_dtr;
    QCheckBox* m_rts;
    QList<QWidget*> m_lock;
};

class SerialPage : public ChannelPage {
public:
    explicit SerialPage(QWidget* parent = 0) : ChannelPage(parent), m_fields(new SerialFields(this))
    {
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->addWidget(m_fields);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommSerial;
        channel->params.insert("protocol", "raw");
        m_fields->write(channel);
    }
    QString summary() const override { return m_fields->summary(); }
    void setLocked(bool locked) override { m_fields->setLocked(locked); }
    bool serialLines() const override { return true; }
    bool dtr() const override { return m_fields->dtr(); }
    bool rts() const override { return m_fields->rts(); }
    SerialFields* fields() const { return m_fields; }
private:
    SerialFields* m_fields;
};

class TcpClientPage : public ChannelPage {
public:
    explicit TcpClientPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_host = new QLineEdit("192.168.1.10", this);
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(9000);
        m_local = new QSpinBox(this);
        m_local->setRange(0, 65535);
        m_timeout = new QSpinBox(this);
        m_timeout->setRange(100, 60000);
        m_timeout->setValue(3000);
        m_timeout->setSuffix(" ms");
        FieldRow* row = new FieldRow(this);
        row->add(QString::fromUtf8("远程主机"), m_host, 2);
        row->add(QString::fromUtf8("远程端口"), m_port);
        row->add(QString::fromUtf8("本地端口"), m_local);
        row->add(QString::fromUtf8("连接超时"), m_timeout);
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->addWidget(row);
        m_lock << m_host << m_port << m_local << m_timeout;
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommTcpClient;
        channel->params.insert("protocol", "raw");
        channel->params.insert("host", m_host->text().trimmed().toUtf8().constData());
        channel->params.insert("port", m_port->value());
        channel->params.insert("localPort", m_local->value());
        channel->params.insert("timeoutMs", m_timeout->value());
    }
    QString summary() const override
    {
        return m_host->text().trimmed() + ":" + QString::number(m_port->value());
    }
    void setLocked(bool locked) override
    {
        for (int i = 0; i < m_lock.size(); ++i) {
            m_lock[i]->setEnabled(!locked);
        }
    }
private:
    QLineEdit* m_host;
    QSpinBox* m_port;
    QSpinBox* m_local;
    QSpinBox* m_timeout;
    QList<QWidget*> m_lock;
};

class TcpServerPage : public ChannelPage {
public:
    explicit TcpServerPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_host = new QLineEdit("0.0.0.0", this);
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(9000);
        FieldRow* row = new FieldRow(this);
        row->add(QString::fromUtf8("监听地址"), m_host, 2);
        row->add(QString::fromUtf8("监听端口"), m_port);
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->addWidget(row);
        m_lock << m_host << m_port;
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommTcpServer;
        channel->params.insert("protocol", "raw");
        channel->params.insert("host", m_host->text().trimmed().toUtf8().constData());
        channel->params.insert("port", m_port->value());
    }
    bool server() const override { return true; }
    QString summary() const override
    {
        return QString::fromUtf8("监听 ") + m_host->text().trimmed() + ":" + QString::number(m_port->value());
    }
    void setLocked(bool locked) override
    {
        for (int i = 0; i < m_lock.size(); ++i) {
            m_lock[i]->setEnabled(!locked);
        }
    }
private:
    QLineEdit* m_host;
    QSpinBox* m_port;
    QList<QWidget*> m_lock;
};

class UdpPage : public ChannelPage {
public:
    explicit UdpPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(9000);
        m_host = new QLineEdit("192.168.1.20", this);
        m_remote = new QSpinBox(this);
        m_remote->setRange(1, 65535);
        m_remote->setValue(9000);
        m_broadcast = new QCheckBox(QString::fromUtf8("广播"), this);
        connect(m_broadcast, &QCheckBox::toggled, this, [this](bool on) {
            m_host->setEnabled(!on);
            if (on) {
                m_savedHost = m_host->text();
                m_host->setText("255.255.255.255");
            } else if (!m_savedHost.isEmpty()) {
                m_host->setText(m_savedHost);
            }
        });
        FieldRow* row = new FieldRow(this);
        row->add(QString::fromUtf8("本地端口"), m_port);
        row->add(QString::fromUtf8("远程主机"), m_host, 2);
        row->add(QString::fromUtf8("远程端口"), m_remote);
        row->addWidget(m_broadcast);
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->addWidget(row);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommUdp;
        channel->params.insert("protocol", "raw");
        channel->params.insert("port", m_port->value());
        channel->params.insert("host", udpHost().toUtf8().constData());
        channel->params.insert("remotePort", m_remote->value());
    }
    bool udp() const override { return true; }
    QString summary() const override
    {
        return QString::fromUtf8("本地 :") + QString::number(m_port->value())
            + QString::fromUtf8("  →  ") + udpHost() + ":" + QString::number(m_remote->value());
    }
    void setLocked(bool locked) override { m_port->setEnabled(!locked); }
    QString udpHost() const override { return m_host->text().trimmed(); }
    int udpPort() const override { return m_remote->value(); }
    QLineEdit* hostEdit() const { return m_host; }
    QSpinBox* remotePort() const { return m_remote; }
private:
    QSpinBox* m_port;
    QLineEdit* m_host;
    QSpinBox* m_remote;
    QCheckBox* m_broadcast;
    QString m_savedHost;
};

class HttpPage : public ChannelPage {
public:
    explicit HttpPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_url = new QLineEdit("http://192.168.1.10/api/status", this);
        m_timeout = new QSpinBox(this);
        m_timeout->setRange(100, 120000);
        m_timeout->setValue(5000);
        m_timeout->setSuffix(" ms");
        FieldRow* row = new FieldRow(this);
        row->add("URL", m_url, 3);
        row->add(QString::fromUtf8("超时"), m_timeout);
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->addWidget(row);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommHttp;
        channel->params.insert("url", m_url->text().trimmed().toUtf8().constData());
        channel->params.insert("timeoutMs", m_timeout->value());
    }
    bool stream() const override { return false; }
    bool http() const override { return true; }
    QString summary() const override { return "HTTP"; }
    QString url() const { return m_url->text().trimmed(); }
    void setLocked(bool locked) override
    {
        m_url->setEnabled(!locked);
        m_timeout->setEnabled(!locked);
    }
private:
    QLineEdit* m_url;
    QSpinBox* m_timeout;
};

class WebSocketPage : public ChannelPage {
public:
    explicit WebSocketPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_url = new QLineEdit("ws://192.168.1.10/ws", this);
        FieldRow* row = new FieldRow(this);
        row->add("URL", m_url, 1);
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->addWidget(row);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommWebSocket;
        channel->params.insert("url", m_url->text().trimmed().toUtf8().constData());
        channel->params.insert("timeoutMs", 5000);
    }
    bool stream() const override { return false; }
    bool webSocket() const override { return true; }
    QString summary() const override { return m_url->text().trimmed(); }
    void setLocked(bool locked) override { m_url->setEnabled(!locked); }
private:
    QLineEdit* m_url;
};

class ModbusRtuPage : public ChannelPage {
public:
    explicit ModbusRtuPage(QWidget* parent = 0) : ChannelPage(parent), m_fields(new SerialFields(this))
    {
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->addWidget(m_fields);
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommSerial;
        channel->params.insert("protocol", "modbus");
        channel->params.insert("role", "master");
        m_fields->write(channel);
    }
    bool stream() const override { return false; }
    bool modbus() const override { return true; }
    QString summary() const override { return m_fields->summary(); }
    void setLocked(bool locked) override { m_fields->setLocked(locked); }
    bool serialLines() const override { return true; }
    bool dtr() const override { return m_fields->dtr(); }
    bool rts() const override { return m_fields->rts(); }
    SerialFields* fields() const { return m_fields; }
private:
    SerialFields* m_fields;
};

class ModbusTcpPage : public ChannelPage {
public:
    explicit ModbusTcpPage(QWidget* parent = 0) : ChannelPage(parent)
    {
        m_host = new QLineEdit("192.168.1.10", this);
        m_port = new QSpinBox(this);
        m_port->setRange(1, 65535);
        m_port->setValue(502);
        FieldRow* row = new FieldRow(this);
        row->add(QString::fromUtf8("主机"), m_host, 2);
        row->add(QString::fromUtf8("端口"), m_port);
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->addWidget(row);
        m_lock << m_host << m_port;
    }
    void fill(Vision::CommChannel* channel) const override
    {
        channel->type = Vision::CommTcpClient;
        channel->params.insert("protocol", "modbus");
        channel->params.insert("role", "master");
        channel->params.insert("host", m_host->text().trimmed().toUtf8().constData());
        channel->params.insert("port", m_port->value());
    }
    bool stream() const override { return false; }
    bool modbus() const override { return true; }
    QString summary() const override
    {
        return m_host->text().trimmed() + ":" + QString::number(m_port->value());
    }
    void setLocked(bool locked) override
    {
        for (int i = 0; i < m_lock.size(); ++i) {
            m_lock[i]->setEnabled(!locked);
        }
    }
private:
    QLineEdit* m_host;
    QSpinBox* m_port;
    QList<QWidget*> m_lock;
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
