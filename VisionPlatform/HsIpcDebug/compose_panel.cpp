/**
 * @file compose_panel.cpp
 * @brief 发送表单
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "compose_panel.h"

#include <cstring>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

class SignalForm : public MessageForm {
public:
    explicit SignalForm(QWidget* parent = 0)
        : MessageForm(parent)
    {
        m_name = new QLineEdit("station-1", this);
        m_result = new QComboBox(this);
        m_result->addItem("OK", HS_IPC_RESULT_OK);
        m_result->addItem("NG", HS_IPC_RESULT_NG);
        m_code = new QSpinBox(this);
        m_code->setRange(-1000000, 1000000);
        m_text = new QLineEdit(this);
        QFormLayout* form = new QFormLayout(this);
        form->addRow(QString::fromUtf8("名称"), m_name);
        form->addRow(QString::fromUtf8("结果"), m_result);
        form->addRow(QString::fromUtf8("代码"), m_code);
        form->addRow(QString::fromUtf8("文本"), m_text);
    }
    bool fill(OwnedMsg* msg, QString*) const override
    {
        msg->type = HS_IPC_SIGNAL;
        msg->name = m_name->text().toUtf8().constData();
        msg->result = m_result->currentData().toInt();
        msg->code = m_code->value();
        msg->text = m_text->text().toUtf8().constData();
        return true;
    }
private:
    QLineEdit* m_name;
    QComboBox* m_result;
    QSpinBox* m_code;
    QLineEdit* m_text;
};

class StatsForm : public MessageForm {
public:
    explicit StatsForm(QWidget* parent = 0)
        : MessageForm(parent)
    {
        QFormLayout* form = new QFormLayout(this);
        for (int i = 0; i < 4; ++i) {
            m_key[i] = new QLineEdit(this);
            m_value[i] = new QLineEdit(this);
            if (i == 0) {
                m_key[i]->setText("pass");
                m_value[i]->setText("0");
            }
            QHBoxLayout* row = new QHBoxLayout();
            row->addWidget(m_key[i], 1);
            row->addWidget(m_value[i], 1);
            form->addRow(QString::fromUtf8("键值"), row);
        }
    }
    bool fill(OwnedMsg* msg, QString*) const override
    {
        msg->type = HS_IPC_STATS;
        for (int i = 0; i < 4; ++i) {
            const QString key = m_key[i]->text().trimmed();
            if (key.isEmpty()) {
                continue;
            }
            HsIpcStat st;
            std::memset(&st, 0, sizeof(st));
            st.type = HS_IPC_VALUE_INT;
            st.i64 = m_value[i]->text().toLongLong();
            msg->statKeys.push_back(key.toUtf8().constData());
            msg->stats.push_back(st);
        }
        return true;
    }
private:
    QLineEdit* m_key[4];
    QLineEdit* m_value[4];
};

class ImageForm : public MessageForm {
public:
    explicit ImageForm(QWidget* parent = 0)
        : MessageForm(parent)
    {
        m_name = new QLineEdit("frame", this);
        m_path = new QLabel(QString::fromUtf8("未选择文件"), this);
        m_path->setObjectName("hint");
        QPushButton* browse = new QPushButton(QString::fromUtf8("选择图像"), this);
        connect(browse, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择图像"));
            if (!path.isEmpty()) {
                m_file = path;
                m_path->setText(path);
            }
        });
        QFormLayout* form = new QFormLayout(this);
        form->addRow(QString::fromUtf8("名称"), m_name);
        form->addRow(QString::fromUtf8("文件"), browse);
        form->addRow(QString(), m_path);
    }
    bool fill(OwnedMsg* msg, QString* err) const override
    {
        QFile file(m_file);
        if (m_file.isEmpty() || !file.open(QIODevice::ReadOnly)) {
            if (err) {
                *err = QString::fromUtf8("请选择图像文件");
            }
            return false;
        }
        const QByteArray bytes = file.readAll();
        msg->type = HS_IPC_IMAGE;
        msg->name = m_name->text().toUtf8().constData();
        msg->format = HS_IPC_IMAGE_JPEG;
        msg->data.assign(bytes.begin(), bytes.end());
        return true;
    }
private:
    QLineEdit* m_name;
    QLabel* m_path;
    QString m_file;
};

class FileForm : public MessageForm {
public:
    explicit FileForm(QWidget* parent = 0)
        : MessageForm(parent)
    {
        m_name = new QLineEdit("file", this);
        m_pathOnly = new QCheckBox(QString::fromUtf8("只发路径"), this);
        m_path = new QLabel(QString::fromUtf8("未选择文件"), this);
        m_path->setObjectName("hint");
        QPushButton* browse = new QPushButton(QString::fromUtf8("选择文件"), this);
        connect(browse, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择文件"));
            if (!path.isEmpty()) {
                m_file = path;
                m_path->setText(path);
            }
        });
        QFormLayout* form = new QFormLayout(this);
        form->addRow(QString::fromUtf8("名称"), m_name);
        form->addRow(QString(), m_pathOnly);
        form->addRow(QString::fromUtf8("文件"), browse);
        form->addRow(QString(), m_path);
    }
    bool fill(OwnedMsg* msg, QString* err) const override
    {
        if (m_file.isEmpty()) {
            if (err) {
                *err = QString::fromUtf8("请选择文件");
            }
            return false;
        }
        msg->type = HS_IPC_FILE;
        msg->name = m_name->text().toUtf8().constData();
        if (m_pathOnly->isChecked()) {
            msg->path = m_file.toUtf8().constData();
            return true;
        }
        QFile file(m_file);
        if (!file.open(QIODevice::ReadOnly)) {
            if (err) {
                *err = QString::fromUtf8("读文件失败");
            }
            return false;
        }
        const QByteArray bytes = file.readAll();
        msg->data.assign(bytes.begin(), bytes.end());
        msg->totalBytes = (uint64_t)bytes.size();
        return true;
    }
private:
    QLineEdit* m_name;
    QCheckBox* m_pathOnly;
    QLabel* m_path;
    QString m_file;
};

class AckForm : public MessageForm {
public:
    explicit AckForm(QWidget* parent = 0)
        : MessageForm(parent)
    {
        m_seq = new QSpinBox(this);
        m_seq->setRange(0, 100000000);
        m_result = new QSpinBox(this);
        m_result->setRange(-1000000, 1000000);
        QFormLayout* form = new QFormLayout(this);
        form->addRow(QString::fromUtf8("确认序号"), m_seq);
        form->addRow(QString::fromUtf8("结果"), m_result);
    }
    bool fill(OwnedMsg* msg, QString*) const override
    {
        msg->type = HS_IPC_ACK;
        msg->ackSeq = (uint32_t)m_seq->value();
        msg->ackResult = m_result->value();
        return true;
    }
private:
    QSpinBox* m_seq;
    QSpinBox* m_result;
};

class EmptyForm : public MessageForm {
public:
    explicit EmptyForm(uint16_t type, QWidget* parent = 0)
        : MessageForm(parent)
        , m_type(type)
    {
        QLabel* hint = new QLabel(QString::fromUtf8("这条报文没有正文"), this);
        hint->setObjectName("hint");
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->addWidget(hint);
        lay->addStretch(1);
    }
    bool fill(OwnedMsg* msg, QString*) const override
    {
        msg->type = m_type;
        return true;
    }
private:
    uint16_t m_type;
};

}

ComposePanel::ComposePanel(QWidget* parent)
    : QWidget(parent)
{
    m_type = new QComboBox(this);
    m_type->addItem(QString::fromUtf8("信号"), 0);
    m_type->addItem(QString::fromUtf8("统计"), 1);
    m_type->addItem(QString::fromUtf8("图像"), 2);
    m_type->addItem(QString::fromUtf8("文件"), 3);
    m_type->addItem("Hello", 4);
    m_type->addItem(QString::fromUtf8("心跳"), 5);
    m_type->addItem("Ack", 6);
    m_type->addItem("Bye", 7);

    m_signal = new SignalForm(this);
    m_stats = new StatsForm(this);
    m_image = new ImageForm(this);
    m_file = new FileForm(this);
    m_ack = new AckForm(this);
    m_empty = new EmptyForm(HS_IPC_HELLO, this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_signal);
    m_stack->addWidget(m_stats);
    m_stack->addWidget(m_image);
    m_stack->addWidget(m_file);
    m_stack->addWidget(m_empty);
    m_stack->addWidget(new EmptyForm(HS_IPC_HEARTBEAT, this));
    m_stack->addWidget(m_ack);
    m_stack->addWidget(new EmptyForm(HS_IPC_BYE, this));

    m_send = new QPushButton(QString::fromUtf8("发送"), this);
    m_send->setObjectName("primary");

    QFormLayout* typeRow = new QFormLayout();
    typeRow->addRow(QString::fromUtf8("消息类型"), m_type);
    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addLayout(typeRow);
    lay->addWidget(m_stack, 1);
    lay->addWidget(m_send, 0, Qt::AlignLeft);

    connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) { onType(i); });
}

bool ComposePanel::fill(OwnedMsg* msg, QString* err) const
{
    MessageForm* form = static_cast<MessageForm*>(m_stack->currentWidget());
    return form && form->fill(msg, err);
}

void ComposePanel::onType(int index)
{
    m_stack->setCurrentIndex(index);
}
