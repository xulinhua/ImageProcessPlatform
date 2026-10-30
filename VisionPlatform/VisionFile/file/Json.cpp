/**
 * @file Json.cpp
 * @brief Json实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Json.h"
#include <cstdio>
#include <cstdlib>

namespace Vision {

// 空值：Null、假、0
JsonValue::JsonValue()
    : m_type(Null)
    , m_bool(false)
    , m_num(0)
{
}

// 布尔值，数字槽位同步为 0/1
JsonValue::JsonValue(bool v)
    : m_type(Bool)
    , m_bool(v)
    , m_num(v ? 1.0 : 0.0)
{
}

// 整数存为 Number
JsonValue::JsonValue(int v)
    : m_type(Number)
    , m_bool(v != 0)
    , m_num(static_cast<double>(v))
{
}

// 浮点 Number
JsonValue::JsonValue(double v)
    : m_type(Number)
    , m_bool(v != 0.0)
    , m_num(v)
{
}

// C 字符串；空指针当空串
JsonValue::JsonValue(const char* v)
    : m_type(String)
    , m_bool(false)
    , m_num(0)
    , m_str(v ? v : "")
{
}

// std::string
JsonValue::JsonValue(const std::string& v)
    : m_type(String)
    , m_bool(false)
    , m_num(0)
    , m_str(v)
{
}

// 按类型转布尔；Null 用默认值
bool JsonValue::toBool(bool def) const
{
    if (m_type == Null) {
        return def;
    }
    if (m_type == Bool) {
        return m_bool;
    }
    if (m_type == Number) {
        return m_num != 0.0;
    }
    if (m_type == String) {
        return m_str == "1" || m_str == "true" || m_str == "True";
    }
    return def;
}

// 按类型转 int；字符串走 atoi
int JsonValue::toInt(int def) const
{
    if (m_type == Null) {
        return def;
    }
    if (m_type == Number) {
        return static_cast<int>(m_num);
    }
    if (m_type == Bool) {
        return m_bool ? 1 : 0;
    }
    if (m_type == String) {
        return atoi(m_str.c_str());
    }
    return def;
}

// 按类型转 double；字符串走 atof
double JsonValue::toDouble(double def) const
{
    if (m_type == Null) {
        return def;
    }
    if (m_type == Number) {
        return m_num;
    }
    if (m_type == Bool) {
        return m_bool ? 1.0 : 0.0;
    }
    if (m_type == String) {
        return atof(m_str.c_str());
    }
    return def;
}

// 字符串原样；布尔 true/false；整数无小数否则 %.17g
std::string JsonValue::toString() const
{
    if (m_type == String) {
        return m_str;
    }
    if (m_type == Bool) {
        return m_bool ? "true" : "false";
    }
    if (m_type == Number) {
        char buf[64];
        if (m_num == static_cast<double>(static_cast<int>(m_num))
            && m_num >= -2147483647.0 && m_num <= 2147483647.0) {
            sprintf(buf, "%d", static_cast<int>(m_num));
        } else {
            sprintf(buf, "%.17g", m_num);
        }
        return std::string(buf);
    }
    return std::string();
}

// 数组长度或对象键数
int JsonValue::size() const
{
    if (m_type == Array) {
        return static_cast<int>(m_arr.size());
    }
    if (m_type == Object) {
        return static_cast<int>(m_obj.size());
    }
    return 0;
}

// 数组下标；越界返回 Null
JsonValue JsonValue::at(int i) const
{
    if (m_type != Array || i < 0 || i >= static_cast<int>(m_arr.size())) {
        return JsonValue();
    }
    return m_arr[static_cast<size_t>(i)];
}

// 清空为数组
void JsonValue::makeArray()
{
    m_type = Array;
    m_arr.clear();
    m_obj.clear();
    m_str.clear();
}

// 清空为对象
void JsonValue::makeObject()
{
    m_type = Object;
    m_arr.clear();
    m_obj.clear();
    m_str.clear();
}

// 追加到数组；非数组则先转数组
void JsonValue::append(const JsonValue& v)
{
    if (m_type != Array) {
        makeArray();
    }
    m_arr.push_back(v);
}

// 对象是否含该键
bool JsonValue::contains(const std::string& key) const
{
    if (m_type != Object) {
        return false;
    }
    return m_obj.find(key) != m_obj.end();
}

// 取对象字段；缺失返回 Null
JsonValue JsonValue::value(const std::string& key) const
{
    if (m_type != Object) {
        return JsonValue();
    }
    std::map<std::string, JsonValue>::const_iterator it = m_obj.find(key);
    if (it == m_obj.end()) {
        return JsonValue();
    }
    return it->second;
}

// 写对象字段；非对象则先转对象
void JsonValue::set(const std::string& key, const JsonValue& v)
{
    if (m_type != Object) {
        makeObject();
    }
    m_obj[key] = v;
}

// 对象字段转浮点；缺键用默认值
double JsonValue::number(const std::string& key, double def) const
{
    if (!contains(key)) {
        return def;
    }
    return value(key).toDouble(def);
}

// 对象字段转整数；缺键用默认值
int JsonValue::integerOf(const std::string& key, int def) const
{
    if (!contains(key)) {
        return def;
    }
    return value(key).toInt(def);
}

// 对象字段转布尔；缺键用默认值
bool JsonValue::flagOf(const std::string& key, bool def) const
{
    if (!contains(key)) {
        return def;
    }
    return value(key).toBool(def);
}

// 对象字段转文本；缺键为空串
std::string JsonValue::stringOf(const std::string& key) const
{
    if (!contains(key)) {
        return std::string();
    }
    return value(key).toString();
}

struct JsonParser {
    const std::string& t;
    size_t i;
    std::string* err;

    // 从文本起点解析，错误写入 err
    JsonParser(const std::string& text, std::string* e)
        : t(text)
        , i(0)
        , err(e)
    {
    }

    // 只记第一条错误
    void fail(const char* m)
    {
        if (err && err->empty()) {
            *err = m;
        }
    }

    // 跳过空白
    void skipWs()
    {
        while (i < t.size()) {
            const unsigned char c = static_cast<unsigned char>(t[i]);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++i;
            } else {
                break;
            }
        }
    }

    // 跳空白后若匹配 c 则吞掉
    bool eat(char c)
    {
        skipWs();
        if (i < t.size() && t[i] == c) {
            ++i;
            return true;
        }
        return false;
    }

    // 按首字符分派值类型
    JsonValue parseValue()
    {
        skipWs();
        if (i >= t.size()) {
            fail("unexpected end");
            return JsonValue();
        }
        const char c = t[i];
        if (c == '{') {
            return parseObject();
        }
        if (c == '[') {
            return parseArray();
        }
        if (c == '"') {
            return parseString();
        }
        if (c == 't' || c == 'f') {
            return parseBool();
        }
        if (c == 'n') {
            return parseNull();
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return parseNumber();
        }
        fail("unexpected token");
        return JsonValue();
    }

    // 字面量 null
    JsonValue parseNull()
    {
        if (t.compare(i, 4, "null") == 0) {
            i += 4;
            return JsonValue();
        }
        fail("expected null");
        return JsonValue();
    }

    // 字面量 true/false
    JsonValue parseBool()
    {
        if (t.compare(i, 4, "true") == 0) {
            i += 4;
            return JsonValue(true);
        }
        if (t.compare(i, 5, "false") == 0) {
            i += 5;
            return JsonValue(false);
        }
        fail("expected bool");
        return JsonValue();
    }

    // 扫描整数/小数/指数后 atof
    JsonValue parseNumber()
    {
        const size_t start = i;
        if (i < t.size() && t[i] == '-') {
            ++i;
        }
        while (i < t.size() && t[i] >= '0' && t[i] <= '9') {
            ++i;
        }
        if (i < t.size() && t[i] == '.') {
            ++i;
            while (i < t.size() && t[i] >= '0' && t[i] <= '9') {
                ++i;
            }
        }
        if (i < t.size() && (t[i] == 'e' || t[i] == 'E')) {
            ++i;
            if (i < t.size() && (t[i] == '+' || t[i] == '-')) {
                ++i;
            }
            while (i < t.size() && t[i] >= '0' && t[i] <= '9') {
                ++i;
            }
        }
        return JsonValue(atof(t.substr(start, i - start).c_str()));
    }

    // 码点写成 UTF-8
    static void appendUtf8(std::string* o, unsigned int cp)
    {
        if (cp <= 0x7F) {
            o->push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            o->push_back(static_cast<char>(0xC0 | (cp >> 6)));
            o->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            o->push_back(static_cast<char>(0xE0 | (cp >> 12)));
            o->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            o->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            o->push_back(static_cast<char>(0xF0 | (cp >> 18)));
            o->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            o->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            o->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    // 读 4 位十六进制
    int hex4()
    {
        int v = 0;
        for (int n = 0; n < 4; ++n) {
            if (i >= t.size()) {
                return -1;
            }
            const char c = t[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') {
                v += c - '0';
            } else if (c >= 'a' && c <= 'f') {
                v += c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                v += c - 'A' + 10;
            } else {
                return -1;
            }
        }
        return v;
    }

    // 解析带转义的 JSON 字符串
    JsonValue parseString()
    {
        if (!eat('"')) {
            fail("expected string");
            return JsonValue();
        }
        std::string o;
        while (i < t.size()) {
            const char c = t[i++];
            if (c == '"') {
                // 源码与磁盘 JSON 均为 UTF-8，内存不再转系统 ANSI
                return JsonValue(o);
            }
            if (c != '\\') {
                o.push_back(c);
                continue;
            }
            if (i >= t.size()) {
                break;
            }
            const char e = t[i++];
            if (e == '"' || e == '\\' || e == '/') {
                o.push_back(e);
            } else if (e == 'b') {
                o.push_back('\b');
            } else if (e == 'f') {
                o.push_back('\f');
            } else if (e == 'n') {
                o.push_back('\n');
            } else if (e == 'r') {
                o.push_back('\r');
            } else if (e == 't') {
                o.push_back('\t');
            } else if (e == 'u') {
                const int cp = hex4();
                if (cp < 0) {
                    fail("bad unicode");
                    return JsonValue();
                }
                appendUtf8(&o, static_cast<unsigned int>(cp));
            } else {
                o.push_back(e);
            }
        }
        fail("unterminated string");
        return JsonValue();
    }

    // 解析数组
    JsonValue parseArray()
    {
        if (!eat('[')) {
            fail("expected array");
            return JsonValue();
        }
        JsonValue arr;
        arr.makeArray();
        skipWs();
        if (eat(']')) {
            return arr;
        }
        while (true) {
            arr.append(parseValue());
            skipWs();
            if (eat(']')) {
                return arr;
            }
            if (!eat(',')) {
                fail("expected comma");
                return arr;
            }
        }
    }

    // 解析对象
    JsonValue parseObject()
    {
        if (!eat('{')) {
            fail("expected object");
            return JsonValue();
        }
        JsonValue obj;
        obj.makeObject();
        skipWs();
        if (eat('}')) {
            return obj;
        }
        while (true) {
            skipWs();
            const JsonValue key = parseString();
            skipWs();
            if (!eat(':')) {
                fail("expected colon");
                return obj;
            }
            obj.set(key.toString(), parseValue());
            skipWs();
            if (eat('}')) {
                return obj;
            }
            if (!eat(',')) {
                fail("expected comma");
                return obj;
            }
        }
    }
};

// 解析整段 JSON；尾随内容记错误但仍返回已解析值
JsonValue JsonValue::parse(const std::string& text, std::string* error)
{
    if (error) {
        error->clear();
    }
    JsonParser p(text, error);
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEF
        && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF) {
        p.i = 3;
    }
    JsonValue v = p.parseValue();
    p.skipWs();
    if (p.i != text.size() && error && error->empty()) {
        *error = "trailing data";
    }
    return v;
}

// 写成 JSON 字符串字面量（内存已是 UTF-8）
static void escapeTo(std::string* o, const std::string& s)
{
    const std::string& u = s;
    o->push_back('"');
    for (size_t i = 0; i < u.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(u[i]);
        if (c == '"') {
            o->append("\\\"");
        } else if (c == '\\') {
            o->append("\\\\");
        } else if (c == '\n') {
            o->append("\\n");
        } else if (c == '\r') {
            o->append("\\r");
        } else if (c == '\t') {
            o->append("\\t");
        } else if (c < 0x20) {
            char buf[8];
            sprintf(buf, "\\u%04x", c);
            o->append(buf);
        } else {
            o->push_back(static_cast<char>(c));
        }
    }
    o->push_back('"');
}

// 递归序列化；pretty 时按 indent 缩进
static void dump(const JsonValue& v, std::string* o, bool pretty, int indent)
{
    switch (v.type()) {
    case JsonValue::Null:
        o->append("null");
        break;
    case JsonValue::Bool:
        o->append(v.toBool() ? "true" : "false");
        break;
    case JsonValue::Number:
        o->append(v.toString());
        break;
    case JsonValue::String:
        escapeTo(o, v.toString());
        break;
    case JsonValue::Array: {
        o->push_back('[');
        const int n = v.size();
        for (int i = 0; i < n; ++i) {
            if (pretty) {
                o->push_back('\n');
                o->append(static_cast<size_t>(indent + 2), ' ');
            }
            dump(v.at(i), o, pretty, indent + 2);
            if (i + 1 < n) {
                o->push_back(',');
            }
        }
        if (pretty && n > 0) {
            o->push_back('\n');
            o->append(static_cast<size_t>(indent), ' ');
        }
        o->push_back(']');
        break;
    }
    case JsonValue::Object: {
        // 对象键按 std::map 顺序（字典序）输出
        o->push_back('{');
        const std::map<std::string, JsonValue>& m = v.object();
        int i = 0;
        const int n = static_cast<int>(m.size());
        for (std::map<std::string, JsonValue>::const_iterator it = m.begin(); it != m.end(); ++it, ++i) {
            if (pretty) {
                o->push_back('\n');
                o->append(static_cast<size_t>(indent + 2), ' ');
            }
            escapeTo(o, it->first);
            o->append(pretty ? ": " : ":");
            dump(it->second, o, pretty, indent + 2);
            if (i + 1 < n) {
                o->push_back(',');
            }
        }
        if (pretty && n > 0) {
            o->push_back('\n');
            o->append(static_cast<size_t>(indent), ' ');
        }
        o->push_back('}');
        break;
    }
    }
}

// 序列化；pretty 时末尾换行
std::string JsonValue::stringify(const JsonValue& v, bool pretty)
{
    std::string o;
    dump(v, &o, pretty, 0);
    if (pretty) {
        o.push_back('\n');
    }
    return o;
}

// 当前种类
JsonValue::Type JsonValue::type() const
{
    return m_type;
}

// 是否为空
bool JsonValue::isNull() const
{
    return m_type == Null;
}

// 是否对象
bool JsonValue::isObject() const
{
    return m_type == Object;
}

// 是否数组
bool JsonValue::isArray() const
{
    return m_type == Array;
}

// 是否文本
bool JsonValue::isString() const
{
    return m_type == String;
}

// 是否数字
bool JsonValue::isNumber() const
{
    return m_type == Number;
}

// 是否布尔
bool JsonValue::isBool() const
{
    return m_type == Bool;
}

// 底层数组
const std::vector<JsonValue>& JsonValue::array() const
{
    return m_arr;
}

// 底层对象
const std::map<std::string, JsonValue>& JsonValue::object() const
{
    return m_obj;
}



}
