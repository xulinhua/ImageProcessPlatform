/**
 * @file main.cpp
 * @brief 厂家电脑上的注册机。私钥只读程序旁边的 sign.key，不写客户电脑上的文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LicenseCodec.h"
#include "LicenseCodeUi.h"
#include "LicenseUiTheme.h"
#include <QApplication>
#include <QCalendarWidget>
#include <QClipboard>
#include <QDate>
#include <QDialog>
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
#include <fstream>
#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

namespace {

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

/** @brief 十六进制字符的值，非法字符返回 -1 */
int hexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/** @brief 读 exe 旁的 sign.key（十六进制文本，可含空白），得到 BCRYPT_ECCPRIVATE_BLOB */
bool loadKey(std::vector<unsigned char>* key)
{
    if (!key) {
        return false;
    }
    key->clear();
    const std::wstring path = exeDir() + L"\\sign.key";
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    int high = -1;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
            continue;
        }
        const int v = hexValue(c);
        if (v < 0) {
            key->clear();
            return false;
        }
        if (high < 0) {
            high = v;
        } else {
            key->push_back(static_cast<unsigned char>((high << 4) | v));
            high = -1;
        }
    }
    return high < 0 && !key->empty();
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
bool sign(const std::vector<unsigned char>& key, const std::string& body, unsigned char sig[64])
{
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

/** @brief 启动口令框。口令对返回 true；输错时提示后返回 false，直接关闭也返回 false */
bool confirmPassword()
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

}

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Vision.LicenseSign"));
    applyLicenseUiTheme(&app);
    if (!confirmPassword()) {
        return 0;
    }

    // 私钥在启动时读一次；读不到时只能查看申请码，签发按钮禁用
    std::vector<unsigned char> key;
    const bool keyOk = loadKey(&key);

    QWidget window;
    window.setObjectName(QStringLiteral("licenseWindow"));
    window.setWindowTitle(QString::fromUtf8("注册机"));
    window.resize(640, 640);

    QLabel* productHead = new QLabel(QString::fromUtf8("私钥"));
    productHead->setObjectName(QStringLiteral("product"));
    QLabel* keyPill = new QLabel(keyOk
        ? QString::fromUtf8("sign.key 已加载")
        : QString::fromUtf8("未找到 sign.key"));
    keyPill->setObjectName(keyOk ? QStringLiteral("pillOk") : QStringLiteral("pillBad"));

    // 控件：申请码区、申请信息、签发参数、注册码结果
    QLabel* requestTitle = new QLabel(QString::fromUtf8("1  客户申请码"));
    requestTitle->setObjectName(QStringLiteral("section"));
    QPushButton* importBtn = new QPushButton(QString::fromUtf8("导入"));
    QPushButton* look = new QPushButton(QString::fromUtf8("查看"));
    QPlainTextEdit* request = new QPlainTextEdit();
    request->setObjectName(QStringLiteral("codeBox"));
    request->setPlaceholderText(QString::fromUtf8("粘贴申请码"));

    QLabel* productKey = new QLabel(QString::fromUtf8("产品"));
    QLabel* digestKey = new QLabel(QString::fromUtf8("摘要"));
    QLabel* seenKey = new QLabel(QString::fromUtf8("当前序号"));
    productKey->setObjectName(QStringLiteral("keyName"));
    digestKey->setObjectName(QStringLiteral("keyName"));
    seenKey->setObjectName(QStringLiteral("keyName"));
    QLabel* productValue = new QLabel(QString::fromUtf8("—"));
    QLabel* digestValue = new QLabel(QString::fromUtf8("还没有查看"));
    QLabel* seenValue = new QLabel(QString::fromUtf8("—"));
    digestValue->setWordWrap(true);
    digestValue->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QLabel* issueTitle = new QLabel(QString::fromUtf8("2  签发"));
    issueTitle->setObjectName(QStringLiteral("section"));
    QLineEdit* licenseId = new QLineEdit(QStringLiteral("issued"));
    QSpinBox* serial = new QSpinBox();
    serial->setRange(1, 2147483647);
    serial->setValue(1);
    QRadioButton* perpetual = new QRadioButton(QString::fromUtf8("永久注册"));
    QRadioButton* until = new QRadioButton(QString::fromUtf8("到期日"));
    perpetual->setChecked(true);
    QLineEdit* untilDate = new QLineEdit();
    untilDate->setPlaceholderText(QStringLiteral("YYYY-MM-DD"));
    untilDate->setEnabled(false);
    QPushButton* calBtn = new QPushButton(QString::fromUtf8("日历"));
    calBtn->setEnabled(false);
    QCalendarWidget* calendar = new QCalendarWidget(&window);
    calendar->setWindowFlags(Qt::Popup);
    calendar->setGridVisible(true);

    QLabel* resultTitle = new QLabel(QString::fromUtf8("3  注册码"));
    resultTitle->setObjectName(QStringLiteral("section"));
    QPushButton* copyBtn = new QPushButton(QString::fromUtf8("复制"));
    QPushButton* exportBtn = new QPushButton(QString::fromUtf8("导出"));
    QPushButton* qrBtn = new QPushButton(QString::fromUtf8("二维码"));
    QPlainTextEdit* result = new QPlainTextEdit();
    result->setObjectName(QStringLiteral("codeBox"));
    result->setReadOnly(true);
    result->setPlaceholderText(QString::fromUtf8("签发后显示在这里"));
    QLabel* status = new QLabel(keyOk
        ? QString::fromUtf8("先查看申请码。序号会按客户机器上的当前序号加 1。")
        : QString::fromUtf8("没有找到可用的 sign.key，不能签发。"));
    status->setWordWrap(true);
    QPushButton* issue = new QPushButton(QString::fromUtf8("签发"));
    issue->setProperty("primary", true);
    issue->setEnabled(keyOk);
    polishWidget(issue);
    polishWidget(keyPill);

    // 到期日：选限期才可编辑，日历弹出在按钮下方
    QObject::connect(until, &QRadioButton::toggled, &window, [untilDate, calBtn](bool on) {
        untilDate->setEnabled(on);
        calBtn->setEnabled(on);
        if (!on) {
            untilDate->clear();
        }
    });
    QObject::connect(calBtn, &QPushButton::clicked, &window, [calendar, calBtn, untilDate]() {
        const QDate parsed = QDate::fromString(untilDate->text().trimmed(), QStringLiteral("yyyy-MM-dd"));
        if (parsed.isValid()) {
            calendar->setSelectedDate(parsed);
        }
        calendar->move(calBtn->mapToGlobal(QPoint(0, calBtn->height())));
        calendar->show();
    });
    QObject::connect(calendar, &QCalendarWidget::clicked, &window, [calendar, untilDate](QDate date) {
        untilDate->setText(date.toString(QStringLiteral("yyyy-MM-dd")));
        calendar->hide();
    });
    // 查看：解开申请码，显示摘要和客户当前序号，并把签发序号预填为当前序号加 1
    QObject::connect(look, &QPushButton::clicked, &window, [request, productValue, digestValue, seenValue, serial, status]() {
        std::string digest;
        int seen = 0;
        if (!Vision::LicenseCodec::splitRequest(request->toPlainText().toUtf8().constData(), &digest, &seen)) {
            productValue->setText(QString::fromUtf8("—"));
            digestValue->setText(QString::fromUtf8("申请码解不开"));
            seenValue->setText(QString::fromUtf8("—"));
            status->setText(QString::fromUtf8("申请码无效。"));
            return;
        }
        productValue->setText(QStringLiteral("VisionPlatform"));
        digestValue->setText(QString::fromUtf8(digest.c_str()));
        seenValue->setText(QString::number(seen));
        if (seen >= 2147483647) {
            status->setText(QString::fromUtf8("客户机器的序号已经到顶，发不出更大的注册码。"));
            return;
        }
        serial->setValue(seen + 1);
        status->setText(QString::fromUtf8("客户机器当前序号是 %1，已填入 %2。").arg(seen).arg(seen + 1));
    });
    // 签发：重新解申请码并校验序号、编号、到期日，再签名编码成注册码
    QObject::connect(issue, &QPushButton::clicked, &window, [&key, request, licenseId, serial, perpetual, untilDate, result, status]() {
        std::string digest;
        int seen = 0;
        if (!Vision::LicenseCodec::splitRequest(request->toPlainText().toUtf8().constData(), &digest, &seen)) {
            status->setText(QString::fromUtf8("申请码无效。"));
            return;
        }
        if (serial->value() <= seen) {
            status->setText(QString::fromUtf8("序号必须大于客户机器上的当前序号 %1。").arg(seen));
            return;
        }
        const QString id = licenseId->text().trimmed();
        if (id.isEmpty() || id.contains(QLatin1Char('\n'))) {
            status->setText(QString::fromUtf8("编号不能为空。"));
            return;
        }
        const bool permanent = perpetual->isChecked();
        const QString date = untilDate->text().trimmed();
        if (!permanent && !validDate(date)) {
            status->setText(QString::fromUtf8("到期日不符合标准，要写成 YYYY-MM-DD，例如 2026-10-28。"));
            return;
        }
        const std::string body = Vision::LicenseCodec::payload(
            id.toUtf8().constData(), digest, permanent, date.toUtf8().constData(), serial->value());
        unsigned char sig[64];
        if (body.empty() || !sign(key, body, sig)) {
            status->setText(QString::fromUtf8("签发失败。sign.key 和库里的公钥不是一对。"));
            return;
        }
        const std::string code = Vision::LicenseCodec::registration(body, sig, 64);
        if (code.empty()) {
            status->setText(QString::fromUtf8("注册码编码失败。"));
            return;
        }
        result->setPlainText(QString::fromUtf8(code.c_str()));
        status->setText(QString::fromUtf8("已签发。把注册码发给客户。"));
    });
    // 申请码导入，注册码导出、二维码、复制
    QObject::connect(importBtn, &QPushButton::clicked, &window, [&window, request, status]() {
        importLicenseCode(&window, request, status, QString::fromUtf8("申请码"));
    });
    QObject::connect(exportBtn, &QPushButton::clicked, &window, [&window, result, status]() {
        exportLicenseCode(&window, licenseCodeText(result), status, QString::fromUtf8("注册码"));
    });
    QObject::connect(qrBtn, &QPushButton::clicked, &window, [&window, result, status]() {
        showLicenseQr(&window, licenseCodeText(result), QString::fromUtf8("注册码"), status);
    });
    QObject::connect(copyBtn, &QPushButton::clicked, &window, [result, status]() {
        if (result->toPlainText().isEmpty()) {
            status->setText(QString::fromUtf8("还没有注册码。"));
            return;
        }
        QApplication::clipboard()->setText(result->toPlainText());
        status->setText(QString::fromUtf8("已复制注册码。"));
    });

    // 布局
    QHBoxLayout* head = new QHBoxLayout();
    head->addWidget(productHead);
    head->addStretch(1);
    head->addWidget(keyPill);
    QHBoxLayout* requestHead = new QHBoxLayout();
    requestHead->addWidget(requestTitle);
    requestHead->addStretch(1);
    requestHead->addWidget(importBtn);
    requestHead->addWidget(look);
    QHBoxLayout* factRow = new QHBoxLayout();
    factRow->addWidget(productKey);
    factRow->addWidget(productValue);
    factRow->addSpacing(16);
    factRow->addWidget(seenKey);
    factRow->addWidget(seenValue);
    factRow->addStretch(1);
    QHBoxLayout* digestRow = new QHBoxLayout();
    digestRow->addWidget(digestKey);
    digestRow->addWidget(digestValue, 1);
    QHBoxLayout* idRow = new QHBoxLayout();
    QLabel* idKey = new QLabel(QString::fromUtf8("编号"));
    QLabel* serialKey = new QLabel(QString::fromUtf8("序号"));
    idKey->setObjectName(QStringLiteral("keyName"));
    serialKey->setObjectName(QStringLiteral("keyName"));
    idRow->addWidget(idKey);
    idRow->addWidget(licenseId, 1);
    idRow->addWidget(serialKey);
    idRow->addWidget(serial);
    QHBoxLayout* kindRow = new QHBoxLayout();
    kindRow->addWidget(perpetual);
    kindRow->addWidget(until);
    kindRow->addWidget(untilDate, 1);
    kindRow->addWidget(calBtn);
    QHBoxLayout* resultHead = new QHBoxLayout();
    resultHead->addWidget(resultTitle);
    resultHead->addStretch(1);
    resultHead->addWidget(copyBtn);
    resultHead->addWidget(exportBtn);
    resultHead->addWidget(qrBtn);
    QHBoxLayout* foot = new QHBoxLayout();
    foot->addWidget(status, 1);
    foot->addWidget(issue);

    QVBoxLayout* layout = new QVBoxLayout(&window);
    layout->setContentsMargins(16, 14, 16, 12);
    layout->setSpacing(12);
    layout->addLayout(head);
    layout->addLayout(requestHead);
    layout->addWidget(request, 1);
    layout->addLayout(factRow);
    layout->addLayout(digestRow);
    layout->addWidget(issueTitle);
    layout->addLayout(idRow);
    layout->addLayout(kindRow);
    layout->addLayout(resultHead);
    layout->addWidget(result, 1);
    layout->addLayout(foot);
    applyLicenseWindowIcon(&window);
    window.show();
    return app.exec();
}
