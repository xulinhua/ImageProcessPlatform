/**
 * @file main_window.cpp
 * @brief 主窗口布局
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "main_window.h"
#include "compose_panel.h"
#include "open_panel.h"
#include "session_worker.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTextStream>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_sawOpen(false)
{
    setWindowTitle(QString::fromUtf8("HsIpc 调试器"));

    m_open = new OpenPanel(this);
    m_compose = new ComposePanel(this);
    m_worker = new SessionWorker();
    m_worker->start();

    QFrame* left = new QFrame(this);
    left->setObjectName("panel");
    QVBoxLayout* leftLay = new QVBoxLayout(left);
    leftLay->addWidget(m_open);
    leftLay->addWidget(m_compose, 1);

    m_log = new QTableWidget(0, 5, this);
    m_log->setHorizontalHeaderLabels(QStringList()
        << QString::fromUtf8("时间")
        << QString::fromUtf8("方向")
        << QString::fromUtf8("序号")
        << QString::fromUtf8("类型")
        << QString::fromUtf8("摘要"));
    m_log->horizontalHeader()->setStretchLastSection(true);
    m_log->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_log->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_log->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_log->verticalHeader()->setVisible(false);

    m_preview = new QLabel(QString::fromUtf8("还没收到图像"), this);
    m_preview->setObjectName("hint");
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(280, 220);
    m_preview->setWordWrap(true);
    QFrame* previewBox = new QFrame(this);
    previewBox->setObjectName("panel");
    QVBoxLayout* previewLay = new QVBoxLayout(previewBox);
    QLabel* title = new QLabel(QString::fromUtf8("图像预览"), previewBox);
    previewLay->addWidget(title);
    previewLay->addWidget(m_preview, 1);

    QPushButton* save = new QPushButton(QString::fromUtf8("保存记录"), this);
    QPushButton* clear = new QPushButton(QString::fromUtf8("清空"), this);
    QPushButton* another = new QPushButton(QString::fromUtf8("再开一个"), this);
    m_beatOn = new QCheckBox(QString::fromUtf8("定时心跳"), this);
    m_beatMs = new QSpinBox(this);
    m_beatMs->setRange(200, 60000);
    m_beatMs->setValue(1000);
    m_beatMs->setSuffix(" ms");
    m_idle = new QLabel(QString::fromUtf8("还没收到"), this);
    m_idle->setObjectName("hint");
    QHBoxLayout* logBar = new QHBoxLayout();
    logBar->addWidget(save);
    logBar->addWidget(clear);
    logBar->addWidget(another);
    logBar->addWidget(m_beatOn);
    logBar->addWidget(m_beatMs);
    logBar->addStretch(1);
    logBar->addWidget(m_idle);

    QFrame* logBox = new QFrame(this);
    logBox->setObjectName("panel");
    QVBoxLayout* logLay = new QVBoxLayout(logBox);
    logLay->addLayout(logBar);
    logLay->addWidget(m_log);

    QSplitter* right = new QSplitter(Qt::Vertical, this);
    right->addWidget(previewBox);
    right->addWidget(logBox);
    right->setStretchFactor(1, 1);

    QSplitter* root = new QSplitter(Qt::Horizontal, this);
    root->addWidget(left);
    root->addWidget(right);
    root->setStretchFactor(1, 1);
    setCentralWidget(root);
    statusBar()->showMessage(QString::fromUtf8("选择传输和通道名后打开"));

    connect(m_open->openButton(), &QPushButton::clicked, this, [this]() { onOpen(); });
    connect(m_open->closeButton(), &QPushButton::clicked, this, [this]() { onClose(); });
    connect(m_compose->sendButton(), &QPushButton::clicked, this, [this]() { onSend(); });
    connect(m_log, &QTableWidget::cellClicked, this, [this](int row, int) {
        QTableWidgetItem* item = m_log->item(row, 0);
        if (!item) {
            return;
        }
        const QByteArray image = item->data(Qt::UserRole).toByteArray();
        showImage(image, item->data(Qt::UserRole + 1).toBool());
    });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() { drain(); });
    m_timer->start(40);
    m_heartbeat = new QTimer(this);
    connect(m_heartbeat, &QTimer::timeout, this, [this]() { sendHeartbeat(); });
    connect(m_beatOn, &QCheckBox::toggled, this, [this](bool on) {
        if (on) {
            m_heartbeat->start(m_beatMs->value());
        } else {
            m_heartbeat->stop();
        }
    });
    connect(m_beatMs, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_beatOn->isChecked()) {
            m_heartbeat->start(v);
        }
    });
    connect(save, &QPushButton::clicked, this, [this]() { saveLog(); });
    connect(clear, &QPushButton::clicked, m_log, &QTableWidget::clearContents);
    connect(clear, &QPushButton::clicked, this, [this]() { m_log->setRowCount(0); });
    connect(another, &QPushButton::clicked, this, [this]() {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList());
    });
    resize(1080, 700);
}

MainWindow::~MainWindow()
{
    m_heartbeat->stop();
    m_timer->stop();
    m_worker->requestClose();
    delete m_worker;
}

void MainWindow::onOpen()
{
    const QString name = m_open->channelName();
    if (name.size() < 2) {
        statusBar()->showMessage(QString::fromUtf8("通道名至少两个字符，只能含字母、数字、点、下划线和减号"));
        return;
    }
    m_sawOpen = true;
    m_worker->requestOpen(m_open->transport(), name, m_open->host(), m_open->port(),
                          m_open->capacity(), m_open->imageSlotBytes());
    statusBar()->showMessage(QString::fromUtf8("正在打开"));
}

void MainWindow::onClose()
{
    m_worker->requestClose();
}

void MainWindow::onSend()
{
    OwnedMsg msg;
    QString err;
    if (!m_compose->fill(&msg, &err)) {
        statusBar()->showMessage(err);
        return;
    }
    m_worker->requestSend(msg);
}

void MainWindow::drain()
{
    const int idle = m_worker->idleMs();
    m_idle->setText(idle < 0 ? QString::fromUtf8("还没收到")
                             : QString::fromUtf8("已 %1 ms 未收到").arg(idle));

    const bool opened = m_worker->opened();
    const QString error = m_worker->lastError();
    if (m_sawOpen && !opened && !error.isEmpty()) {
        m_open->setOpened(false, error);
        statusBar()->showMessage(error);
        m_sawOpen = false;
    } else if (opened) {
        m_open->setOpened(true, QString());
        m_sawOpen = false;
    } else if (!m_sawOpen) {
        m_open->setOpened(false, QString());
    }

    LogRow row;
    while (m_worker->takeRow(&row)) {
        const int at = m_log->rowCount();
        m_log->insertRow(at);
        QTableWidgetItem* time = new QTableWidgetItem(row.time);
        time->setData(Qt::UserRole, row.image);
        time->setData(Qt::UserRole + 1, row.jpeg);
        m_log->setItem(at, 0, time);
        m_log->setItem(at, 1, new QTableWidgetItem(row.dir));
        m_log->setItem(at, 2, new QTableWidgetItem(row.seq));
        m_log->setItem(at, 3, new QTableWidgetItem(row.type));
        m_log->setItem(at, 4, new QTableWidgetItem(row.summary));
        m_log->scrollToBottom();
        if (!row.image.isEmpty()) {
            showImage(row.image, row.jpeg);
        }
        while (m_log->rowCount() > 500) {
            m_log->removeRow(0);
        }
    }
}

void MainWindow::saveLog()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("保存记录"), QString(),
                                                      QString::fromUtf8("文本 (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage(QString::fromUtf8("保存失败"));
        return;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    for (int col = 0; col < m_log->columnCount(); ++col) {
        if (col) {
            out << '\t';
        }
        out << m_log->horizontalHeaderItem(col)->text();
    }
    out << '\n';
    for (int row = 0; row < m_log->rowCount(); ++row) {
        for (int col = 0; col < m_log->columnCount(); ++col) {
            if (col) {
                out << '\t';
            }
            QTableWidgetItem* item = m_log->item(row, col);
            out << (item ? item->text() : QString());
        }
        out << '\n';
    }
    statusBar()->showMessage(path);
}

void MainWindow::sendHeartbeat()
{
    OwnedMsg msg;
    msg.type = HS_IPC_HEARTBEAT;
    m_worker->requestSend(msg);
}

void MainWindow::showImage(const QByteArray& bytes, bool jpeg)
{
    if (bytes.isEmpty()) {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(QString::fromUtf8("还没收到图像"));
        return;
    }
    QPixmap pix;
    if (jpeg && pix.loadFromData(bytes, "JPEG")) {
        m_preview->setPixmap(pix.scaled(m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_preview->setText(QString());
        return;
    }
    m_preview->setPixmap(QPixmap());
    m_preview->setText(QString::fromUtf8("图像 %1 字节，当前只预览 JPEG").arg(bytes.size()));
}
