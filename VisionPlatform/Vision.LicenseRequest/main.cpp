/**
 * @file main.cpp
 * @brief 客户电脑上的申请和注册。打出申请码，验过注册码后写入许可证
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LicenseCodeUi.h"
#include "LicenseUiTheme.h"
#include "SoftLicense.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace {

const int kMaxRegisterPerMinute = 50;          ///< 滑动窗口内允许的注册次数
const qint64 kRegisterWindowMs = 60 * 1000;    ///< 限流滑动窗口，毫秒

/** @brief 一分钟内最多 50 次注册。超出后禁用按钮，到点再放开 */
class RegisterPace : public QObject {
public:
    /** @brief 记下按钮初始是否可用；初始禁用的按钮到点也不放开 */
    RegisterPace(QPushButton* button, QLabel* status, QWidget* parent)
        : QObject(parent)
        , m_button(button)
        , m_status(status)
        , m_open(button->isEnabled())
        , m_timer(new QTimer(this))
    {
        m_timer->setSingleShot(true);
        QObject::connect(m_timer, &QTimer::timeout, this, [this]() { release(); });
    }

    /** @brief 本次点击是否放行；放行则记下时间，超限则禁用按钮并弹窗 */
    bool allow()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        dropExpired(now);
        // 窗口内次数已满：禁用按钮，等最早一次滑出窗口再放开
        if (m_at.size() >= kMaxRegisterPerMinute) {
            hold(now);
            QMessageBox::warning(window(), QString::fromUtf8("注册"),
                QString::fromUtf8("一分钟内注册不能超过 50 次，请稍后再试。"));
            return false;
        }
        m_at.append(now);
        return true;
    }

private:
    /** @brief 丢掉滑出窗口的记录 */
    void dropExpired(qint64 now)
    {
        while (!m_at.isEmpty() && now - m_at.first() >= kRegisterWindowMs) {
            m_at.removeFirst();
        }
    }

    /** @brief 禁用按钮，定时到最早一次记录滑出窗口 */
    void hold(qint64 now)
    {
        m_button->setEnabled(false);
        m_status->setText(QString::fromUtf8("一分钟内注册不能超过 50 次，请稍后再试。"));
        qint64 wait = kRegisterWindowMs;
        if (!m_at.isEmpty()) {
            wait = kRegisterWindowMs - (now - m_at.first());
        }
        if (wait < 1) {
            wait = 1;
        }
        m_timer->start(static_cast<int>(wait));
    }

    /** @brief 定时到点：仍超限就继续等，否则恢复按钮 */
    void release()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        dropExpired(now);
        if (m_at.size() >= kMaxRegisterPerMinute) {
            hold(now);
            return;
        }
        if (!m_open) {
            return;
        }
        m_button->setEnabled(true);
        m_status->setText(QString::fromUtf8("可以继续注册。"));
    }

    /** @brief 弹窗用的父窗口 */
    QWidget* window() const
    {
        return qobject_cast<QWidget*>(parent());
    }

    QPushButton* m_button;   ///< 注册按钮，不持有
    QLabel* m_status;        ///< 状态提示，不持有
    bool m_open;             ///< 构造时按钮可用；为 false 时到点也不放开
    QTimer* m_timer;         ///< 单次定时，到点调用 release
    QVector<qint64> m_at;    ///< 窗口内每次放行的时间戳（毫秒），按时间先后
};

/** @brief 换状态胶囊的样式名和文字，并刷新样式 */
void setPill(QLabel* pill, const char* name, const QString& text)
{
    pill->setObjectName(QString::fromLatin1(name));
    pill->setText(text);
    polishWidget(pill);
}

