/**
 * @file LicenseGate.cpp
 * @brief 启动对话框和运行中的倒计时。倒计时这版固定 10 分钟
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
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

const int kPollMs = 30000;          ///< 运行中复查许可的间隔，毫秒
const int kCountdownSec = 10 * 60;  ///< 许可失效后到停工站的倒计时，秒

/** @brief std::string（UTF-8）转 QString */
QString utf8(const std::string& text)
{
    return QString::fromUtf8(text.c_str());
}

/** @brief 挂在主窗口上：定时复查许可，失效时弹倒计时框，到点排队调用 MainWindow::stopStation */
class LicenseWatch : public QObject {
public:
    /** @brief 启动 30 秒复查定时器和 1 秒倒计时定时器，随主窗口销毁 */
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

    /** @brief 倒计时框无父对象，随本对象一起释放 */
    ~LicenseWatch()
    {
        delete m_dlg;
    }

private:
    /**
     * @brief 复查许可：恢复则关掉倒计时框；失效且尚未弹框时从 10 分钟开始倒计时
     * @note 用户点 X 只是隐藏了倒计时框，倒计时照走；这里把同一个框重新显示，不重置剩余时间
     */
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
            return;
        }
        if (!m_dlg->isVisible()) {
            m_dlg->show();
            m_dlg->raise();
            m_dlg->activateWindow();
        }
    }

    /** @brief 每秒刷新剩余时间；归零时停工站，只停一次 */
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
        // 到点只停工站，不退出进程，留着窗口让用户补注册码
        if (m_left == 0) {
            m_stopped = true;
            QMetaObject::invokeMethod(m_window, "stopStation", Qt::QueuedConnection);
            if (m_status) {
                m_status->setText(QString::fromUtf8("工站已停止。主窗口还在，可以粘贴新的注册码。"));
            }
        }
    }

    /** @brief 弹出置顶的非模态倒计时框：原因、剩余时间、申请码、注册码输入 */
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

    /** @brief 验粘贴的注册码，通过则关框，失败显示原因 */
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

    /** @brief 关闭并延迟释放倒计时框，清空控件指针 */
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

    MainWindow* m_window;      ///< 主窗口，也是本对象的父对象
    int m_left;                ///< 倒计时剩余秒数
    bool m_stopped;            ///< 本轮倒计时已到点并停了工站
    QDialog* m_dlg;            ///< 倒计时框，无父对象，未弹出时为空
    QLabel* m_remain;          ///< 剩余时间标签，属于 m_dlg
    QPlainTextEdit* m_paste;   ///< 注册码输入框，属于 m_dlg
    QLabel* m_status;          ///< 结果提示，属于 m_dlg
};

}

bool promptLicense(SoftLicense* license, LicenseReason reason)
{
    if (!license) {
        return false;
    }
    // 反复弹框直到许可有效；注册成功后再按启动流程 open 一次确认
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
