/**
 * @file Xml.cpp
 * @brief Xml实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Xml.h"

namespace Vision {

namespace {

// 字面量长度
size_t litLen(const char* s)
{
    size_t n = 0;
    if (!s) {
        return 0;
    }
    while (s[n]) {
        ++n;
    }
    return n;
}
// 名字首字符：字母、下划线、冒号或非 ASCII
bool isNameStart(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':' || c >= 0x80;
}

// 名字后续字符，含数字、减号、点
bool isNameChar(unsigned char c)
{
    return isNameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

// 去掉文本两端空白
void trimEnds(std::string* s)
{
    size_t a = 0;
    while (a < s->size()) {
        const unsigned char c = static_cast<unsigned char>((*s)[a]);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++a;
        } else {
            break;
        }
    }
    size_t b = s->size();
    while (b > a) {
        const unsigned char c = static_cast<unsigned char>((*s)[b - 1]);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            --b;
        } else {
            break;
        }
    }
    if (a != 0 || b != s->size()) {
        *s = s->substr(a, b - a);
    }
}

// 写出时转义 & < >，属性再转义引号
void appendEscaped(std::string* o, const std::string& s, bool attr)
{
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '&') {
            o->append("&amp;");
        } else if (c == '<') {
            o->append("&lt;");
        } else if (c == '>') {
            o->append("&gt;");
        } else if (attr && c == '"') {
            o->append("&quot;");
        } else {
            o->push_back(static_cast<char>(c));
        }
    }
}

struct XmlParser {
    const std::string& t;
    size_t i;
    std::string* err;

// 从文本起点解析
    XmlParser(const std::string& text, std::string* e)
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

// 当前位置是否以字面量开头
    bool starts(const char* s) const
    {
        const size_t n = litLen(s);
        return i + n <= t.size() && t.compare(i, n, s) == 0;
    }

// 当前位置是 c 则吞掉
    bool eat(char c)
    {
        if (i < t.size() && t[i] == c) {
            ++i;
            return true;
        }
        return false;
    }

// 跳到结束标记并吞掉；找不到返回 false
    bool skipUntil(const char* endMark)
    {
        const size_t n = litLen(endMark);
        while (i + n <= t.size()) {
            if (t.compare(i, n, endMark) == 0) {
                i += n;
                return true;
            }
            ++i;
        }
        return false;
    }

// 跳过注释或处理指令
    bool skipMisc()
    {
        if (starts("<!--")) {
            i += 4;
            if (!skipUntil("-->")) {
                fail("unclosed comment");
                return false;
            }
            return true;
        }
        if (starts("<?")) {
            i += 2;
            if (!skipUntil("?>")) {
                fail("unclosed declaration");
                return false;
            }
            return true;
        }
        return false;
    }

// 读元素名或属性名
    bool parseName(std::string* out)
    {
        if (i >= t.size() || !isNameStart(static_cast<unsigned char>(t[i]))) {
            fail("bad name");
            return false;
        }
        const size_t start = i;
        ++i;
        while (i < t.size() && isNameChar(static_cast<unsigned char>(t[i]))) {
            ++i;
        }
        *out = t.substr(start, i - start);
        return true;
    }

// 读一个字符或实体引用
    bool appendEntity(std::string* o)
    {
        if (i >= t.size()) {
            fail("bad entity");
            return false;
        }
        if (t[i] != '&') {
            o->push_back(t[i++]);
            return true;
        }
        if (t.compare(i, 4, "&lt;") == 0) {
            o->push_back('<');
            i += 4;
            return true;
        }
        if (t.compare(i, 4, "&gt;") == 0) {
            o->push_back('>');
            i += 4;
            return true;
        }
        if (t.compare(i, 5, "&amp;") == 0) {
            o->push_back('&');
            i += 5;
            return true;
        }
        if (t.compare(i, 6, "&quot;") == 0) {
            o->push_back('"');
            i += 6;
            return true;
        }
        if (t.compare(i, 6, "&apos;") == 0) {
            o->push_back('\'');
            i += 6;
            return true;
        }
        if (i + 2 < t.size() && t[i + 1] == '#') {
            size_t j = i + 2;
            int base = 10;
            if (j < t.size() && (t[j] == 'x' || t[j] == 'X')) {
                base = 16;
                ++j;
            }
            unsigned int cp = 0;
            const size_t digits = j;
            while (j < t.size() && t[j] != ';') {
                const char c = t[j];
                int d = -1;
                if (c >= '0' && c <= '9') {
                    d = c - '0';
                } else if (base == 16 && c >= 'a' && c <= 'f') {
                    d = c - 'a' + 10;
                } else if (base == 16 && c >= 'A' && c <= 'F') {
                    d = c - 'A' + 10;
                }
                if (d < 0) {
                    break;
                }
                cp = cp * static_cast<unsigned int>(base) + static_cast<unsigned int>(d);
                ++j;
            }
            if (j == digits || j >= t.size() || t[j] != ';' || cp == 0 || cp > 0x10FFFF) {
                fail("bad entity");
                return false;
            }
            i = j + 1;
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
            return true;
        }
        fail("bad entity");
        return false;
    }

// 读引号括起的属性值
    bool parseQuoted(std::string* out)
    {
        if (i >= t.size() || (t[i] != '"' && t[i] != '\'')) {
            fail("bad attribute");
            return false;
        }
        const char q = t[i++];
        out->clear();
        while (i < t.size() && t[i] != q) {
            if (t[i] == '<') {
                fail("bad attribute");
                return false;
            }
            if (!appendEntity(out)) {
                return false;
            }
        }
        if (!eat(q)) {
            fail("bad attribute");
            return false;
        }
        return true;
    }

    bool parseElement(XmlValue* out);
};

// 解析一个元素，含属性和子元素
bool XmlParser::parseElement(XmlValue* out)
{
    skipWs();
    while (starts("<!--") || starts("<?")) {
        if (!skipMisc()) {
            return false;
        }
        skipWs();
    }
    if (!eat('<') || eat('/')) {
        fail("expected element");
        return false;
    }
    std::string name;
    if (!parseName(&name)) {
        return false;
    }
    out->setName(name);
    while (true) {
        skipWs();
        if (starts("/>")) {
            i += 2;
            return true;
        }
        if (eat('>')) {
            break;
        }
        std::string key;
        std::string val;
        if (!parseName(&key)) {
            return false;
        }
        skipWs();
        if (!eat('=')) {
            fail("bad attribute");
            return false;
        }
        skipWs();
        if (!parseQuoted(&val)) {
            return false;
        }
        out->setAttr(key, val);
    }
    std::string text;
    while (i < t.size()) {
        if (starts("</")) {
            i += 2;
            std::string endName;
            if (!parseName(&endName) || endName != name) {
                fail("mismatched end tag");
                return false;
            }
            skipWs();
            if (!eat('>')) {
                fail("bad end tag");
                return false;
            }
            trimEnds(&text);
            out->setText(text);
            return true;
        }
        if (starts("<!--") || starts("<?")) {
            if (!skipMisc()) {
                return false;
            }
            continue;
        }
        if (t[i] == '<') {
            XmlValue child;
            if (!parseElement(&child)) {
                return false;
            }
            out->appendChild(child);
            continue;
        }
        if (!appendEntity(&text)) {
            return false;
        }
    }
    fail("unclosed element");
    return false;
}

// 递归写出元素
void dump(const XmlValue& v, std::string* o, bool pretty, int indent)
{
    if (pretty && indent > 0) {
        o->push_back('\n');
        o->append(static_cast<size_t>(indent), ' ');
    }
    o->push_back('<');
    o->append(v.name());
    const std::map<std::string, std::string>& attr = v.attrs();
    for (std::map<std::string, std::string>::const_iterator it = attr.begin(); it != attr.end(); ++it) {
        o->push_back(' ');
        o->append(it->first);
        o->append("=\"");
        appendEscaped(o, it->second, true);
        o->push_back('"');
    }
    const int n = v.childCount();
    if (n == 0 && v.text().empty()) {
        o->append("/>");
        return;
    }
    o->push_back('>');
    if (!v.text().empty()) {
        appendEscaped(o, v.text(), false);
    }
    for (int i = 0; i < n; ++i) {
        dump(v.childAt(i), o, pretty, indent + 2);
    }
    if (pretty && n > 0) {
        o->push_back('\n');
        o->append(static_cast<size_t>(indent), ' ');
    }
    o->append("</");
    o->append(v.name());
    o->push_back('>');
}

}

// 空元素
XmlValue::XmlValue()
{
}

// 设置元素名
void XmlValue::setName(const std::string& elementName)
{
    m_name = elementName;
}

// 设置元素文本
void XmlValue::setText(const std::string& text)
{
    m_text = text;
}

// 是否有该属性
bool XmlValue::hasAttr(const std::string& key) const
{
    return m_attr.find(key) != m_attr.end();
}

// 读属性；没有则返回默认值
std::string XmlValue::attr(const std::string& key, const std::string& def) const
{
    std::map<std::string, std::string>::const_iterator it = m_attr.find(key);
    if (it == m_attr.end()) {
        return def;
    }
    return it->second;
}

// 写属性
void XmlValue::setAttr(const std::string& key, const std::string& value)
{
    m_attr[key] = value;
}

// 子元素个数
int XmlValue::childCount() const
{
    return static_cast<int>(m_children.size());
}

// 按下标取子元素；越界为空元素
XmlValue XmlValue::childAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_children.size())) {
        return XmlValue();
    }
    return m_children[static_cast<size_t>(index)];
}

// 取第一个同名子元素
XmlValue XmlValue::child(const std::string& elementName) const
{
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i].name() == elementName) {
            return m_children[i];
        }
    }
    return XmlValue();
}

// 是否有同名子元素
bool XmlValue::hasChild(const std::string& elementName) const
{
    return !child(elementName).isEmpty();
}

// 追加子元素
void XmlValue::appendChild(const XmlValue& child)
{
    m_children.push_back(child);
}

// 解析文档根元素；可带 BOM 和 XML 声明
XmlValue XmlValue::parse(const std::string& text, std::string* error)
{
    if (error) {
        error->clear();
    }
    XmlParser p(text, error);
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEF
        && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF) {
        p.i = 3;
    }
    XmlValue v;
    if (!p.parseElement(&v)) {
        return XmlValue();
    }
    p.skipWs();
    while (p.starts("<!--") || p.starts("<?")) {
        if (!p.skipMisc()) {
            return XmlValue();
        }
        p.skipWs();
    }
    if (p.i != text.size() && error && error->empty()) {
        *error = "trailing data";
    }
    return v;
}

// 序列化；空元素返回空串
std::string XmlValue::stringify(const XmlValue& v, bool pretty)
{
    if (v.isEmpty()) {
        return std::string();
    }
    std::string o = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    if (pretty) {
        o.push_back('\n');
    }
    dump(v, &o, pretty, 0);
    if (pretty) {
        o.push_back('\n');
    }
    return o;
}

// 是否为空
bool XmlValue::isEmpty() const
{
    return m_name.empty();
}

// 节点名
const std::string& XmlValue::name() const
{
    return m_name;
}

// 文本
const std::string& XmlValue::text() const
{
    return m_text;
}

// 属性表
const std::map<std::string, std::string>& XmlValue::attrs() const
{
    return m_attr;
}

}