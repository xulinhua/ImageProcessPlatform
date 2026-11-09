/**
 * @file main.cpp
 * @brief 厂家电脑上的注册机。私钥表在程序旁边的 keys 目录，公钥必须在库内的公钥表里
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LicenseSign.h"
#include "VisionLicenseTool.h"
#include "code_ui.h"
#include "key_vault.h"
#include "theme.h"
#include <QApplication>
#include <QCalendarWidget>
#include <QClipboard>
#include <QDate>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

namespace {

/** @brief 程序旁边 sign.key 的核对结果 */
enum KeyState {
    KeyMissing = 0,    ///< 没有选中可签发的私钥
    KeyMismatch,       ///< 选中的公钥还没编进当前程序
    KeyReady           ///< 可以签发
};

/** @brief 当前 exe 所在目录，不带结尾斜杠 */
std::wstring exeDir()
{
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring ws(path, n);
    const size_t slash = ws.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return std::wstring();
    }
    return ws.substr(0, slash);
}

/** @brief 正文的 SHA-256 */
bool sha256(const std::string& body, unsigned char hash[32])
{
    BCRYPT_ALG_HANDLE alg = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, 0, 0) != 0) {
        return false;
    }
    DWORD objLen = 0;
    DWORD cb = 0;
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cb, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    std::vector<unsigned char> obj(objLen);
    BCRYPT_HASH_HANDLE h = 0;
    if (BCryptCreateHash(alg, &h, &obj[0], objLen, 0, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    const NTSTATUS st = BCryptHashData(h, reinterpret_cast<PUCHAR>(const_cast<char*>(body.data())),
                                       static_cast<ULONG>(body.size()), 0);
    const NTSTATUS done = BCryptFinishHash(h, hash, 32, 0);
    BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0 && done == 0;
}

/** @brief 用私钥对正文的 SHA-256 做 ECDSA P-256 签名，输出 r||s 共 64 字节 */
bool signBody(const std::vector<unsigned char>& key, const std::string& body, unsigned char sig[64])
{
    if (key.empty()) {
        return false;
    }
    unsigned char hash[32];
    if (!sha256(body, hash)) {
        return false;
    }
    BCRYPT_ALG_HANDLE alg = 0;
    BCRYPT_KEY_HANDLE handle = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, 0, 0) != 0) {
        return false;
    }
    if (BCryptImportKeyPair(alg, 0, BCRYPT_ECCPRIVATE_BLOB, &handle,
                            const_cast<PUCHAR>(&key[0]), static_cast<ULONG>(key.size()), 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    ULONG n = 64;
    const NTSTATUS st = BCryptSignHash(handle, 0, hash, 32, sig, 64, &n, 0);
    BCryptDestroyKey(handle);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0 && n == 64;
}

/** @brief 严格的 yyyy-MM-dd 日期（补零、日期真实存在） */
bool validDate(const QString& text)
{
    const QDate date = QDate::fromString(text, QStringLiteral("yyyy-MM-dd"));
    return date.isValid() && date.toString(QStringLiteral("yyyy-MM-dd")) == text;
}

/** @brief 厂家签发窗口 */
class SignWindow : public QWidget {
public:
    /** @brief 启动口令。对返回 true；输错提示后返回 false，直接关闭也返回 false */
    static bool confirmPassword()
    {
        QDialog dlg;
        dlg.setWindowTitle(QString::fromUtf8("注册机"));
        dlg.setObjectName(QStringLiteral("licenseWindow"));
        dlg.setFixedWidth(320);
        QLabel* title = new QLabel(QString::fromUtf8("输入密码"), &dlg);
        title->setObjectName(QStringLiteral("product"));
        QLineEdit* edit = new QLineEdit(&dlg);
        edit->setEchoMode(QLineEdit::Password);
        edit->setPlaceholderText(QString::fromUtf8("密码"));
        QPushButton* ok = new QPushButton(QString::fromUtf8("确定"), &dlg);
        ok->setProperty("primary", true);
        polishWidget(ok);
        bool wrong = false;
        const auto submit = [&]() {
            if (edit->text() == QStringLiteral("hoson123")) {
                dlg.accept();
                return;
            }
            wrong = true;
            dlg.reject();
        };
        QObject::connect(ok, &QPushButton::clicked, &dlg, submit);
        QObject::connect(edit, &QLineEdit::returnPressed, &dlg, submit);
        QHBoxLayout* row = new QHBoxLayout();
        row->addStretch(1);
        row->addWidget(ok);
        QVBoxLayout* layout = new QVBoxLayout(&dlg);
        layout->setContentsMargins(16, 14, 16, 12);
        layout->setSpacing(12);
        layout->addWidget(title);
        layout->addWidget(edit);
        layout->addLayout(row);
        edit->setFocus();
        applyLicenseWindowIcon(&dlg);
        if (dlg.exec() == QDialog::Accepted) {
            return true;
        }
        if (wrong) {
            QMessageBox box(QMessageBox::Warning, QString::fromUtf8("注册机"), QString::fromUtf8("密码错误。"));
            applyLicenseWindowIcon(&box);
            box.exec();
        }
        return false;
    }

    SignWindow()
        : QWidget()
        , m_vault()
        , m_keyState(KeyMissing)
        , m_pill(0)
        , m_request(0)
        , m_productValue(0)
        , m_digestValue(0)
        , m_seenValue(0)
        , m_licenseId(0)
        , m_serial(0)
        , m_perpetual(0)
        , m_until(0)
        , m_untilDate(0)
        , m_calBtn(0)
        , m_calendar(0)
        , m_result(0)
        , m_status(0)
        , m_issue(0)
    {
        m_vault.load(exeDir());
        setObjectName(QStringLiteral("licenseWindow"));
        setWindowTitle(QString::fromUtf8("注册机"));
        resize(680, 700);

        QLabel* productHead = new QLabel(QString::fromUtf8("注册机"), this);
        productHead->setObjectName(QStringLiteral("product"));
        m_pill = new QLabel(this);
        QLabel* lead = new QLabel(QString::fromUtf8(
            "查看客户申请码，填写编号和期限后签发。钥匙在「密钥」里管理，签发用的那把必须已经编进当前程序。"), this);
        lead->setObjectName(QStringLiteral("lead"));
        lead->setWordWrap(true);

        QLabel* requestTitle = new QLabel(QString::fromUtf8("1  客户申请码"), this);
        requestTitle->setObjectName(QStringLiteral("section"));
        QPushButton* importBtn = new QPushButton(QString::fromUtf8("导入"), this);
        QPushButton* look = new QPushButton(QString::fromUtf8("查看"), this);
        m_request = new QPlainTextEdit(this);
        m_request->setObjectName(QStringLiteral("codeBox"));
        m_request->setPlaceholderText(QString::fromUtf8("粘贴客户发来的申请码，前缀是 VP-A1-"));

        QFrame* card = new QFrame(this);
        card->setObjectName(QStringLiteral("card"));
        m_productValue = new QLabel(QString::fromUtf8("—"), card);
        m_seenValue = new QLabel(QString::fromUtf8("—"), card);
        m_digestValue = new QLabel(QString::fromUtf8("还没有查看"), card);
        m_digestValue->setWordWrap(true);
        m_digestValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QLabel* productKey = new QLabel(QString::fromUtf8("产品"), card);
        QLabel* seenKey = new QLabel(QString::fromUtf8("客户序号"), card);
        QLabel* digestKey = new QLabel(QString::fromUtf8("摘要"), card);
        productKey->setObjectName(QStringLiteral("keyName"));
        seenKey->setObjectName(QStringLiteral("keyName"));
        digestKey->setObjectName(QStringLiteral("keyName"));
        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 8, 12, 8);
        cardLayout->setSpacing(6);
        QHBoxLayout* factRow = new QHBoxLayout();
        factRow->addWidget(productKey);
        factRow->addWidget(m_productValue);
        factRow->addSpacing(16);
        factRow->addWidget(seenKey);
        factRow->addWidget(m_seenValue);
        factRow->addStretch(1);
        QHBoxLayout* digestRow = new QHBoxLayout();
        digestRow->addWidget(digestKey);
        digestRow->addWidget(m_digestValue, 1);
        cardLayout->addLayout(factRow);
        cardLayout->addLayout(digestRow);

        QLabel* issueTitle = new QLabel(QString::fromUtf8("2  签发"), this);
        issueTitle->setObjectName(QStringLiteral("section"));
        m_licenseId = new QLineEdit(QStringLiteral("issued"), this);
        m_serial = new QSpinBox(this);
        m_serial->setRange(1, 2147483647);
        m_serial->setValue(1);
        m_perpetual = new QRadioButton(QString::fromUtf8("永久注册"), this);
        m_until = new QRadioButton(QString::fromUtf8("到期日"), this);
        m_perpetual->setChecked(true);
        m_untilDate = new QLineEdit(this);
        m_untilDate->setPlaceholderText(QStringLiteral("YYYY-MM-DD"));
        m_untilDate->setEnabled(false);
        m_calBtn = new QPushButton(QString::fromUtf8("日历"), this);
        m_calBtn->setEnabled(false);
        m_calendar = new QCalendarWidget(this);
        m_calendar->setWindowFlags(Qt::Popup);
        m_calendar->setGridVisible(true);
        QLabel* serialHint = new QLabel(QString::fromUtf8("序号必须大于客户机器上的当前序号。编号只写入日志，不参与验机。"), this);
        serialHint->setObjectName(QStringLiteral("hint"));
        serialHint->setWordWrap(true);

        QLabel* resultTitle = new QLabel(QString::fromUtf8("3  注册码"), this);
        resultTitle->setObjectName(QStringLiteral("section"));
        QPushButton* copyBtn = new QPushButton(QString::fromUtf8("复制"), this);
        QPushButton* exportBtn = new QPushButton(QString::fromUtf8("导出"), this);
        QPushButton* qrBtn = new QPushButton(QString::fromUtf8("二维码"), this);
        m_result = new QPlainTextEdit(this);
        m_result->setObjectName(QStringLiteral("codeBox"));
        m_result->setReadOnly(true);
        m_result->setPlaceholderText(QString::fromUtf8("签发后显示在这里，前缀是 VP-R1-"));
        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        m_issue = new QPushButton(QString::fromUtf8("签发"), this);
        m_issue->setProperty("primary", true);
        polishWidget(m_issue);
        reloadFromVault();

        QObject::connect(m_until, &QRadioButton::toggled, this, [this](bool on) {
            m_untilDate->setEnabled(on);
            m_calBtn->setEnabled(on);
            if (!on) {
                m_untilDate->clear();
            }
        });
        QObject::connect(m_calBtn, &QPushButton::clicked, this, [this]() {
            const QDate parsed = QDate::fromString(m_untilDate->text().trimmed(), QStringLiteral("yyyy-MM-dd"));
            if (parsed.isValid()) {
                m_calendar->setSelectedDate(parsed);
            }
            m_calendar->move(m_calBtn->mapToGlobal(QPoint(0, m_calBtn->height())));
            m_calendar->show();
        });
        QObject::connect(m_calendar, &QCalendarWidget::clicked, this, [this](QDate date) {
            m_untilDate->setText(date.toString(QStringLiteral("yyyy-MM-dd")));
            m_calendar->hide();
        });
        QObject::connect(look, &QPushButton::clicked, this, [this]() { lookRequest(); });
        QObject::connect(m_issue, &QPushButton::clicked, this, [this]() { issue(); });
        QObject::connect(importBtn, &QPushButton::clicked, this, [this]() {
            importLicenseCode(this, m_request, m_status, QString::fromUtf8("申请码"));
        });
        QObject::connect(exportBtn, &QPushButton::clicked, this, [this]() {
            exportLicenseCode(this, licenseCodeText(m_result), m_status, QString::fromUtf8("注册码"));
        });
        QObject::connect(qrBtn, &QPushButton::clicked, this, [this]() {
            showLicenseQr(this, licenseCodeText(m_result), QString::fromUtf8("注册码"), m_status);
        });
        QObject::connect(copyBtn, &QPushButton::clicked, this, [this]() { copyResult(); });

        QPushButton* keysBtn = new QPushButton(QString::fromUtf8("密钥"), this);
        QObject::connect(keysBtn, &QPushButton::clicked, this, [this]() {
            m_vault.manage(this);
            reloadFromVault();
        });
        QHBoxLayout* head = new QHBoxLayout();
        head->addWidget(productHead);
        head->addStretch(1);
        head->addWidget(keysBtn);
        head->addWidget(m_pill);
        QHBoxLayout* requestHead = new QHBoxLayout();
        requestHead->addWidget(requestTitle);
        requestHead->addStretch(1);
        requestHead->addWidget(importBtn);
        requestHead->addWidget(look);
        QHBoxLayout* idRow = new QHBoxLayout();
        QLabel* idKey = new QLabel(QString::fromUtf8("编号"), this);
        QLabel* serialKey = new QLabel(QString::fromUtf8("序号"), this);
        idKey->setObjectName(QStringLiteral("keyName"));
        serialKey->setObjectName(QStringLiteral("keyName"));
        idRow->addWidget(idKey);
        idRow->addWidget(m_licenseId, 1);
        idRow->addWidget(serialKey);
        idRow->addWidget(m_serial);
        QHBoxLayout* kindRow = new QHBoxLayout();
        kindRow->addWidget(m_perpetual);
        kindRow->addWidget(m_until);
        kindRow->addWidget(m_untilDate, 1);
        kindRow->addWidget(m_calBtn);
        QHBoxLayout* resultHead = new QHBoxLayout();
        resultHead->addWidget(resultTitle);
        resultHead->addStretch(1);
        resultHead->addWidget(copyBtn);
        resultHead->addWidget(exportBtn);
        resultHead->addWidget(qrBtn);
        QHBoxLayout* foot = new QHBoxLayout();
        foot->addWidget(m_status, 1);
        foot->addWidget(m_issue);

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 12);
        layout->setSpacing(10);
        layout->addLayout(head);
        layout->addWidget(lead);
        layout->addLayout(requestHead);
        layout->addWidget(m_request, 1);
        layout->addWidget(card);
        layout->addWidget(issueTitle);
        layout->addLayout(idRow);
        layout->addLayout(kindRow);
        layout->addWidget(serialHint);
        layout->addLayout(resultHead);
        layout->addWidget(m_result, 1);
        layout->addLayout(foot);
        applyLicenseWindowIcon(this);
    }

