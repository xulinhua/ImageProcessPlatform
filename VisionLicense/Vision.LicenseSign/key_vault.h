/**
 * @file key_vault.h
 * @brief 注册机旁边的私钥表。公钥表写回源码，重新编译后客户程序才信任
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LICENSE_SIGN_KEY_VAULT_H
#define VISION_LICENSE_SIGN_KEY_VAULT_H

#include <QString>
#include <QVector>
#include <string>
#include <vector>

class QWidget;

/** @brief 一把本地私钥在列表里的样子 */
struct KeyRow {
    int id;            ///< 钥匙编号，和库内公钥表一致
    QString label;     ///< 名称
    bool trusted;      ///< 当前这个程序的公钥表里已经有它
    bool active;       ///< 正在用来签发
};

/**
 * @class KeyVault
 * @brief 读写 exe 旁 keys 目录，并维护下一版要编进库的公钥表
 */
class KeyVault {
public:
    /** @brief 尚未读取 */
    KeyVault();

    /** @brief 读取 keys 目录。目录是空的且旁边有 sign.key 时，导入为 1 号 */
    bool load(const std::wstring& exeDirectory);
    /** @brief 当前用来签发的编号。没有则是 0 */
    int activeId() const;
    /** @brief 当前签发钥匙的名称 */
    QString activeLabel() const;
    /** @brief 复制当前签发私钥。没有返回 false */
    bool copyActive(std::vector<unsigned char>* key) const;
    /** @brief 当前签发钥匙的公钥是否已经在这个程序里 */
    bool activeTrusted() const;
    /** @brief 列表里还在用的钥匙，不含已移除的 */
    QVector<KeyRow> rows() const;
    /** @brief 生成一把新钥匙并写入公钥表源码。返回给界面的说明 */
    QString generate(const QString& label);
    /** @brief 改用这把已信任的钥匙签发 */
    QString use(int id);
    /** @brief 下一版公钥表去掉这把。已经发出的码在客户升级后会失效 */
    QString remove(int id);
    /** @brief 这把钥匙的公钥十六进制。没有返回空 */
    QString publicHex(int id) const;
    /** @brief 打开密钥窗口，改完就返回 */
    void manage(QWidget* parent);

private:
    struct Entry {
        int id;
        QString label;
        QString state;                         ///< sign、keep、drop
        std::vector<unsigned char> priv;
        unsigned char pub[72];
        bool hasPub;
        bool trusted;
    };

    /** @brief 把内存里的目录写回 keys.txt */
    bool saveIndex() const;
    /** @brief 按私钥刷新公钥，并对照当前程序里的公钥表 */
    void refresh();
    /** @brief 下一版公钥表的源码路径。找不到返回空 */
    std::wstring tablePath() const;
    /** @brief 用未移除的钥匙重写公钥表源码 */
    bool writeTable(QString* error) const;
    /** @brief 编号对应的记录。没有返回空 */
    Entry* find(int id);
    const Entry* find(int id) const;
    /** @brief 现有编号和库内编号的下一个 */
    int nextId() const;

    std::wstring m_dir;             ///< keys 目录
    std::wstring m_exeDir;          ///< exe 目录，用来找 sign.key 和源码
    std::vector<Entry> m_entries;
};

#endif
