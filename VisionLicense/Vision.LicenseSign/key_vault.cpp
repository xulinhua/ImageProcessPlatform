/**
 * @file key_vault.cpp
 * @brief 注册机的私钥目录和公钥表源码
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "key_vault.h"
#include "LicenseSign.h"
#include "theme.h"
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

namespace {

/** @brief 十六进制字符的值，非法返回 -1 */
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

/** @brief 读十六进制私钥文件 */
bool readHexKey(const std::wstring& path, std::vector<unsigned char>* key)
{
    if (!key) {
        return false;
    }
    key->clear();
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

/** @brief 把私钥写成十六进制文本 */
bool writeHexKey(const std::wstring& path, const std::vector<unsigned char>& key)
{
    static const char* kHex = "0123456789abcdef";
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    for (size_t i = 0; i < key.size(); ++i) {
        out << kHex[key[i] >> 4] << kHex[key[i] & 0x0f];
        if ((i + 1) % 32 == 0) {
            out << "\n";
        }
    }
    if (key.size() % 32 != 0) {
        out << "\n";
    }
    return out.good();
}

/** @brief 从 P-256 私钥块取出公钥块。私钥魔数是 ECS2 */
bool publicFromPrivate(const std::vector<unsigned char>& priv, unsigned char pub[72])
{
    if (priv.size() != 104 || priv[0] != 0x45 || priv[1] != 0x43 || priv[2] != 0x53 || priv[3] != 0x32) {
        return false;
    }
    pub[0] = 0x45;
    pub[1] = 0x43;
    pub[2] = 0x53;
    pub[3] = 0x31;
    memcpy(pub + 4, &priv[4], 68);
    return true;
}

/** @brief 生成一把 P-256 私钥 */
bool generatePrivate(std::vector<unsigned char>* key)
{
    if (!key) {
        return false;
    }
    key->clear();
    BCRYPT_ALG_HANDLE alg = 0;
    BCRYPT_KEY_HANDLE handle = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, 0, 0) != 0) {
        return false;
    }
    if (BCryptGenerateKeyPair(alg, &handle, 256, 0) != 0 || BCryptFinalizeKeyPair(handle, 0) != 0) {
        if (handle) {
            BCryptDestroyKey(handle);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    ULONG n = 0;
    if (BCryptExportKey(handle, 0, BCRYPT_ECCPRIVATE_BLOB, 0, 0, &n, 0) != 0 || n == 0) {
        BCryptDestroyKey(handle);
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    key->resize(n);
    const NTSTATUS st = BCryptExportKey(handle, 0, BCRYPT_ECCPRIVATE_BLOB, &(*key)[0], n, &n, 0);
    BCryptDestroyKey(handle);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (st != 0) {
        key->clear();
        return false;
    }
    key->resize(n);
    return n == 104;
}

/** @brief 去掉路径最后一级 */
std::wstring parentDir(const std::wstring& dir)
{
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos || slash == 0) {
        return std::wstring();
    }
    return dir.substr(0, slash);
}

/** @brief 文件是否存在 */
bool fileExists(const std::wstring& path)
{
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

/** @brief 名称里不能有制表和换行 */
QString cleanLabel(const QString& text)
{
    QString out;
    const QString trimmed = text.trimmed();
    for (int i = 0; i < trimmed.size() && out.size() < 32; ++i) {
        const QChar c = trimmed.at(i);
        if (c == QLatin1Char('\t') || c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            out.append(QLatin1Char(' '));
        } else {
            out.append(c);
        }
    }
    return out.trimmed();
}

/** @brief 字节转成 0xNN 列表，每行 16 个 */
std::string cBytes(const unsigned char* data, int n)
{
    static const char* kHex = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < n; ++i) {
        if (i > 0) {
            out += ",";
            out += (i % 16 == 0) ? "\n    " : " ";
        }
        out += "0x";
        out += kHex[data[i] >> 4];
        out += kHex[data[i] & 0x0f];
    }
    return out;
}

}

KeyVault::KeyVault()
{
}

