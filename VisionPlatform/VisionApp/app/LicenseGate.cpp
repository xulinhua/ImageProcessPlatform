/**
 * @file LicenseGate.cpp
 * @brief 启动对话框和运行中的倒计时。倒计时这版固定 10 分钟
 */

#include "LicenseGate.h"
#include "MainWindow.h"
#include "SoftLicense.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QDialog>

namespace Vision {

namespace {

const int kPollMs = 30000;
const int kCountdownSec = 10 * 60;

QString utf8(const std::string& text)
{
    return QString::fromUtf8(text.c_str());
}

class LicenseWatch : public QObject {
public:
    explicit LicenseWatch(MainWindow* window)
        : QObject(window)
        , m_window(window)
        , m_left(0)
        , m_stopped(false)
        , m_dlg(0)
        , m_remain(0)
        , m_paste(0)
        , m_status(0)
    {
        QTimer* poll = new QTimer(this);
        connect(poll, &QTimer::timeout, this, [this]() { onPoll(); });
        poll->start(kPollMs);
        QTimer* tick = new QTimer(this);
        connect(tick, &QTimer::timeout, this, [this]() { onTick(); });
        tick->start(1000);
    }

private:
    void onPoll()
    {
        SoftLicense license;
        const LicenseReason reason = license.poll();
        if (reason == LicenseOk) {
            closeCountdown();
            return;
        }
        if (!m_dlg) {
            m_left = kCountdownSec;
            m_stopped = false;
            showCountdown(license, reason);
        }
    }

    void onTick()
    {
        if (!m_dlg || m_stopped) {
            return;
        }
        if (m_left > 0) {
            --m_left;
        }
        if (m_remain) {
            const int min = m_left / 60;
            const int sec = m_left % 60;
            m_remain->setText(QString::fromUtf8("剩余 %1:%2")
                                  .arg(min, 2, 10, QLatin1Char('0'))
                                  .arg(sec, 2, 10, QLatin1Char('0')));
        }
        if (m_left == 0) {
            m_stopped = true;
            QMetaObject::invokeMethod(m_window, "stopStation", Qt::QueuedConnection);
            if (m_status) {
                m_status->setText(QString::fromUtf8("工站已停止。主窗口还在，可以粘贴新的注册码。"));
            }
        }
    }

    void showCountdown(SoftLicense& license, LicenseReason reason)
    {
        m_dlg = new QDialog();
        m_dlg->setWindowTitle(QString::fromUtf8("许可"));
        m_dlg->setWindowFlags(m_dlg->windowFlags() | Qt::WindowStaysOnTopHint);
        m_dlg->resize(520, 360);
        QLabel* why = new QLabel(utf8(license.reasonText(reason)), m_dlg);
        why->setWordWrap(true);
        m_remain = new QLabel(m_dlg);
        QPlainTextEdit* request = new QPlainTextEdit(utf8(license.requestCode()), m_dlg);
        request->setReadOnly(true);
        m_paste = new QPlainTextEdit(m_dlg);
        m_paste->setPlaceholderText(QString::fromUtf8("粘贴注册码"));
        m_status = new QLabel(m_dlg);
        m_status->setWordWrap(true);
        QPushButton* apply = new QPushButton(QString::fromUtf8("注册"), m_dlg);
        connect(apply, &QPushButton::clicked, m_dlg, [this]() { applyCode(); });
        QVBoxLayout* layout = new QVBoxLayout(m_dlg);
        layout->addWidget(why);
        layout->addWidget(m_remain);
        layout->addWidget(request);
        layout->addWidget(m_paste);
        layout->addWidget(m_status);
        layout->addWidget(apply);
        m_remain->setText(QString::fromUtf8("剩余 10:00"));
        m_dlg->show();
    }

    void applyCode()
    {
        if (!m_paste) {
            return;
        }
        SoftLicense license;
        const LicenseReason reason = license.accept(m_paste->toPlainText().toUtf8().constData());
        if (reason == LicenseOk) {
            closeCountdown();
            return;
        }
        if (m_status) {
            m_status->setText(utf8(license.reasonText(reason)));
        }
    }

    void closeCountdown()
    {
        if (m_dlg) {
            m_dlg->close();
            m_dlg->deleteLater();
            m_dlg = 0;
        }
        m_remain = 0;
        m_paste = 0;
        m_status = 0;
        m_stopped = false;
    }

    MainWindow* m_window;
    int m_left;
    bool m_stopped;
    QDialog* m_dlg;
    QLabel* m_remain;
    QPlainTextEdit* m_paste;
    QLabel* m_status;
};

}

bool promptLicense(SoftLicense* license, LicenseReason reason)
{
    if (!license) {
        return false;
    }
    while (reason != LicenseOk) {
        QDialog dlg;
        dlg.setWindowTitle(QString::fromUtf8("许可"));
        dlg.resize(520, 380);
        QLabel* why = new QLabel(utf8(license->reasonText(reason)), &dlg);
        why->setWordWrap(true);
        QPlainTextEdit* request = new QPlainTextEdit(utf8(license->requestCode()), &dlg);
        request->setReadOnly(true);
        QPlainTextEdit* paste = new QPlainTextEdit(&dlg);
        paste->setPlaceholderText(QString::fromUtf8("粘贴注册码"));
        QLabel* status = new QLabel(&dlg);
        status->setWordWrap(true);
        QPushButton* apply = new QPushButton(QString::fromUtf8("注册"), &dlg);
        QPushButton* quit = new QPushButton(QString::fromUtf8("退出"), &dlg);
        QObject::connect(quit, &QPushButton::clicked, &dlg, &QDialog::reject);
        QObject::connect(apply, &QPushButton::clicked, &dlg, [&]() {
            const LicenseReason got = license->accept(paste->toPlainText().toUtf8().constData());
            if (got == LicenseOk) {
                dlg.accept();
                return;
            }
            status->setText(utf8(license->reasonText(got)));
        });
        QHBoxLayout* buttons = new QHBoxLayout();
        buttons->addStretch(1);
        buttons->addWidget(apply);
        buttons->addWidget(quit);
        QVBoxLayout* layout = new QVBoxLayout(&dlg);
        layout->addWidget(why);
        layout->addWidget(request);
        layout->addWidget(paste);
        layout->addWidget(status);
        layout->addLayout(buttons);
        if (dlg.exec() != QDialog::Accepted) {
            return false;
        }
        reason = license->open();
    }
    return true;
}

void watchLicense(QApplication* app, MainWindow* window)
{
    if (!app || !window) {
        return;
    }
    new LicenseWatch(window);
    Q_UNUSED(app);
}

}
