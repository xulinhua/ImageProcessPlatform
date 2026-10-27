/**
 * @file Translate.cpp
 * @brief Translate实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Translate.h"
#include "TextEncoding.h"
#include <map>
#include <mutex>
#include <set>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

struct LangRow {
    std::string key; ///< 存盘键
    std::string native; ///< 语言本名
    std::string file; ///< 词条文件名；没有则为 -
};

LangId g_lang = LangZhCN;
std::string g_langKey = "zh-CN";
std::vector<LangRow> g_langs;
std::map<std::string, std::string> g_table;
std::map<std::string, std::string> g_utf8Keys;
bool g_listReady = false;
std::set<std::string> g_missing;
std::recursive_mutex g_mu;

struct Entry {
    const char* key; ///< 源语言字面量
    const char* en; ///< 英文译文
};

}

static const Entry kEn[] = {
#include "TranslateEn.inc"
    { 0, 0 }
};

// 是否简体中文键
static bool isZhKey(const std::string& key)
{
    return key.empty() || key == "zh" || key == "zh-CN";
}

// 当前模块所在目录
static std::wstring exeDirW()
{
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(0, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return std::wstring();
    }
    std::wstring path(buf, n);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return std::wstring();
    }
    return path.substr(0, slash);
}

// UTF-8 转宽字符，用来拼词条路径
static std::wstring langUtf8ToWide(const std::string& text)
{
    if (text.empty()) {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), 0, 0);
    if (n <= 0) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &out[0], n);
    return out;
}

// 读整个词条文件，并去掉 BOM
static bool readUtf8(const std::wstring& path, std::string* out)
{
    if (!out) {
        return false;
    }
    out->clear();
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    const DWORD size = GetFileSize(file, 0);
    if (size == INVALID_FILE_SIZE || size > 4u * 1024u * 1024u) {
        CloseHandle(file);
        return false;
    }
    std::string buf(size, '\0');
    DWORD got = 0;
    const BOOL ok = (size == 0) || ReadFile(file, &buf[0], size, &got, 0);
    CloseHandle(file);
    if (!ok) {
        return false;
    }
    buf.resize(got);
    if (buf.size() >= 3
        && static_cast<unsigned char>(buf[0]) == 0xEF
        && static_cast<unsigned char>(buf[1]) == 0xBB
        && static_cast<unsigned char>(buf[2]) == 0xBF) {
        buf.erase(0, 3);
    }
    *out = buf;
    return true;
}

// 把转义的换行、制表和反斜杠还原成字符
static std::string unescapeField(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            const char n = text[i + 1];
            if (n == 'n') {
                out.push_back('\n');
                ++i;
                continue;
            }
            if (n == 't') {
                out.push_back('\t');
                ++i;
                continue;
            }
            if (n == 'r') {
                out.push_back('\r');
                ++i;
                continue;
            }
            if (n == '\\') {
                out.push_back('\\');
                ++i;
                continue;
            }
        }
        out.push_back(text[i]);
    }
    return out;
}

// 按行切开，去掉行尾回车
static void splitLines(const std::string& text, std::vector<std::string>* lines)
{
    std::string cur;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            if (!cur.empty() && cur[cur.size() - 1] == '\r') {
                cur.erase(cur.size() - 1);
            }
            lines->push_back(cur);
            cur.clear();
        } else {
            cur.push_back(text[i]);
        }
    }
    if (!cur.empty()) {
        if (cur[cur.size() - 1] == '\r') {
            cur.erase(cur.size() - 1);
        }
        lines->push_back(cur);
    }
}

// 拼 exe 下 lang 目录中的词条路径
static std::wstring langFilePath(const std::string& name)
{
    const std::wstring dir = exeDirW();
    if (dir.empty() || name.empty() || name == "-") {
        return std::wstring();
    }
    return dir + L"\\lang\\" + langUtf8ToWide(name);
}

// 读语言列表；缺文件时只保留中英
static void loadLanguageList()
{
    if (g_listReady) {
        return;
    }
    g_listReady = true;
    g_langs.clear();
    std::string text;
    if (readUtf8(langFilePath("languages.txt"), &text)) {
        std::vector<std::string> lines;
        splitLines(text, &lines);
        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string& line = lines[i];
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const size_t tab1 = line.find('\t');
            if (tab1 == std::string::npos || tab1 == 0) {
                continue;
            }
            const size_t tab2 = line.find('\t', tab1 + 1);
            if (tab2 == std::string::npos) {
                continue;
            }
            LangRow row;
            row.key = line.substr(0, tab1);
            row.native = unescapeField(line.substr(tab1 + 1, tab2 - tab1 - 1));
            row.file = line.substr(tab2 + 1);
            if (!row.key.empty()) {
                g_langs.push_back(row);
            }
        }
    }
    if (g_langs.empty()) {
        LangRow zh;
        zh.key = "zh-CN";
        zh.native = "简体中文";
        zh.file = "-";
        LangRow en;
        en.key = "en-US";
        en.native = "English";
        en.file = "en-US.txt";
        g_langs.push_back(zh);
        g_langs.push_back(en);
    }
}

// 按存盘键找语言行
static const LangRow* findLang(const std::string& key)
{
    for (size_t i = 0; i < g_langs.size(); ++i) {
        if (g_langs[i].key == key) {
            return &g_langs[i];
        }
    }
    return 0;
}

// 用词条文件覆盖内置译文
static void overlayFile(const std::string& fileName)
{
    std::string text;
    const std::wstring path = langFilePath(fileName);
    if (path.empty() || !readUtf8(path, &text)) {
        return;
    }
    std::vector<std::string> lines;
    splitLines(text, &lines);
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || tab == 0) {
            continue;
        }
        const std::string key = unescapeField(line.substr(0, tab));
        const std::string value = unescapeField(line.substr(tab + 1));
        if (!key.empty()) {
            g_table[key] = value;
        }
    }
}

// 按当前语言重建查找表
static void rebuildTable()
{
    g_table.clear();
    if (isZhKey(g_langKey)) {
        g_lang = LangZhCN;
        return;
    }
    g_lang = (g_langKey == "en" || g_langKey == "en-US") ? LangEnUS : LangZhCN;
    if (g_lang == LangEnUS) {
        for (int i = 0; kEn[i].key != 0; ++i) {
            g_table[kEn[i].key] = kEn[i].en ? kEn[i].en : "";
        }
    }
    std::string fileName;
    if (g_langKey == "en") {
        fileName = "en-US.txt";
    }
    const LangRow* row = findLang(g_langKey == "en" ? "en-US" : g_langKey);
    if (row && !row->file.empty() && row->file != "-") {
        fileName = row->file;
    }
    overlayFile(fileName);
}

// 把花括号占位换成对应参数
static std::string applyArgs(const std::string& text, const std::vector<std::string>& args)
{
    std::string out;
    out.reserve(text.size() + 16);
    for (size_t i = 0; i < text.size(); ) {
        if (text[i] == '{') {
            size_t j = i + 1;
            size_t n = 0;
            bool digits = false;
            while (j < text.size() && text[j] >= '0' && text[j] <= '9') {
                digits = true;
                n = n * 10 + static_cast<size_t>(text[j] - '0');
                ++j;
            }
            if (digits && j < text.size() && text[j] == '}') {
                if (n < args.size()) {
                    out += args[n];
                } else {
                    out.append(text, i, j - i + 1);
                }
                i = j + 1;
                continue;
            }
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

#ifdef _DEBUG
// 把缺词条记到 lang 目录的 missing.txt
static void noteMissing(const std::string& key)
{
    if (key.empty() || g_missing.find(key) != g_missing.end()) {
        return;
    }
    g_missing.insert(key);
    const std::wstring dir = exeDirW();
    if (dir.empty()) {
        return;
    }
    const std::wstring folder = dir + L"\\lang";
    CreateDirectoryW(folder.c_str(), 0);
    std::string body;
    for (std::set<std::string>::const_iterator it = g_missing.begin(); it != g_missing.end(); ++it) {
        body += *it;
        body += "\r\n";
    }
    const HANDLE file = CreateFileW((folder + L"\\missing.txt").c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                    0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD wrote = 0;
    if (!body.empty()) {
        WriteFile(file, body.data(), static_cast<DWORD>(body.size()), &wrote, 0);
    }
    CloseHandle(file);
}
#endif

// 把源文件字面量从GBK转成UTF-8并缓存
static const char* sourceAsUtf8(const char* key)
{
    bool high = false;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(key); *p; ++p) {
        if (*p >= 0x80) {
            high = true;
            break;
        }
    }
    if (!high) {
        return key;
    }
    const std::string raw(key);
    std::map<std::string, std::string>::iterator it = g_utf8Keys.find(raw);
    if (it == g_utf8Keys.end()) {
        it = g_utf8Keys.insert(std::make_pair(raw, Vision::acpToUtf8(raw))).first;
    }
    return it->second.c_str();
}

// 按当前语言取译文副本。源语言返回键的 UTF-8，其它语种查词条表
static std::string lookup(const char* key)
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    if (!key || key[0] == 0) {
        return std::string();
    }
    loadLanguageList();
    const char* utf8 = sourceAsUtf8(key);
    const std::string utf8Key = utf8 ? std::string(utf8) : std::string();
    if (isZhKey(g_langKey)) {
        return utf8Key;
    }
    std::map<std::string, std::string>::const_iterator it = g_table.find(utf8Key);
    if (it == g_table.end()) {
        it = g_table.find(key);
    }
    if (it == g_table.end()) {
#ifdef _DEBUG
        noteMissing(key);
#endif
        return utf8Key;
    }
    return it->second;
}

// 切到中文或英文并重建词条表
void Translate::setLanguage(LangId lang)
{
    setLanguageKey(lang == LangEnUS ? "en-US" : "zh-CN");
}

// 按存盘键切换；不认识则回到中文
bool Translate::setLanguageKey(const std::string& key)
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    loadLanguageList();
    std::string canon = key;
    if (key == "en") {
        canon = "en-US";
    } else if (key == "zh") {
        canon = "zh-CN";
    }
    const bool known = isZhKey(canon) || findLang(canon) != 0;
    if (!known) {
        g_langKey = "zh-CN";
    } else if (canon.empty()) {
        g_langKey = "zh-CN";
    } else {
        g_langKey = canon;
    }
    rebuildTable();
    return known;
}

// 当前语言枚举
LangId Translate::language()
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    loadLanguageList();
    return g_lang;
}

// 当前存盘键
std::string Translate::languageKey()
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    loadLanguageList();
    return g_langKey.c_str();
}

// 语言种数
int Translate::languageCount()
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    loadLanguageList();
    return static_cast<int>(g_langs.size());
}

// 按下标取语言；越界或非英文回中文
LangId Translate::languageAt(int i)
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    loadLanguageList();
    if (i >= 0 && i < static_cast<int>(g_langs.size()) && g_langs[static_cast<size_t>(i)].key == "en-US") {
        return LangEnUS;
    }
    return LangZhCN;
}

// 按下标取存盘键；越界空串
std::string Translate::languageKeyAt(int i)
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    loadLanguageList();
    if (i < 0 || i >= static_cast<int>(g_langs.size())) {
        return "";
    }
    return g_langs[static_cast<size_t>(i)].key.c_str();
}

// 枚举对应的存盘键
std::string Translate::languageKeyOf(LangId lang)
{
    return (lang == LangEnUS) ? "en-US" : "zh-CN";
}

// 语言本名，不随界面语言变
std::string Translate::languageNativeName(LangId lang)
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    loadLanguageList();
    const char* want = (lang == LangEnUS) ? "en-US" : "zh-CN";
    for (size_t i = 0; i < g_langs.size(); ++i) {
        if (g_langs[i].key == want) {
            return g_langs[i].native.c_str();
        }
    }
    return (lang == LangEnUS) ? "English" : "简体中文";
}

// 按下标取语言本名；越界空串
std::string Translate::languageNativeNameAt(int i)
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    loadLanguageList();
    if (i < 0 || i >= static_cast<int>(g_langs.size())) {
        return "";
    }
    return g_langs[static_cast<size_t>(i)].native.c_str();
}

// 取当前语言文本
std::string Translate::tr(const char* key)
{
    return lookup(key);
}

// 取译文并替换全部占位
std::string Translate::format(const char* key, const std::vector<std::string>& args)
{
    return applyArgs(lookup(key), args);
}

// 用一个参数替换占位
std::string Translate::format(const char* key, const std::string& a0)
{
    std::vector<std::string> args;
    args.push_back(a0);
    return format(key, args);
}

// 用两个参数替换占位
std::string Translate::format(const char* key, const std::string& a0, const std::string& a1)
{
    std::vector<std::string> args;
    args.push_back(a0);
    args.push_back(a1);
    return format(key, args);
}

// 用三个参数替换占位
std::string Translate::format(const char* key, const std::string& a0, const std::string& a1, const std::string& a2)
{
    std::vector<std::string> args;
    args.push_back(a0);
    args.push_back(a1);
    args.push_back(a2);
    return format(key, args);
}

}