bool KeyVault::load(const std::wstring& exeDirectory)
{
    m_exeDir = exeDirectory;
    m_dir = exeDirectory + L"\\keys";
    m_entries.clear();
    CreateDirectoryW(m_dir.c_str(), 0);
    const std::wstring indexPath = m_dir + L"\\keys.txt";
    std::ifstream in(indexPath.c_str(), std::ios::binary);
    if (in) {
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::istringstream lines(text);
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r') {
                line.erase(line.size() - 1);
            }
            if (line.empty()) {
                continue;
            }
            const size_t tab1 = line.find('\t');
            const size_t tab2 = tab1 == std::string::npos ? std::string::npos : line.find('\t', tab1 + 1);
            if (tab1 == std::string::npos || tab2 == std::string::npos) {
                continue;
            }
            Entry entry;
            entry.id = atoi(line.substr(0, tab1).c_str());
            entry.label = QString::fromUtf8(line.substr(tab1 + 1, tab2 - tab1 - 1).c_str());
            entry.state = QString::fromUtf8(line.substr(tab2 + 1).c_str());
            entry.hasPub = false;
            entry.trusted = false;
            if (entry.id < 1) {
                continue;
            }
            readHexKey(m_dir + L"\\" + std::to_wstring(entry.id) + L".key", &entry.priv);
            m_entries.push_back(entry);
        }
    }
    if (m_entries.empty()) {
        std::vector<unsigned char> imported;
        if (readHexKey(m_exeDir + L"\\sign.key", &imported)) {
            Entry entry;
            entry.id = 1;
            entry.label = QString::fromUtf8("初始");
            entry.state = QStringLiteral("sign");
            entry.priv = imported;
            entry.hasPub = false;
            entry.trusted = false;
            m_entries.push_back(entry);
            saveIndex();
            writeHexKey(m_dir + L"\\1.key", imported);
        }
    }
    refresh();
    return true;
}

int KeyVault::activeId() const
{
    const Entry* entry = 0;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state == QLatin1String("sign")) {
            entry = &m_entries[i];
            break;
        }
    }
    return entry ? entry->id : 0;
}

QString KeyVault::activeLabel() const
{
    const Entry* entry = find(activeId());
    return entry ? entry->label : QString();
}

bool KeyVault::copyActive(std::vector<unsigned char>* key) const
{
    const Entry* entry = find(activeId());
    if (!key || !entry || entry->priv.empty()) {
        return false;
    }
    *key = entry->priv;
    return true;
}

bool KeyVault::activeTrusted() const
{
    const Entry* entry = find(activeId());
    return entry && entry->trusted;
}

QVector<KeyRow> KeyVault::rows() const
{
    QVector<KeyRow> out;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state == QLatin1String("drop")) {
            continue;
        }
        KeyRow row;
        row.id = m_entries[i].id;
        row.label = m_entries[i].label;
        row.trusted = m_entries[i].trusted;
        row.active = m_entries[i].state == QLatin1String("sign");
        out.push_back(row);
    }
    return out;
}

QString KeyVault::generate(const QString& label)
{
    std::vector<unsigned char> priv;
    if (!generatePrivate(&priv)) {
        return QString::fromUtf8("没有生成出 P-256 私钥。");
    }
    Entry entry;
    entry.id = nextId();
    entry.label = cleanLabel(label);
    if (entry.label.isEmpty()) {
        entry.label = QString::fromUtf8("钥匙 %1").arg(entry.id);
    }
    entry.state = QStringLiteral("keep");
    entry.priv = priv;
    entry.hasPub = false;
    entry.trusted = false;
    if (!writeHexKey(m_dir + L"\\" + std::to_wstring(entry.id) + L".key", priv)) {
        return QString::fromUtf8("私钥没有写进 keys 目录。");
    }
    m_entries.push_back(entry);
    refresh();
    if (!saveIndex()) {
        return QString::fromUtf8("钥匙目录没有保存成功。");
    }
    QString error;
    const bool wrote = writeTable(&error);
    QString text = QString::fromUtf8("已生成 %1 号钥匙「%2」。私钥在本程序旁边的 keys 目录，不进客户包。")
        .arg(entry.id).arg(entry.label);
    if (wrote) {
        text += QString::fromUtf8("公钥已写入源码。重新编译并发布后，客户程序才会信任它。这次运行还不能用它签发。");
    } else {
        text += error;
    }
    return text;
}

