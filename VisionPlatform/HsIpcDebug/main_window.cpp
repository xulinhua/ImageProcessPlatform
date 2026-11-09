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

#include <QAction>
#include <QCoreApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
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
#include <QApplication>
#include <QIcon>
#include <QImage>

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
    , m_sawOpen(false)
{
    useExeIcon();
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

    QFrame* logBox = new QFrame(this);
    logBox->setObjectName("panel");
    QVBoxLayout* logLay = new QVBoxLayout(logBox);
    logLay->addWidget(new QLabel(QString::fromUtf8("日志"), logBox));
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
    m_state = new QLabel(QString::fromUtf8("未打开"), this);
    m_state->setObjectName("statusOff");
    m_where = new QLabel(this);
    statusBar()->addWidget(m_state);
    statusBar()->addWidget(m_where, 1);
    statusBar()->addPermanentWidget(m_idle);
    statusBar()->showMessage(QString::fromUtf8("选择传输和通道名后打开"));
    buildMenu();

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
    note(QString::fromUtf8("正在打开 ") + m_open->summary());
    statusBar()->showMessage(QString::fromUtf8("正在打开"));
}

void MainWindow::onClose()
{
    m_worker->requestClose();
    note(QString::fromUtf8("正在关闭"));
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
    m_where->setText(m_open->summary());
    m_state->setObjectName(opened ? "statusOn" : "statusOff");
    m_state->setText(opened ? QString::fromUtf8("已打开") : QString::fromUtf8("未打开"));
    m_state->style()->unpolish(m_state);
    m_state->style()->polish(m_state);
    if (m_sawOpen && !opened && !error.isEmpty()) {
        m_open->setOpened(false, error);
        note(QString::fromUtf8("打开失败  ") + error);
        statusBar()->showMessage(error);
        m_sawOpen = false;
    } else if (opened) {
        if (m_sawOpen) {
            note(QString::fromUtf8("已打开"));
        }
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

void MainWindow::note(const QString& summary)
{
    const int at = m_log->rowCount();
    m_log->insertRow(at);
    m_log->setItem(at, 0, new QTableWidgetItem(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")));
    m_log->setItem(at, 1, new QTableWidgetItem(QString::fromUtf8("日志")));
    m_log->setItem(at, 2, new QTableWidgetItem(QString()));
    m_log->setItem(at, 3, new QTableWidgetItem(QString::fromUtf8("日志")));
    m_log->setItem(at, 4, new QTableWidgetItem(summary));
    m_log->scrollToBottom();
}

void MainWindow::buildMenu()
{
    QMenu* file = menuBar()->addMenu(QString::fromUtf8("文件"));
    file->addAction(QString::fromUtf8("保存记录"), this, [this]() { saveLog(); });
    file->addSeparator();
    file->addAction(QString::fromUtf8("退出"), this, [this]() { close(); });

    QMenu* edit = menuBar()->addMenu(QString::fromUtf8("编辑"));
    edit->addAction(QString::fromUtf8("清空记录"), this, [this]() {
        m_log->clearContents();
        m_log->setRowCount(0);
    });

    QMenu* tool = menuBar()->addMenu(QString::fromUtf8("工具"));
    QAction* beat = tool->addAction(QString::fromUtf8("定时心跳"));
    beat->setCheckable(true);
    connect(beat, &QAction::toggled, m_beatOn, &QCheckBox::setChecked);
    connect(m_beatOn, &QCheckBox::toggled, beat, &QAction::setChecked);
    tool->addAction(QString::fromUtf8("再开一个"), this, [this]() {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList());
    });

    QMenu* help = menuBar()->addMenu(QString::fromUtf8("帮助"));
    help->addAction(QString::fromUtf8("关于"), this, [this]() {
        QMessageBox::about(this, QString::fromUtf8("关于"),
            QString::fromUtf8("HsIpc 调试器\n用来对共享内存、管道、消息队列和套接字。不改协议。"));
    });
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
