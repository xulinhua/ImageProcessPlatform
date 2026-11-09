/**
 * @file open_panel.cpp
 * @brief 打开参数区
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "open_panel.h"
#include "hs_ipc.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

OpenPanel::OpenPanel(QWidget* parent)
    : QWidget(parent)
{
    m_transport = new QComboBox(this);
    m_transport->addItem(QString::fromUtf8("共享内存"), HS_IPC_SHARED_MEMORY);
    m_transport->addItem(QString::fromUtf8("命名管道"), HS_IPC_NAMED_PIPE);
    m_transport->addItem(QString::fromUtf8("消息队列"), HS_IPC_MESSAGE_QUEUE);
    m_transport->addItem(QString::fromUtf8("套接字"), HS_IPC_SOCKET);

    m_name = new QLineEdit("Hs.VisionMotion", this);

    m_capacity = new QSpinBox(this);
    m_capacity->setRange(1, 256);
    m_capacity->setValue(32);
    m_slotMb = new QSpinBox(this);
    m_slotMb->setRange(1, 64);
    m_slotMb->setValue(1);
    m_slotMb->setSuffix(" MB");

    QFormLayout* shmForm = new QFormLayout();
    shmForm->addRow(QString::fromUtf8("信号环容量"), m_capacity);
    shmForm->addRow(QString::fromUtf8("图像槽"), m_slotMb);
    m_shm = new QWidget(this);
    m_shm->setLayout(shmForm);

    m_host = new QLineEdit(this);
    m_host->setPlaceholderText("127.0.0.1");
    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(28121);
    QFormLayout* sockForm = new QFormLayout();
    sockForm->addRow(QString::fromUtf8("主机"), m_host);
    sockForm->addRow(QString::fromUtf8("端口"), m_port);
    m_socket = new QWidget(this);
    m_socket->setLayout(sockForm);

    m_open = new QPushButton(QString::fromUtf8("打开"), this);
    m_open->setObjectName("primary");
    m_close = new QPushButton(QString::fromUtf8("关闭"), this);
    m_close->setEnabled(false);
    m_status = new QLabel(QString::fromUtf8("未打开"), this);
    m_status->setObjectName("statusOff");

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(m_open);
    buttons->addWidget(m_close);
    buttons->addWidget(m_status);
    buttons->addStretch(1);

    QFormLayout* top = new QFormLayout();
    top->addRow(QString::fromUtf8("传输"), m_transport);
    top->addRow(QString::fromUtf8("通道名"), m_name);

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addLayout(top);
    lay->addWidget(m_shm);
    lay->addWidget(m_socket);
    lay->addLayout(buttons);

    connect(m_transport, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) { onTransport(i); });
    onTransport(0);
}

int OpenPanel::transport() const
{
    return m_transport->currentData().toInt();
}

QString OpenPanel::channelName() const
{
    return m_name->text().trimmed();
}

QString OpenPanel::summary() const
{
    return m_transport->currentText() + QString::fromUtf8("  ") + channelName();
}

QString OpenPanel::host() const
{
    return m_host->text().trimmed();
}

int OpenPanel::port() const
{
    return m_port->value();
}

int OpenPanel::capacity() const
{
    return m_capacity->value();
}

uint32_t OpenPanel::imageSlotBytes() const
{
    return (uint32_t)m_slotMb->value() * 1024u * 1024u;
}

void OpenPanel::setOpened(bool opened, const QString& error)
{
    m_transport->setEnabled(!opened);
    m_name->setEnabled(!opened);
    m_shm->setEnabled(!opened);
    m_socket->setEnabled(!opened);
    m_open->setEnabled(!opened);
    m_close->setEnabled(opened);
    if (opened) {
        m_status->setObjectName("statusOn");
        m_status->setText(QString::fromUtf8("已打开"));
    } else if (!error.isEmpty()) {
        m_status->setObjectName("statusOff");
        m_status->setText(error);
    } else {
        m_status->setObjectName("statusOff");
        m_status->setText(QString::fromUtf8("未打开"));
    }
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
}

void OpenPanel::onTransport(int)
{
    const int kind = transport();
    m_shm->setVisible(kind == HS_IPC_SHARED_MEMORY);
    m_socket->setVisible(kind == HS_IPC_SOCKET);
}