QString KeyVault::use(int id)
{
    Entry* entry = find(id);
    if (!entry || entry->state == QLatin1String("drop") || entry->priv.empty()) {
        return QString::fromUtf8("这把钥匙没有可用的私钥。");
    }
    if (!entry->trusted) {
        return QString::fromUtf8("这把公钥还没编进当前程序。重新编译并发布后才能用它签发。");
    }
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state == QLatin1String("sign")) {
            m_entries[i].state = QStringLiteral("keep");
        }
    }
    entry->state = QStringLiteral("sign");
    if (!saveIndex()) {
        return QString::fromUtf8("没有保存当前签发钥匙。");
    }
    return QString::fromUtf8("之后用 %1 号钥匙「%2」签发。").arg(entry->id).arg(entry->label);
}

QString KeyVault::remove(int id)
{
    Entry* entry = find(id);
    if (!entry || entry->state == QLatin1String("drop")) {
        return QString::fromUtf8("没有这把钥匙。");
    }
    int alive = 0;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state != QLatin1String("drop")) {
            ++alive;
        }
    }
    const int baked = Vision::LicenseSign::count();
    for (int i = 0; i < baked; ++i) {
        int bakedId = 0;
        unsigned char pub[80];
        int pubLen = 0;
        if (!Vision::LicenseSign::at(i, &bakedId, pub, 80, &pubLen)) {
            continue;
        }
        const Entry* known = find(bakedId);
        if (!known) {
            ++alive;
        }
    }
    if (alive <= 1) {
        return QString::fromUtf8("至少留一把钥匙。");
    }
    const QString label = entry->label;
    entry->state = QStringLiteral("drop");
    if (entry->state == QLatin1String("sign")) {
        entry->state = QStringLiteral("drop");
    }
    if (!saveIndex()) {
        return QString::fromUtf8("没有保存移除结果。");
    }
    QString error;
    writeTable(&error);
    QString text = QString::fromUtf8("已从下一版公钥表移除 %1 号「%2」。重新编译发布后，它签过的注册码会失效。私钥文件还留在 keys 目录。")
        .arg(id).arg(label);
    if (!error.isEmpty()) {
        text += error;
    }
    return text;
}

QString KeyVault::publicHex(int id) const
{
    const Entry* entry = find(id);
    if (!entry || !entry->hasPub) {
        return QString();
    }
    static const char* kHex = "0123456789abcdef";
    QString out;
    for (int i = 0; i < 72; ++i) {
        out.append(QLatin1Char(kHex[entry->pub[i] >> 4]));
        out.append(QLatin1Char(kHex[entry->pub[i] & 0x0f]));
    }
    return out;
}