private:
    /** @brief 按当前签发钥匙刷新胶囊、私钥和签发按钮 */
    void reloadFromVault()
    {
        m_key.clear();
        if (!m_vault.copyActive(&m_key)) {
            m_keyState = KeyMissing;
        } else if (!m_vault.activeTrusted()) {
            m_key.clear();
            m_keyState = KeyMismatch;
        } else {
            m_keyState = KeyReady;
        }
        applyKeyState();
    }

    /** @brief 按私钥核对结果设置胶囊、签发按钮和说明 */
    void applyKeyState()
    {
        const bool ready = m_keyState == KeyReady;
        m_issue->setEnabled(ready);
        if (ready) {
            m_pill->setObjectName(QStringLiteral("pillOk"));
            m_pill->setText(QString::fromUtf8("钥匙 %1").arg(m_vault.activeId()));
            m_status->setText(QString::fromUtf8("当前用 %1 号钥匙「%2」签发。先查看申请码。")
                .arg(m_vault.activeId()).arg(m_vault.activeLabel()));
        } else if (m_keyState == KeyMissing) {
            m_pill->setObjectName(QStringLiteral("pillBad"));
            m_pill->setText(QString::fromUtf8("未选择钥匙"));
            m_status->setText(QString::fromUtf8("打开「密钥」，选择一把本程序已经信任的私钥。没有钥匙时只能查看申请码。"));
        } else {
            m_pill->setObjectName(QStringLiteral("pillBad"));
            m_pill->setText(QString::fromUtf8("钥匙待编译"));
            m_status->setText(QString::fromUtf8("选中的公钥还没编进当前程序。重新编译并发布后才能用它签发。"));
        }
        polishWidget(m_pill);
    }

    /** @brief 解开申请码，显示摘要，并把签发序号预填为当前序号加 1 */
    void lookRequest()
    {
        std::string digest;
        int seen = 0;
        const QString compact = licenseCodeCompact(m_request->toPlainText());
        if (compact.startsWith(QStringLiteral("VP-R1-"), Qt::CaseInsensitive)) {
            clearRequest(QString::fromUtf8("这是注册码。请粘贴客户的申请码。"));
            return;
        }
        if (!Vision::LicenseCodec::splitRequest(compact.toUtf8().constData(), &digest, &seen)) {
            clearRequest(QString::fromUtf8("申请码无效。应以 VP-A1- 开头。"));
            return;
        }
        m_productValue->setText(QStringLiteral("VisionPlatform"));
        m_digestValue->setText(QString::fromUtf8(digest.c_str()));
        m_seenValue->setText(QString::number(seen));
        if (seen >= 2147483647) {
            m_status->setText(QString::fromUtf8("客户机器的序号已经到顶，发不出更大的注册码。"));
            return;
        }
        m_serial->setValue(seen + 1);
        m_status->setText(QString::fromUtf8("客户机器当前序号是 %1，已填入 %2。").arg(seen).arg(seen + 1));
    }

    /** @brief 查看失败时清掉上一份申请信息 */
    void clearRequest(const QString& status)
    {
        m_productValue->setText(QString::fromUtf8("—"));
        m_digestValue->setText(QString::fromUtf8("申请码解不开"));
        m_seenValue->setText(QString::fromUtf8("—"));
        m_status->setText(status);
    }

    /** @brief 重新解申请码并校验序号、编号、到期日，签名后再用库内公钥验一次 */
    void issue()
    {
        if (m_keyState != KeyReady) {
            applyKeyState();
            return;
        }
        std::string digest;
        int seen = 0;
        if (!Vision::LicenseCodec::splitRequest(licenseCodeCompact(m_request->toPlainText()).toUtf8().constData(), &digest, &seen)) {
            m_status->setText(QString::fromUtf8("申请码无效。先粘贴并查看申请码。"));
            return;
        }
        if (m_serial->value() <= seen) {
            m_status->setText(QString::fromUtf8("序号必须大于客户机器上的当前序号 %1。").arg(seen));
            return;
        }
        const QString id = m_licenseId->text().trimmed();
        if (id.isEmpty() || id.contains(QLatin1Char('\n')) || id.contains(QLatin1Char('\r'))) {
            m_status->setText(QString::fromUtf8("编号不能为空，也不能换行。"));
            return;
        }
        const bool permanent = m_perpetual->isChecked();
        const QString date = m_untilDate->text().trimmed();
        if (!permanent && !validDate(date)) {
            m_status->setText(QString::fromUtf8("到期日要写成 YYYY-MM-DD，例如 2026-10-28。"));
            return;
        }
        const int keyId = m_vault.activeId();
        const std::string body = Vision::LicenseCodec::payload(
            id.toUtf8().constData(), digest, permanent, date.toUtf8().constData(), m_serial->value(), keyId);
        unsigned char sig[64];
        if (body.empty() || !signBody(m_key, body, sig)) {
            m_result->clear();
            m_status->setText(QString::fromUtf8("签发失败。%1 号钥匙不是可用的 P-256 私钥。").arg(keyId));
            return;
        }
        if (!Vision::LicenseSign::verify(body, sig, 64)) {
            m_result->clear();
            m_status->setText(QString::fromUtf8("签发失败。%1 号钥匙和库里的公钥不是一对。").arg(keyId));
            return;
        }
        const std::string code = Vision::LicenseCodec::registration(body, sig, 64);
        if (code.empty()) {
            m_result->clear();
            m_status->setText(QString::fromUtf8("注册码编码失败。"));
            return;
        }
        m_result->setPlainText(QString::fromUtf8(code.c_str()));
        m_status->setText(QString::fromUtf8("已签发。把注册码发给客户。它只对这份申请码对应的电脑有效。"));
    }

    /** @brief 复制注册码 */
    void copyResult()
    {
        if (m_result->toPlainText().isEmpty()) {
            m_status->setText(QString::fromUtf8("还没有注册码。"));
            return;
        }
        QApplication::clipboard()->setText(m_result->toPlainText());
        m_status->setText(QString::fromUtf8("已复制注册码。"));
    }

    KeyVault m_vault;                   ///< 本机私钥表
    std::vector<unsigned char> m_key;   ///< 当前签发私钥，对不上库时为空
    KeyState m_keyState;                ///< 启动时的核对结果
    QLabel* m_pill;                     ///< 私钥状态
    QPlainTextEdit* m_request;          ///< 申请码
    QLabel* m_productValue;             ///< 产品名
    QLabel* m_digestValue;              ///< 硬件摘要
    QLabel* m_seenValue;                ///< 客户当前序号
    QLineEdit* m_licenseId;             ///< 签发编号
    QSpinBox* m_serial;                 ///< 签发序号
    QRadioButton* m_perpetual;          ///< 永久
    QRadioButton* m_until;              ///< 限期
    QLineEdit* m_untilDate;             ///< 到期日
    QPushButton* m_calBtn;              ///< 打开日历
    QCalendarWidget* m_calendar;        ///< 选到期日
    QPlainTextEdit* m_result;           ///< 注册码
    QLabel* m_status;                   ///< 底部状态
    QPushButton* m_issue;               ///< 签发
};

}

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Vision.LicenseSign"));
    applyLicenseUiTheme(&app);
    if (!SignWindow::confirmPassword()) {
        return 0;
    }
    SignWindow window;
    window.show();
    return app.exec();
}
