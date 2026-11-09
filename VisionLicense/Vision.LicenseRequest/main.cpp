/**
 * @file main.cpp
 * @brief 客户电脑上的申请和注册。打出申请码，验过注册码后写入许可证
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "code_ui.h"
#include "theme.h"
#include "VisionLicenseTool.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
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

/** @brief 客户机窗口：本机状态、申请码、注册码 */
class RequestWindow : public QWidget {
public:
    RequestWindow()
        : QWidget()
        , m_hardwareOk(false)
        , m_pill(0)
        , m_idValue(0)
        , m_kindValue(0)
        , m_serialValue(0)
        , m_code(0)
        , m_note(0)
        , m_paste(0)
        , m_status(0)
        , m_apply(0)
        , m_copy(0)
        , m_export(0)
        , m_qr(0)
        , m_pace(0)
    {
        setObjectName(QStringLiteral("licenseWindow"));
        setWindowTitle(QString::fromUtf8("VisionPlatform 许可"));
        resize(640, 560);

        QLabel* product = new QLabel(QStringLiteral("VisionPlatform"), this);
        product->setObjectName(QStringLiteral("product"));
        m_pill = new QLabel(this);
        QLabel* lead = new QLabel(QString::fromUtf8(
            "先把申请码发给厂家。厂家发回注册码后粘贴在下面，验过才写入这台电脑。"), this);
        lead->setObjectName(QStringLiteral("lead"));
        lead->setWordWrap(true);

        QFrame* card = new QFrame(this);
        card->setObjectName(QStringLiteral("card"));
        m_idValue = new QLabel(QString::fromUtf8("—"), card);
        m_kindValue = new QLabel(QString::fromUtf8("—"), card);
        m_serialValue = new QLabel(QString::fromUtf8("—"), card);
        QLabel* idKey = new QLabel(QString::fromUtf8("编号"), card);
        QLabel* kindKey = new QLabel(QString::fromUtf8("期限"), card);
        QLabel* serialKey = new QLabel(QString::fromUtf8("序号"), card);
        idKey->setObjectName(QStringLiteral("keyName"));
        kindKey->setObjectName(QStringLiteral("keyName"));
        serialKey->setObjectName(QStringLiteral("keyName"));
        QHBoxLayout* factRow = new QHBoxLayout(card);
        factRow->setContentsMargins(12, 8, 12, 8);
        factRow->addWidget(idKey);
        factRow->addWidget(m_idValue);
        factRow->addSpacing(16);
        factRow->addWidget(kindKey);
        factRow->addWidget(m_kindValue);
        factRow->addSpacing(16);
        factRow->addWidget(serialKey);
        factRow->addWidget(m_serialValue);
        factRow->addStretch(1);

        QLabel* requestTitle = new QLabel(QString::fromUtf8("1  申请码"), this);
        requestTitle->setObjectName(QStringLiteral("section"));
        m_copy = new QPushButton(QString::fromUtf8("复制"), this);
        m_export = new QPushButton(QString::fromUtf8("导出"), this);
        m_qr = new QPushButton(QString::fromUtf8("二维码"), this);
        m_code = new QPlainTextEdit(this);
        m_code->setObjectName(QStringLiteral("codeBox"));
        m_code->setReadOnly(true);
        m_code->setPlaceholderText(QString::fromUtf8("申请码会显示在这里"));
        m_note = new QLabel(this);
        m_note->setObjectName(QStringLiteral("hint"));
        m_note->setWordWrap(true);

        QLabel* regTitle = new QLabel(QString::fromUtf8("2  注册码"), this);
        regTitle->setObjectName(QStringLiteral("section"));
        QPushButton* importBtn = new QPushButton(QString::fromUtf8("导入"), this);
        m_paste = new QPlainTextEdit(this);
        m_paste->setObjectName(QStringLiteral("codeBox"));
        m_paste->setPlaceholderText(QString::fromUtf8("粘贴厂家发回的注册码，前缀是 VP-R1-"));
        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        m_apply = new QPushButton(QString::fromUtf8("注册"), this);
        m_apply->setProperty("primary", true);

        const std::string request = m_license.requestCode();
        m_hardwareOk = !request.empty();
        if (m_hardwareOk) {
            m_code->setPlainText(QString::fromUtf8(request.c_str()));
        } else {
            m_copy->setEnabled(false);
            m_export->setEnabled(false);
            m_qr->setEnabled(false);
            m_apply->setEnabled(false);
        }
        const Vision::LicenseReason opened = m_hardwareOk ? m_license.open() : Vision::LicenseMissing;
        showState(opened);
        polishWidget(m_apply);
        m_pace = new RegisterPace(m_apply, m_status, this);

        QObject::connect(m_copy, &QPushButton::clicked, this, [this]() { copyRequest(); });
        QObject::connect(m_export, &QPushButton::clicked, this, [this]() {
            exportLicenseCode(this, licenseCodeText(m_code), m_status, QString::fromUtf8("申请码"));
        });
        QObject::connect(m_qr, &QPushButton::clicked, this, [this]() {
            showLicenseQr(this, licenseCodeText(m_code), QString::fromUtf8("申请码"), m_status);
        });
        QObject::connect(importBtn, &QPushButton::clicked, this, [this]() {
            importLicenseCode(this, m_paste, m_status, QString::fromUtf8("注册码"));
        });
        QObject::connect(m_apply, &QPushButton::clicked, this, [this]() { acceptCode(); });

        QHBoxLayout* head = new QHBoxLayout();
        head->addWidget(product);
        head->addStretch(1);
        head->addWidget(m_pill);
        QHBoxLayout* requestHead = new QHBoxLayout();
        requestHead->addWidget(requestTitle);
        requestHead->addStretch(1);
        requestHead->addWidget(m_copy);
        requestHead->addWidget(m_export);
        requestHead->addWidget(m_qr);
        QHBoxLayout* regHead = new QHBoxLayout();
        regHead->addWidget(regTitle);
        regHead->addStretch(1);
        regHead->addWidget(importBtn);
        QHBoxLayout* foot = new QHBoxLayout();
        foot->addWidget(m_status, 1);
        foot->addWidget(m_apply);

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 12);
        layout->setSpacing(10);
        layout->addLayout(head);
        layout->addWidget(lead);
        layout->addWidget(card);
        layout->addLayout(requestHead);
        layout->addWidget(m_code, 1);
        layout->addWidget(m_note);
        layout->addLayout(regHead);
        layout->addWidget(m_paste, 1);
        layout->addLayout(foot);
        applyLicenseWindowIcon(this);
    }