void KeyVault::manage(QWidget* parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QString::fromUtf8("密钥"));
    dlg.setObjectName(QStringLiteral("licenseWindow"));
    dlg.resize(560, 420);
    QLabel* lead = new QLabel(QString::fromUtf8(
        "私钥只在这台电脑的 keys 目录。新公钥要重新编译发布后，客户程序才会信任。"), &dlg);
    lead->setObjectName(QStringLiteral("lead"));
    lead->setWordWrap(true);
    QListWidget* list = new QListWidget(&dlg);
    QLineEdit* name = new QLineEdit(&dlg);
    name->setPlaceholderText(QString::fromUtf8("新钥匙名称"));
    QPushButton* make = new QPushButton(QString::fromUtf8("生成"), &dlg);
    make->setProperty("primary", true);
    polishWidget(make);
    QPushButton* useBtn = new QPushButton(QString::fromUtf8("设为签发"), &dlg);
    QPushButton* removeBtn = new QPushButton(QString::fromUtf8("移除"), &dlg);
    QPushButton* copyBtn = new QPushButton(QString::fromUtf8("复制公钥"), &dlg);
    QLabel* status = new QLabel(&dlg);
    status->setWordWrap(true);
    const auto refill = [&]() {
        list->clear();
        const QVector<KeyRow> items = rows();
        for (int i = 0; i < items.size(); ++i) {
            const QString trust = items[i].trusted
                ? QString::fromUtf8("已信任")
                : QString::fromUtf8("待编译");
            const QString useText = items[i].active ? QString::fromUtf8("当前签发") : QString();
            QListWidgetItem* item = new QListWidgetItem(
                QString::fromUtf8("%1    %2    %3    %4")
                    .arg(items[i].id).arg(items[i].label, trust, useText));
            item->setData(Qt::UserRole, items[i].id);
            list->addItem(item);
        }
    };
    const auto selectedId = [&]() {
        QListWidgetItem* item = list->currentItem();
        return item ? item->data(Qt::UserRole).toInt() : 0;
    };
    QObject::connect(make, &QPushButton::clicked, &dlg, [&]() {
        status->setText(generate(name->text()));
        name->clear();
        refill();
    });
    QObject::connect(useBtn, &QPushButton::clicked, &dlg, [&]() {
        const int id = selectedId();
        if (id < 1) {
            status->setText(QString::fromUtf8("先选一把钥匙。"));
            return;
        }
        status->setText(use(id));
        refill();
    });
    QObject::connect(removeBtn, &QPushButton::clicked, &dlg, [&]() {
        const int id = selectedId();
        if (id < 1) {
            status->setText(QString::fromUtf8("先选一把钥匙。"));
            return;
        }
        const Entry* entry = find(id);
        const QString label = entry ? entry->label : QString::number(id);
        QMessageBox box(QMessageBox::Warning, QString::fromUtf8("移除钥匙"),
            QString::fromUtf8("从下一版公钥表移除 %1 号「%2」。客户升级到那一版后，这把钥匙签过的注册码会失效。")
                .arg(id).arg(label),
            QMessageBox::Ok | QMessageBox::Cancel, &dlg);
        applyLicenseWindowIcon(&box);
        if (box.exec() != QMessageBox::Ok) {
            return;
        }
        status->setText(remove(id));
        refill();
    });
    QObject::connect(copyBtn, &QPushButton::clicked, &dlg, [&]() {
        const QString hex = publicHex(selectedId());
        if (hex.isEmpty()) {
            status->setText(QString::fromUtf8("这把钥匙没有公钥。"));
            return;
        }
        QApplication::clipboard()->setText(hex);
        status->setText(QString::fromUtf8("已复制公钥。"));
    });
    QHBoxLayout* makeRow = new QHBoxLayout();
    makeRow->addWidget(name, 1);
    makeRow->addWidget(make);
    QHBoxLayout* actions = new QHBoxLayout();
    actions->addWidget(useBtn);
    actions->addWidget(removeBtn);
    actions->addWidget(copyBtn);
    actions->addStretch(1);
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 14, 16, 12);
    layout->setSpacing(10);
    layout->addWidget(lead);
    layout->addWidget(list, 1);
    layout->addLayout(makeRow);
    layout->addLayout(actions);
    layout->addWidget(status);
    refill();
    applyLicenseWindowIcon(&dlg);
    dlg.exec();
}

bool KeyVault::saveIndex() const
{
    std::ofstream out((m_dir + L"\\keys.txt").c_str(), std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const QByteArray label = m_entries[i].label.toUtf8();
        const QByteArray state = m_entries[i].state.toUtf8();
        out << m_entries[i].id << '\t' << label.constData() << '\t' << state.constData() << "\n";
    }
    return out.good();
}

void KeyVault::refresh()
{
    for (size_t i = 0; i < m_entries.size(); ++i) {
        m_entries[i].hasPub = publicFromPrivate(m_entries[i].priv, m_entries[i].pub);
        m_entries[i].trusted = m_entries[i].hasPub
            && Vision::LicenseSign::matches(m_entries[i].id, m_entries[i].pub, 72);
    }
}

