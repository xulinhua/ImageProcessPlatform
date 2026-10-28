/**
 * @file FileDocument.cpp
 * @brief FileDocument实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FileDocument.h"
#include "TextEncoding.h"
#include <cstdio>
#include <ctime>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

// 写入错误文本；error 为空则忽略
void setErr(std::string* error, const std::string& msg)
{
    if (error) {
        *error = msg;
    }
}

// 按路径逐级建目录
bool mkdirDeep(const std::string& dir)
{
    if (dir.empty()) {
        return false;
    }
    std::string cur;
    for (size_t i = 0; i < dir.size(); ++i) {
        cur.push_back(dir[i]);
        if (dir[i] == '\\' || dir[i] == '/' || i + 1 == dir.size()) {
            if (cur.size() >= 2) {
                CreateDirectoryW(utf8ToWide(cur).c_str(), 0);
            }
        }
    }
    return true;
}

// 去掉最后一段，得到父目录
std::string parentDir(const std::string& path)
{
    const size_t s = path.find_last_of("\\/");
    if (s == std::string::npos) {
        return std::string();
    }
    return path.substr(0, s);
}

} // namespace

// 目录不存在则逐级创建
bool FileUtil::ensureDir(const std::string& dir, std::string* error)
{
    if (dir.empty()) {
        setErr(error, "empty dir");
        return false;
    }
    mkdirDeep(dir);
    const DWORD a = GetFileAttributesW(utf8ToWide(dir).c_str());
    if (a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        setErr(error, "mkdir failed");
        return false;
    }
    return true;
}

// 先写临时文件再替换，避免半截内容
bool FileUtil::writeAtomic(const std::string& path, const std::string& bytes, std::string* error)
{
    const std::string dir = parentDir(path);
    if (!dir.empty() && !ensureDir(dir, error)) {
        return false;
    }
    const std::string tmp = path + ".tmp";
    FILE* f = _wfopen(utf8ToWide(tmp).c_str(), L"wb");
    if (!f) {
        setErr(error, "open tmp failed");
        return false;
    }
    if (!bytes.empty()) {
        fwrite(bytes.data(), 1, bytes.size(), f);
    }
    fclose(f);
    if (!MoveFileExW(utf8ToWide(tmp).c_str(), utf8ToWide(path).c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(utf8ToWide(tmp).c_str());
        setErr(error, "rename failed");
        return false;
    }
    return true;
}

// 按最后写入时间删掉过期文件
int FileUtil::deleteOlderThanDays(const std::string& dir, const std::string& pattern, int days)
{
    if (days <= 0) {
        return 0;
    }
    const std::wstring pat = utf8ToWide(dir + "\\" + (pattern.empty() ? std::string("*.*") : pattern));
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER n;
    n.LowPart = now.dwLowDateTime;
    n.HighPart = now.dwHighDateTime;
    const ULONGLONG cut = n.QuadPart - (static_cast<ULONGLONG>(days) * 24ULL * 3600ULL * 10000000ULL);
    int del = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        ULARGE_INTEGER t;
        t.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        if (t.QuadPart < cut) {
            const std::string name = wideToUtf8(fd.cFileName);
            if (DeleteFileW(utf8ToWide(dir + "\\" + name).c_str())) {
                ++del;
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return del;
}

// 覆盖写出 UTF-8 文本
bool TxtDocument::writeText(const std::string& path, const std::string& utf8, bool withBom, std::string* error)
{
    std::string body;
    if (withBom) {
        body.append("\xEF\xBB\xBF", 3);
    }
    body += utf8;
    return FileUtil::writeAtomic(path, body, error);
}

// 在文件末尾追加 UTF-8 文本
bool TxtDocument::appendText(const std::string& path, const std::string& utf8, std::string* error)
{
    const std::string dir = parentDir(path);
    if (!dir.empty() && !FileUtil::ensureDir(dir, error)) {
        return false;
    }
    FILE* f = _wfopen(utf8ToWide(path).c_str(), L"ab");
    if (!f) {
        setErr(error, "append open failed");
        return false;
    }
    if (!utf8.empty()) {
        fwrite(utf8.data(), 1, utf8.size(), f);
    }
    fclose(f);
    return true;
}

// 记下文本表的列名
void TxtDocument::setHeaders(const std::vector<std::string>& headers)
{
    m_headers = headers;
}

// 缓存一行单元格
void TxtDocument::addRow(const std::vector<std::string>& cells)
{
    m_rows.push_back(cells);
}

// 按制表符写出，并带 UTF-8 BOM
bool TxtDocument::save(const std::string& path, std::string* error)
{
    std::string s;
    if (!m_headers.empty()) {
        for (size_t i = 0; i < m_headers.size(); ++i) {
            if (i) {
                s += "\t";
            }
            s += m_headers[i];
        }
        s += "\r\n";
    }
    for (size_t r = 0; r < m_rows.size(); ++r) {
        const std::vector<std::string>& row = m_rows[r];
        for (size_t i = 0; i < row.size(); ++i) {
            if (i) {
                s += "\t";
            }
            s += row[i];
        }
        s += "\r\n";
    }
    return writeText(path, s, true, error);
}

// 含逗号、引号或换行时按 CSV 规则加引号
std::string CsvDocument::escapeCell(const std::string& s)
{
    bool need = false;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '"' || c == ',' || c == '\n' || c == '\r') {
            need = true;
            break;
        }
    }
    if (!need) {
        return s;
    }
    std::string o;
    o.push_back('"');
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"') {
            o += "\"\"";
        } else {
            o.push_back(s[i]);
        }
    }
    o.push_back('"');
    return o;
}

// 记下 CSV 列名
void CsvDocument::setHeaders(const std::vector<std::string>& headers)
{
    m_headers = headers;
}

// 缓存一行单元格
void CsvDocument::addRow(const std::vector<std::string>& cells)
{
    m_rows.push_back(cells);
}

// 按逗号写出 CSV
bool CsvDocument::save(const std::string& path, std::string* error)
{
    std::string s;
    if (!m_headers.empty()) {
        for (size_t i = 0; i < m_headers.size(); ++i) {
            if (i) {
                s += ",";
            }
            s += escapeCell(m_headers[i]);
        }
        s += "\r\n";
    }
    for (size_t r = 0; r < m_rows.size(); ++r) {
        const std::vector<std::string>& row = m_rows[r];
        for (size_t i = 0; i < row.size(); ++i) {
            if (i) {
                s += ",";
            }
            s += escapeCell(row[i]);
        }
        s += "\r\n";
    }
    return TxtDocument::writeText(path, s, true, error);
}

} // namespace Vision