private:
    /** @brief 换状态胶囊的样式名和文字 */
    void setPill(const char* name, const QString& text)
    {
        m_pill->setObjectName(QString::fromLatin1(name));
        m_pill->setText(text);
        polishWidget(m_pill);
    }

    /** @brief 按已经落地的许可证刷新卡片、申请码说明和状态 */
    void showState(Vision::LicenseReason reason)
    {
        if (!m_hardwareOk) {
            setPill("pillBad", QString::fromUtf8("无法申请"));
            m_idValue->setText(QString::fromUtf8("—"));
            m_kindValue->setText(QString::fromUtf8("—"));
            m_serialValue->setText(QString::fromUtf8("—"));
            m_note->setText(QString::fromUtf8("读不到 CPU 或主板信息，发不出申请码，也不能注册。"));
            m_status->setText(QString::fromUtf8("这台电脑缺少生成申请码需要的硬件信息。"));
            return;
        }
        m_note->setText(QString::fromUtf8(
            "硬件不变时申请码不变。注册成功后序号增加，申请码会变，要重新发给厂家。"));
        if (reason != Vision::LicenseOk) {
            m_idValue->setText(QString::fromUtf8("—"));
            m_kindValue->setText(QString::fromUtf8("—"));
            m_serialValue->setText(QString::fromUtf8("—"));
        } else {
            const Vision::LicenseInfo info = m_license.describe();
            m_idValue->setText(QString::fromUtf8(info.licenseId.c_str()));
            if (info.kind == Vision::LicenseUntil) {
                m_kindValue->setText(QString::fromUtf8("到期 ") + QString::fromUtf8(info.until.c_str()));
            } else {
                m_kindValue->setText(QString::fromUtf8("永久"));
            }
            m_serialValue->setText(QString::number(info.serial));
        }
        const QString why = QString::fromUtf8(m_license.reasonText(reason).c_str());
        if (reason == Vision::LicenseOk) {
            setPill("pillOk", QString::fromUtf8("已注册"));
            m_status->setText(QString::fromUtf8("这台电脑已经注册。换新注册码时，序号必须大于 %1。")
                .arg(m_serialValue->text()));
            return;
        }
        if (reason == Vision::LicenseExpired || reason == Vision::LicenseClock
            || reason == Vision::LicenseRecordsCleared || reason == Vision::LicenseDamaged) {
            setPill("pillBad", QString::fromUtf8("需处理"));
        } else {
            setPill("pillIdle", QString::fromUtf8("未注册"));
        }
        m_status->setText(why + nextStep(reason));
    }

    /** @brief 当前许可证状态下，用户下一步该做什么 */
    QString nextStep(Vision::LicenseReason reason) const
    {
        if (reason == Vision::LicenseMissing) {
            return QString::fromUtf8("把上面的申请码发给厂家。");
        }
        if (reason == Vision::LicenseExpired || reason == Vision::LicenseDamaged
            || reason == Vision::LicenseBadSign || reason == Vision::LicenseProduct) {
            return QString::fromUtf8("向厂家要一份新的注册码，粘贴到下面。");
        }
        if (reason == Vision::LicenseClock) {
            return QString::fromUtf8("把系统时间改回正确时间后再打开本程序。");
        }
        if (reason == Vision::LicenseRecordsCleared) {
            return QString::fromUtf8("时间记录需要厂家在这台电脑上处理。");
        }
        if (reason == Vision::LicenseMachine) {
            return QString::fromUtf8("硬件已变，把新的申请码发给厂家。");
        }
        if (reason == Vision::LicenseSaveFailed) {
            return QString::fromUtf8("检查 ProgramData\\VisionPlatform\\license 是否可以写入。");
        }
        return QString();
    }

    /** @brief 复制申请码 */
    void copyRequest()
    {
        if (m_code->toPlainText().trimmed().isEmpty()) {
            m_status->setText(QString::fromUtf8("还没有申请码。"));
            return;
        }
        QApplication::clipboard()->setText(m_code->toPlainText());
        m_status->setText(QString::fromUtf8("已复制申请码。"));
    }

    /** @brief 验注册码。成功后刷新申请码，因为序号已经变了 */
    void acceptCode()
    {
        if (!m_pace->allow()) {
            return;
        }
        const QString compact = licenseCodeCompact(m_paste->toPlainText());
        if (compact.isEmpty()) {
            m_status->setText(QString::fromUtf8("先粘贴厂家发回的注册码。"));
            return;
        }
        if (compact.startsWith(QStringLiteral("VP-A1-"), Qt::CaseInsensitive)) {
            m_status->setText(QString::fromUtf8("这是申请码。请粘贴厂家发回的注册码。"));
            return;
        }
        if (!compact.startsWith(QStringLiteral("VP-R1-"), Qt::CaseInsensitive)) {
            m_status->setText(QString::fromUtf8("注册码应以 VP-R1- 开头。"));
            return;
        }
        const Vision::LicenseReason reason = m_license.accept(compact.toUtf8().constData());
        if (reason == Vision::LicenseOk) {
            const std::string request = m_license.requestCode();
            if (!request.empty()) {
                m_code->setPlainText(QString::fromUtf8(request.c_str()));
            }
            showState(Vision::LicenseOk);
            m_status->setText(QString::fromUtf8("注册完成。许可证已写入这台电脑。"));
            return;
        }
        m_status->setText(QString::fromUtf8(m_license.reasonText(reason).c_str()) + nextStep(reason));
    }

    Vision::LicenseTool m_license;   ///< 本机申请和写入
    bool m_hardwareOk;               ///< 申请码已经算出来
    QLabel* m_pill;                  ///< 状态胶囊
    QLabel* m_idValue;               ///< 许可编号
    QLabel* m_kindValue;             ///< 永久或到期日
    QLabel* m_serialValue;           ///< 已落地序号
    QPlainTextEdit* m_code;          ///< 申请码
    QLabel* m_note;                  ///< 申请码说明
    QPlainTextEdit* m_paste;         ///< 注册码
    QLabel* m_status;                ///< 底部状态
    QPushButton* m_apply;            ///< 注册
    QPushButton* m_copy;             ///< 复制申请码
    QPushButton* m_export;           ///< 导出申请码
    QPushButton* m_qr;               ///< 申请码二维码
    RegisterPace* m_pace;            ///< 注册限流
};

}

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Vision.LicenseRequest"));
    applyLicenseUiTheme(&app);
    RequestWindow window;
    window.show();
    return app.exec();
}