std::wstring KeyVault::tablePath() const
{
    std::wstring dir = m_exeDir;
    for (int i = 0; i < 8 && !dir.empty(); ++i) {
        const std::wstring nested = dir + L"\\VisionLicense\\LicenseKeyTable.inc";
        const std::wstring here = dir + L"\\LicenseKeyTable.inc";
        if (fileExists(nested)) {
            return nested;
        }
        if (fileExists(here)) {
            return here;
        }
        dir = parentDir(dir);
    }
    return std::wstring();
}

bool KeyVault::writeTable(QString* error) const
{
    struct Item {
        int id;
        unsigned char pub[72];
    };
    std::vector<Item> items;
    const int baked = Vision::LicenseSign::count();
    for (int i = 0; i < baked; ++i) {
        Item item;
        int pubLen = 0;
        if (!Vision::LicenseSign::at(i, &item.id, item.pub, 72, &pubLen) || pubLen != 72) {
            continue;
        }
        const Entry* entry = find(item.id);
        if (entry && entry->state == QLatin1String("drop")) {
            continue;
        }
        items.push_back(item);
    }
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (!m_entries[i].hasPub || m_entries[i].state == QLatin1String("drop")) {
            continue;
        }
        bool found = false;
        for (size_t j = 0; j < items.size(); ++j) {
            if (items[j].id == m_entries[i].id) {
                memcpy(items[j].pub, m_entries[i].pub, 72);
                found = true;
                break;
            }
        }
        if (!found) {
            Item item;
            item.id = m_entries[i].id;
            memcpy(item.pub, m_entries[i].pub, 72);
            items.push_back(item);
        }
    }
    if (items.empty()) {
        if (error) {
            *error = QString::fromUtf8("公钥表不能是空的。");
        }
        return false;
    }
    for (size_t i = 0; i < items.size(); ++i) {
        for (size_t j = i + 1; j < items.size(); ++j) {
            if (items[j].id < items[i].id) {
                const Item tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }
    const std::wstring path = tablePath();
    if (path.empty()) {
        if (error) {
            *error = QString::fromUtf8("没有找到 LicenseKeyTable.inc，公钥还没写进源码。");
        }
        return false;
    }
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) {
            *error = QString::fromUtf8("公钥表源码没有写成功。");
        }
        return false;
    }
    out << "/**\n * @file LicenseKeyTable.inc\n * @brief 库内公钥表。只有公钥。注册机会重写本文件，重新编译后生效\n */\n\n";
    for (size_t i = 0; i < items.size(); ++i) {
        out << "static const unsigned char kPub_" << items[i].id << "[] = {\n    "
            << cBytes(items[i].pub, 72) << "\n};\n\n";
    }
    out << "static const Pub kPubs[] = {\n";
    for (size_t i = 0; i < items.size(); ++i) {
        out << "    { " << items[i].id << ", kPub_" << items[i].id
            << ", (int)sizeof(kPub_" << items[i].id << ") },\n";
    }
    out << "};\n";
    if (!out.good()) {
        if (error) {
            *error = QString::fromUtf8("公钥表源码没有写完整。");
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

KeyVault::Entry* KeyVault::find(int id)
{
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            return &m_entries[i];
        }
    }
    return 0;
}

const KeyVault::Entry* KeyVault::find(int id) const
{
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            return &m_entries[i];
        }
    }
    return 0;
}

int KeyVault::nextId() const
{
    int n = 0;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id > n) {
            n = m_entries[i].id;
        }
    }
    const int baked = Vision::LicenseSign::count();
    for (int i = 0; i < baked; ++i) {
        int id = 0;
        unsigned char pub[80];
        int pubLen = 0;
        if (Vision::LicenseSign::at(i, &id, pub, 80, &pubLen) && id > n) {
            n = id;
        }
    }
    return n + 1;
}