/** @brief 按校验结果刷新状态胶囊、许可信息行和提示文字 */
void showLicense(QLabel* pill, QWidget* facts, QLabel* idValue, QLabel* kindValue, QLabel* serialValue,
                 QLabel* note, QLabel* status, const Vision::SoftLicense& license, Vision::LicenseReason reason,
                 bool hardwareOk)
{
    if (!hardwareOk) {
        setPill(pill, "pillIdle", QString::fromUtf8("未注册"));
        facts->setVisible(false);
        note->setText(QString());
        status->setText(QString::fromUtf8("读不到足够的硬件信息，发不出申请码，也不能注册。"));
        return;
    }
    note->setText(QString::fromUtf8("申请码里带有当前序号。序号变了，申请码也会变。"));
    if (reason == Vision::LicenseOk) {
        const Vision::LicenseInfo info = license.describe();
        setPill(pill, "pillOk", QString::fromUtf8("已注册"));
        idValue->setText(QString::fromUtf8(info.licenseId.c_str()));
        if (info.kind == Vision::LicenseUntil) {
            kindValue->setText(QString::fromUtf8("到期 ") + QString::fromUtf8(info.until.c_str()));
        } else {
            kindValue->setText(QString::fromUtf8("永久注册"));
        }
        serialValue->setText(QString::number(info.serial));
        facts->setVisible(true);
        status->setText(QString::fromUtf8("这台电脑已经注册。换新注册码时序号必须更大。"));
        return;
    }
    facts->setVisible(false);
    if (reason == Vision::LicenseExpired) {
        setPill(pill, "pillBad", QString::fromUtf8("已过期"));
    } else {
        setPill(pill, "pillIdle", QString::fromUtf8("未注册"));
    }
    status->setText(QString::fromUtf8(license.reasonText(reason).c_str()));
}

}

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Vision.LicenseRequest"));
    applyLicenseUiTheme(&app);

    QWidget window;
    window.setObjectName(QStringLiteral("licenseWindow"));
    window.setWindowTitle(QString::fromUtf8("注册"));
    window.resize(560, 480);

    // 控件：标题与状态、许可信息行、申请码区、注册码区
    QLabel* product = new QLabel(QStringLiteral("VisionPlatform"));
    product->setObjectName(QStringLiteral("product"));
    QLabel* pill = new QLabel();
    QWidget* facts = new QWidget();
    QLabel* idValue = new QLabel();
    QLabel* kindValue = new QLabel();
    QLabel* serialValue = new QLabel();
    QLabel* idKey = new QLabel(QString::fromUtf8("编号"));
    QLabel* kindKey = new QLabel(QString::fromUtf8("类型"));
    QLabel* serialKey = new QLabel(QString::fromUtf8("序号"));
    idKey->setObjectName(QStringLiteral("keyName"));
    kindKey->setObjectName(QStringLiteral("keyName"));
    serialKey->setObjectName(QStringLiteral("keyName"));

    QLabel* requestTitle = new QLabel(QString::fromUtf8("1  申请码"));
    requestTitle->setObjectName(QStringLiteral("section"));
    QPushButton* copyBtn = new QPushButton(QString::fromUtf8("复制"));
    QPushButton* exportBtn = new QPushButton(QString::fromUtf8("导出"));
    QPushButton* qrBtn = new QPushButton(QString::fromUtf8("二维码"));
    QPlainTextEdit* code = new QPlainTextEdit();
    code->setObjectName(QStringLiteral("codeBox"));
    code->setReadOnly(true);
    QLabel* note = new QLabel();
    note->setObjectName(QStringLiteral("hint"));
    note->setWordWrap(true);

    QLabel* regTitle = new QLabel(QString::fromUtf8("2  注册码"));
    regTitle->setObjectName(QStringLiteral("section"));
    QPushButton* importBtn = new QPushButton(QString::fromUtf8("导入"));
    QPlainTextEdit* paste = new QPlainTextEdit();
    paste->setObjectName(QStringLiteral("codeBox"));
    paste->setPlaceholderText(QString::fromUtf8("粘贴厂家发回的注册码"));
    QLabel* status = new QLabel();
    status->setWordWrap(true);
    QPushButton* applyBtn = new QPushButton(QString::fromUtf8("注册"));
    applyBtn->setProperty("primary", true);

    // 现算申请码；硬件信息不够时禁用所有操作，只显示原因
    Vision::SoftLicense license;
    const std::string request = license.requestCode();
    const bool hardwareOk = !request.empty();
    if (hardwareOk) {
        code->setPlainText(QString::fromUtf8(request.c_str()));
    } else {
        copyBtn->setEnabled(false);
        exportBtn->setEnabled(false);
        qrBtn->setEnabled(false);
        applyBtn->setEnabled(false);
    }
    const Vision::LicenseReason opened = hardwareOk ? license.open() : Vision::LicenseMissing;
    showLicense(pill, facts, idValue, kindValue, serialValue, note, status, license, opened, hardwareOk);
    polishWidget(applyBtn);

    // 申请码的复制、导出、二维码，注册码导入
    QObject::connect(copyBtn, &QPushButton::clicked, &window, [code, status]() {
        QApplication::clipboard()->setText(code->toPlainText());
        status->setText(QString::fromUtf8("已复制申请码。"));
    });
    QObject::connect(exportBtn, &QPushButton::clicked, &window, [&window, code, status]() {
        exportLicenseCode(&window, licenseCodeText(code), status, QString::fromUtf8("申请码"));
    });
    QObject::connect(qrBtn, &QPushButton::clicked, &window, [&window, code, status]() {
        showLicenseQr(&window, licenseCodeText(code), QString::fromUtf8("申请码"), status);
    });
    QObject::connect(importBtn, &QPushButton::clicked, &window, [&window, paste, status]() {
        importLicenseCode(&window, paste, status, QString::fromUtf8("注册码"));
    });
    // 注册：先限流，再验注册码并落地许可证
    RegisterPace* pace = new RegisterPace(applyBtn, status, &window);
    QObject::connect(applyBtn, &QPushButton::clicked, &window,
                     [paste, status, pace, pill, facts, idValue, kindValue, serialValue, note]() {
        if (!pace->allow()) {
            return;
        }
        Vision::SoftLicense license;
        const Vision::LicenseReason reason = license.accept(paste->toPlainText().toUtf8().constData());
        if (reason == Vision::LicenseOk) {
            showLicense(pill, facts, idValue, kindValue, serialValue, note, status, license, reason, true);
            status->setText(QString::fromUtf8("注册完成。许可证已写入，时间记录已更新。"));
            return;
        }
        status->setText(QString::fromUtf8(license.reasonText(reason).c_str()));
    });

    // 布局
    QHBoxLayout* head = new QHBoxLayout();
    head->addWidget(product);
    head->addStretch(1);
    head->addWidget(pill);
    QHBoxLayout* factRow = new QHBoxLayout(facts);
    factRow->setContentsMargins(0, 0, 0, 0);
    factRow->addWidget(idKey);
    factRow->addWidget(idValue);
    factRow->addSpacing(12);
    factRow->addWidget(kindKey);
    factRow->addWidget(kindValue);
    factRow->addSpacing(12);
    factRow->addWidget(serialKey);
    factRow->addWidget(serialValue);
    factRow->addStretch(1);
    QHBoxLayout* requestHead = new QHBoxLayout();
    requestHead->addWidget(requestTitle);
    requestHead->addStretch(1);
    requestHead->addWidget(copyBtn);
    requestHead->addWidget(exportBtn);
    requestHead->addWidget(qrBtn);
    QHBoxLayout* regHead = new QHBoxLayout();
    regHead->addWidget(regTitle);
    regHead->addStretch(1);
    regHead->addWidget(importBtn);
    QHBoxLayout* foot = new QHBoxLayout();
    foot->addWidget(status, 1);
    foot->addWidget(applyBtn);

    QVBoxLayout* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(16, 14, 16, 12);
    layout->setSpacing(12);
    layout->addLayout(head);
    layout->addWidget(facts);
    layout->addLayout(requestHead);
    layout->addWidget(code, 1);
    layout->addWidget(note);
    layout->addLayout(regHead);
    layout->addWidget(paste, 1);
    layout->addLayout(foot);
    applyLicenseWindowIcon(&window);
    window.show();
    return app.exec();
}
