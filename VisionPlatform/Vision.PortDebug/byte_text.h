/**
 * @file byte_text.h
 * @brief 文本和 HEX 互转，两个区域的收发框共用
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HS_COMM_DEBUG_BYTE_TEXT_H
#define HS_COMM_DEBUG_BYTE_TEXT_H

#include <QByteArray>
#include <QDateTime>
#include <QString>

/** @brief 把字节显示成 HEX，或把空格分隔的 HEX 解析回去 */
class ByteText {
public:
    static QString stamp()
    {
        return QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    }

    static QString toHex(const QByteArray& bytes)
    {
        QString out;
        for (int i = 0; i < bytes.size(); ++i) {
            if (i) {
                out += ' ';
            }
            out += QString("%1").arg((unsigned char)bytes[i], 2, 16, QChar('0')).toUpper();
        }
        return out;
    }

    /** @brief 允许空格。非法字符时失败，不返回半截 */
    static bool fromHex(const QString& text, QByteArray* out, QString* err)
    {
        if (!out) {
            return false;
        }
        out->clear();
        QString compact = text;
        compact.remove(' ');
        compact.remove('\n');
        compact.remove('\r');
        compact.remove('\t');
        if (compact.isEmpty()) {
            return true;
        }
        if (compact.size() % 2 != 0) {
            if (err) {
                *err = QString::fromUtf8("HEX 位数必须是偶数，例如 01 03 或 0103");
            }
            return false;
        }
        for (int i = 0; i < compact.size(); i += 2) {
            bool ok = false;
            const int v = compact.mid(i, 2).toInt(&ok, 16);
            if (!ok) {
                if (err) {
                    *err = QString::fromUtf8("无法解析 HEX：") + compact.mid(i, 2);
                }
                return false;
            }
            out->append((char)v);
        }
        return true;
    }

    /**
     * @brief 文本里的 \r \n \t \\ \0 \xHH 换成对应字节。非法的 \x 保留原样
     */
    static QString unescape(const QString& text)
    {
        QString out;
        for (int i = 0; i < text.size(); ++i) {
            if (text[i] != '\\' || i + 1 >= text.size()) {
                out += text[i];
                continue;
            }
            const QChar n = text[i + 1];
            if (n == 'r') {
                out += '\r';
            } else if (n == 'n') {
                out += '\n';
            } else if (n == 't') {
                out += '\t';
            } else if (n == '0') {
                out += QChar(0);
            } else if (n == '\\') {
                out += '\\';
            } else if ((n == 'x' || n == 'X') && i + 3 < text.size()) {
                bool ok = false;
                const int v = text.mid(i + 2, 2).toInt(&ok, 16);
                if (!ok) {
                    out += text[i];
                    continue;
                }
                out += QChar(v);
                i += 2;
            } else {
                out += text[i];
                continue;
            }
            ++i;
        }
        return out;
    }

    /** @brief 0 不加，1 累加和，2 异或，3 Modbus CRC16（低字节在前） */
    static void appendCheck(QByteArray* bytes, int kind)
    {
        if (!bytes || bytes->isEmpty() || kind <= 0) {
            return;
        }
        if (kind == 1) {
            unsigned sum = 0;
            for (int i = 0; i < bytes->size(); ++i) {
                sum += (unsigned char)(*bytes)[i];
            }
            bytes->append((char)(sum & 0xFF));
            return;
        }
        if (kind == 2) {
            unsigned char x = 0;
            for (int i = 0; i < bytes->size(); ++i) {
                x = (unsigned char)(x ^ (unsigned char)(*bytes)[i]);
            }
            bytes->append((char)x);
            return;
        }
        unsigned crc = 0xFFFF;
        for (int i = 0; i < bytes->size(); ++i) {
            crc ^= (unsigned char)(*bytes)[i];
            for (int b = 0; b < 8; ++b) {
                crc = (crc & 1u) ? ((crc >> 1) ^ 0xA001u) : (crc >> 1);
            }
        }
        bytes->append((char)(crc & 0xFF));
        bytes->append((char)((crc >> 8) & 0xFF));
    }
};

#endif
