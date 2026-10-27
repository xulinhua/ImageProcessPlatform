/**
 * @file HsTuple.cpp
 * @brief HsTuple实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsTuple.h>
#include <HsCommData/HsException.h>

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

namespace HsCommData {

struct HsTuple::Body {
    TupleType type;                 ///< 当前种类
    std::vector<int64_t> longs;     ///< TupleLong
    std::vector<double> doubles;    ///< TupleDouble
    std::vector<std::string> texts; ///< TupleString
    std::vector<TupleElem> mixed;   ///< TupleMixed

    Body()
        : type(TupleEmpty)
    {
    }
};

// 纯数组升成混合，原数组清空
void HsTuple::promote(Body* body)
{
    if (!body || body->type == TupleMixed || body->type == TupleEmpty) {
        if (body && body->type == TupleEmpty) {
            body->type = TupleMixed;
        }
        return;
    }
    std::vector<TupleElem> mixed;
    if (body->type == TupleLong) {
        mixed.reserve(body->longs.size());
        for (size_t i = 0; i < body->longs.size(); ++i) {
            TupleElem elem;
            elem.kind = ElemLong;
            elem.num.i = body->longs[i];
            mixed.push_back(elem);
        }
        body->longs.clear();
    } else if (body->type == TupleDouble) {
        mixed.reserve(body->doubles.size());
        for (size_t i = 0; i < body->doubles.size(); ++i) {
            TupleElem elem;
            elem.kind = ElemDouble;
            elem.num.d = body->doubles[i];
            mixed.push_back(elem);
        }
        body->doubles.clear();
    } else if (body->type == TupleString) {
        mixed.reserve(body->texts.size());
        for (size_t i = 0; i < body->texts.size(); ++i) {
            TupleElem elem;
            elem.kind = ElemString;
            elem.text = body->texts[i];
            mixed.push_back(elem);
        }
        body->texts.clear();
    }
    body->mixed.swap(mixed);
    body->type = TupleMixed;
}

// 追加一个混合元素
void HsTuple::pushMixed(Body* body, const TupleElem& elem)
{
    promote(body);
    body->mixed.push_back(elem);
}

// 整数 0
TupleElem::TupleElem()
    : kind(ElemLong)
{
    num.i = 0;
}

// 空元组
HsTuple::HsTuple()
{
}

// 同类型拷贝，共享缓冲
HsTuple::HsTuple(const HsTuple& other)
    : m_body(other.m_body)
{
}

// 同类型赋值，共享缓冲
HsTuple& HsTuple::operator=(const HsTuple& other)
{
    m_body = other.m_body;
    return *this;
}

// 一个整数
HsTuple::HsTuple(int64_t value)
{
    append(value);
}

// 32 位整数 ctor，升 64 位，避免与 double 构造歧义
HsTuple::HsTuple(int value)
    : HsTuple(static_cast<int64_t>(value))
{
}

// 一个浮点数
HsTuple::HsTuple(double value)
{
    append(value);
}

// 一个字符串
HsTuple::HsTuple(const char* value)
{
    append(value);
}

// 一个字符串
HsTuple::HsTuple(const std::string& value)
{
    append(value);
}

// 整数数组
HsTuple::HsTuple(const int64_t* data, int count)
{
    if (!data || count <= 0) {
        return;
    }
    ensure();
    m_body->type = TupleLong;
    m_body->longs.assign(data, data + count);
}

// 浮点数组
HsTuple::HsTuple(const double* data, int count)
{
    if (!data || count <= 0) {
        return;
    }
    ensure();
    m_body->type = TupleDouble;
    m_body->doubles.assign(data, data + count);
}

// 换成这一个整数
HsTuple& HsTuple::operator=(int value)
{
    return *this = HsTuple(static_cast<int64_t>(value));
}

// 换成这一个整数
HsTuple& HsTuple::operator=(int64_t value)
{
    return *this = HsTuple(value);
}

// 换成这一个浮点数
HsTuple& HsTuple::operator=(double value)
{
    return *this = HsTuple(value);
}

// 换成这一个字符串
HsTuple& HsTuple::operator=(const char* value)
{
    return *this = HsTuple(value);
}

// 换成这一个字符串
HsTuple& HsTuple::operator=(const std::string& value)
{
    return *this = HsTuple(value);
}

// 没有元素
bool HsTuple::isEmpty() const
{
    return length() == 0;
}

// 元素个数
int HsTuple::length() const
{
    if (!m_body) {
        return 0;
    }
    switch (m_body->type) {
    case TupleLong:
        return static_cast<int>(m_body->longs.size());
    case TupleDouble:
        return static_cast<int>(m_body->doubles.size());
    case TupleString:
        return static_cast<int>(m_body->texts.size());
    case TupleMixed:
        return static_cast<int>(m_body->mixed.size());
    default:
        return 0;
    }
}

// 当前存储种类
TupleType HsTuple::type() const
{
    if (!m_body) {
        return TupleEmpty;
    }
    return m_body->type;
}

// 清空
void HsTuple::clear()
{
    m_body.reset();
}

// 深拷缓冲
HsTuple HsTuple::clone() const
{
    HsTuple out;
    if (!m_body) {
        return out;
    }
    out.m_body = std::make_shared<Body>(*m_body);
    return out;
}

// 追加整数
void HsTuple::append(int64_t value)
{
    ensure();
    detach();
    if (m_body->type == TupleEmpty || m_body->type == TupleLong) {
        m_body->type = TupleLong;
        m_body->longs.push_back(value);
        return;
    }
    TupleElem elem;
    elem.kind = ElemLong;
    elem.num.i = value;
    pushMixed(m_body.get(), elem);
}

// 追加浮点
void HsTuple::append(double value)
{
    ensure();
    detach();
    if (m_body->type == TupleEmpty || m_body->type == TupleDouble) {
        m_body->type = TupleDouble;
        m_body->doubles.push_back(value);
        return;
    }
    TupleElem elem;
    elem.kind = ElemDouble;
    elem.num.d = value;
    pushMixed(m_body.get(), elem);
}

// 追加字符串
void HsTuple::append(const char* value)
{
    append(std::string(value ? value : ""));
}

// 追加字符串
void HsTuple::append(const std::string& value)
{
    ensure();
    detach();
    if (m_body->type == TupleEmpty || m_body->type == TupleString) {
        m_body->type = TupleString;
        m_body->texts.push_back(value);
        return;
    }
    TupleElem elem;
    elem.kind = ElemString;
    elem.text = value;
    pushMixed(m_body.get(), elem);
}

// 接上另一元组
void HsTuple::append(const HsTuple& other)
{
    if (other.isEmpty()) {
        return;
    }
    if (isEmpty()) {
        m_body = other.m_body;
        return;
    }
    if (m_body == other.m_body) {
        append(other.clone());
        return;
    }
    detach();
    if (m_body->type == other.m_body->type && m_body->type != TupleMixed) {
        if (m_body->type == TupleLong) {
            m_body->longs.insert(m_body->longs.end(), other.m_body->longs.begin(), other.m_body->longs.end());
        } else if (m_body->type == TupleDouble) {
            m_body->doubles.insert(m_body->doubles.end(), other.m_body->doubles.begin(), other.m_body->doubles.end());
        } else if (m_body->type == TupleString) {
            m_body->texts.insert(m_body->texts.end(), other.m_body->texts.begin(), other.m_body->texts.end());
        }
        return;
    }
    if (other.m_body->type == TupleLong) {
        for (size_t i = 0; i < other.m_body->longs.size(); ++i) {
            append(other.m_body->longs[i]);
        }
        return;
    }
    if (other.m_body->type == TupleDouble) {
        for (size_t i = 0; i < other.m_body->doubles.size(); ++i) {
            append(other.m_body->doubles[i]);
        }
        return;
    }
    if (other.m_body->type == TupleString) {
        for (size_t i = 0; i < other.m_body->texts.size(); ++i) {
            append(other.m_body->texts[i]);
        }
        return;
    }
    for (size_t i = 0; i < other.m_body->mixed.size(); ++i) {
        const TupleElem& elem = other.m_body->mixed[i];
        if (elem.kind == ElemLong) {
            append(elem.num.i);
        } else if (elem.kind == ElemDouble) {
            append(elem.num.d);
        } else {
            append(elem.text);
        }
    }
}

// 读整数
bool HsTuple::getLong(int index, int64_t* out) const
{
    if (!out) {
        return false;
    }
    const int n = length();
    if (index < 0) {
        index += n;
    }
    if (index < 0 || index >= n) {
        return false;
    }
    if (m_body->type == TupleLong) {
        *out = m_body->longs[static_cast<size_t>(index)];
        return true;
    }
    if (m_body->type == TupleMixed && m_body->mixed[static_cast<size_t>(index)].kind == ElemLong) {
        *out = m_body->mixed[static_cast<size_t>(index)].num.i;
        return true;
    }
    return false;
}

// 读浮点数
bool HsTuple::getDouble(int index, double* out) const
{
    if (!out) {
        return false;
    }
    const int n = length();
    if (index < 0) {
        index += n;
    }
    if (index < 0 || index >= n) {
        return false;
    }
    if (m_body->type == TupleDouble) {
        *out = m_body->doubles[static_cast<size_t>(index)];
        return true;
    }
    if (m_body->type == TupleMixed && m_body->mixed[static_cast<size_t>(index)].kind == ElemDouble) {
        *out = m_body->mixed[static_cast<size_t>(index)].num.d;
        return true;
    }
    return false;
}

// 读字符串
bool HsTuple::getString(int index, std::string* out) const
{
    if (!out) {
        return false;
    }
    const int n = length();
    if (index < 0) {
        index += n;
    }
    if (index < 0 || index >= n) {
        return false;
    }
    if (m_body->type == TupleString) {
        *out = m_body->texts[static_cast<size_t>(index)];
        return true;
    }
    if (m_body->type == TupleMixed && m_body->mixed[static_cast<size_t>(index)].kind == ElemString) {
        *out = m_body->mixed[static_cast<size_t>(index)].text;
        return true;
    }
    return false;
}

// 纯整数连续缓冲。类型不符抛异常
const int64_t* HsTuple::longData() const
{
    if (!m_body || m_body->type != TupleLong) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return m_body->longs.empty() ? 0 : m_body->longs.data();
}

// 可写整数缓冲。类型不符抛异常
int64_t* HsTuple::longData()
{
    if (!m_body || m_body->type != TupleLong) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    detach();
    return m_body->longs.empty() ? 0 : m_body->longs.data();
}

// 纯浮点连续缓冲。类型不符抛异常
const double* HsTuple::doubleData() const
{
    if (!m_body || m_body->type != TupleDouble) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return m_body->doubles.empty() ? 0 : m_body->doubles.data();
}

// 可写浮点缓冲。类型不符抛异常
double* HsTuple::doubleData()
{
    if (!m_body || m_body->type != TupleDouble) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    detach();
    return m_body->doubles.empty() ? 0 : m_body->doubles.data();
}

// 改成整数。越界抛异常
bool HsTuple::setLong(int index, int64_t value)
{
    index = resolveIndex(index);
    detach();
    if (m_body->type == TupleLong) {
        m_body->longs[static_cast<size_t>(index)] = value;
        return true;
    }
    promote(m_body.get());
    m_body->mixed[static_cast<size_t>(index)].kind = ElemLong;
    m_body->mixed[static_cast<size_t>(index)].num.i = value;
    m_body->mixed[static_cast<size_t>(index)].text.clear();
    return true;
}

// 改成浮点。越界抛异常
bool HsTuple::setDouble(int index, double value)
{
    index = resolveIndex(index);
    detach();
    if (m_body->type == TupleDouble) {
        m_body->doubles[static_cast<size_t>(index)] = value;
        return true;
    }
    promote(m_body.get());
    m_body->mixed[static_cast<size_t>(index)].kind = ElemDouble;
    m_body->mixed[static_cast<size_t>(index)].num.d = value;
    m_body->mixed[static_cast<size_t>(index)].text.clear();
    return true;
}

// 改成字符串。越界抛异常
bool HsTuple::setString(int index, const std::string& value)
{
    index = resolveIndex(index);
    detach();
    if (m_body->type == TupleString) {
        m_body->texts[static_cast<size_t>(index)] = value;
        return true;
    }
    promote(m_body.get());
    m_body->mixed[static_cast<size_t>(index)].kind = ElemString;
    m_body->mixed[static_cast<size_t>(index)].num.i = 0;
    m_body->mixed[static_cast<size_t>(index)].text = value;
    return true;
}

// 保证缓冲存在
void HsTuple::ensure()
{
    if (!m_body) {
        m_body = std::make_shared<Body>();
    }
}

// 被共享时复制缓冲
void HsTuple::detach()
{
    if (m_body && m_body.use_count() > 1) {
        m_body = std::make_shared<Body>(*m_body);
    }
}

// 取出一格。越界抛异常
HsTuple HsTuple::operator[](int index) const
{
    index = resolveIndex(index);
    int64_t i = 0;
    double d = 0.0;
    std::string text;
    if (getLong(index, &i)) {
        return HsTuple(i);
    }
    if (getDouble(index, &d)) {
        return HsTuple(d);
    }
    if (getString(index, &text)) {
        return HsTuple(text);
    }
    raiseException(HsTupleAccessException(HS_ERR_TYPE));
    return HsTuple();
}

// 第 0 个 32 位整数
int HsTuple::I() const
{
    return I(0);
}

// 第 index 个 32 位整数。类型不符、越界或超出 int 范围
int HsTuple::I(int index) const
{
    index = resolveIndex(index);
    int64_t value = 0;
    if (!getLong(index, &value)) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    if (value > INT_MAX || value < INT_MIN) {
        raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
    }
    return static_cast<int>(value);
}

// 第 0 个长整数
int64_t HsTuple::L() const
{
    return L(0);
}

// 第 index 个长整数。类型不符或越界抛 HsException
int64_t HsTuple::L(int index) const
{
    index = resolveIndex(index);
    int64_t value = 0;
    if (!getLong(index, &value)) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return value;
}

// 第 0 个浮点数
double HsTuple::D() const
{
    return D(0);
}

// 第 index 个浮点数；类型不符或越界抛异常
double HsTuple::D(int index) const
{
    index = resolveIndex(index);
    double value = 0.0;
    if (!getDouble(index, &value)) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return value;
}

// 第 0 个字符串
std::string HsTuple::S() const
{
    return S(0);
}

// 第 index 个字符串。类型不符或越界抛 HsException
std::string HsTuple::S(int index) const
{
    index = resolveIndex(index);
    std::string text;
    if (!getString(index, &text)) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return text;
}

// 第 0 个字符串指针
const char* HsTuple::C() const
{
    return C(0);
}

// 第 index 个字符串的 C 指针。类型不符或越界抛 HsException
const char* HsTuple::C(int index) const
{
    index = resolveIndex(index);
    if (!m_body) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
        return 0;
    }
    if (m_body->type == TupleString) {
        return m_body->texts[static_cast<size_t>(index)].c_str();
    }
    if (m_body->type == TupleMixed
        && m_body->mixed[static_cast<size_t>(index)].kind == ElemString) {
        return m_body->mixed[static_cast<size_t>(index)].text.c_str();
    }
    raiseException(HsTupleAccessException(HS_ERR_TYPE));
    return 0;
}

namespace {

enum ElemClass {
    ElemNone = 0,
    ElemNumLong,
    ElemNumDouble,
    ElemText
};

struct ElemSnap {
    ElemClass cls;
    int64_t i;
    double d;
    std::string text;

    ElemSnap()
        : cls(ElemNone)
        , i(0)
        , d(0.0)
    {
    }
};

// 取出一格。越界为 ElemNone
ElemSnap snapAt(const HsTuple& t, int index)
{
    ElemSnap s;
    if (t.getLong(index, &s.i)) {
        s.cls = ElemNumLong;
        s.d = static_cast<double>(s.i);
        return s;
    }
    if (t.getDouble(index, &s.d)) {
        s.cls = ElemNumDouble;
        return s;
    }
    if (t.getString(index, &s.text)) {
        s.cls = ElemText;
        return s;
    }
    return s;
}

// 单格比较：-1/0/1 为小于、相等、大于；-2 表示数字与字符串不可比，或出现 NaN
int cmpElem(const ElemSnap& a, const ElemSnap& b)
{
    const bool aNum = a.cls == ElemNumLong || a.cls == ElemNumDouble;
    const bool bNum = b.cls == ElemNumLong || b.cls == ElemNumDouble;
    if (aNum && bNum) {
        if (a.cls == ElemNumLong && b.cls == ElemNumLong) {
            if (a.i < b.i) {
                return -1;
            }
            if (a.i > b.i) {
                return 1;
            }
            return 0;
        }
        const double da = (a.cls == ElemNumLong) ? static_cast<double>(a.i) : a.d;
        const double db = (b.cls == ElemNumLong) ? static_cast<double>(b.i) : b.d;
        if (da < db) {
            return -1;
        }
        if (da > db) {
            return 1;
        }
        if (da == db) {
            return 0;
        }
        return -2;
    }
    if (a.cls == ElemText && b.cls == ElemText) {
        const int c = a.text.compare(b.text);
        if (c < 0) {
            return -1;
        }
        if (c > 0) {
            return 1;
        }
        return 0;
    }
    return -2;
}

// 字典序；前缀相同则更短的更小。不可比时 *bad 置 true
int cmpTuples(const HsTuple& a, const HsTuple& b, bool* bad)
{
    *bad = false;
    const int na = a.length();
    const int nb = b.length();
    const int n = (na < nb) ? na : nb;
    for (int i = 0; i < n; ++i) {
        const int c = cmpElem(snapAt(a, i), snapAt(b, i));
        if (c == -2) {
            *bad = true;
            return 0;
        }
        if (c != 0) {
            return c;
        }
    }
    if (na < nb) {
        return -1;
    }
    if (na > nb) {
        return 1;
    }
    return 0;
}

enum ElemPred {
    PredEq = 0,
    PredNe,
    PredLt,
    PredGt,
    PredLe,
    PredGe
};

enum ArithOp {
    ArithAdd = 0,
    ArithSub,
    ArithMul,
    ArithDiv
};

// 广播结果长度；任一为空则 *empty=true；两边长度均非 1 且不等则抛类型异常
int broadcastCount(int na, int nb, bool* empty)
{
    *empty = false;
    if (na == 0 || nb == 0) {
        *empty = true;
        return 0;
    }
    if (na == nb || nb == 1) {
        return na;
    }
    if (na == 1) {
        return nb;
    }
    raiseException(HsTupleAccessException(HS_ERR_TYPE));
    return 0;
}

// 一边长度为 1 时固定取第 0 个
void pairIndex(int i, int na, int nb, int* ia, int* ib)
{
    *ia = (na == 1 && nb != 1) ? 0 : i;
    *ib = (nb == 1 && na != 1) ? 0 : i;
}

// 逐元素比较；相等时数字与字符串不算相等；不可比类型抛异常
HsTuple elemPred(const HsTuple& a, const HsTuple& b, ElemPred pred)
{
    bool empty = false;
    const int na = a.length();
    const int nb = b.length();
    const int n = broadcastCount(na, nb, &empty);
    if (empty) {
        return HsTuple();
    }
    const bool ordered = pred != PredEq && pred != PredNe;
    HsTuple out;
    for (int i = 0; i < n; ++i) {
        int ia = 0;
        int ib = 0;
        pairIndex(i, na, nb, &ia, &ib);
        const int c = cmpElem(snapAt(a, ia), snapAt(b, ib));
        if (c == -2 && ordered) {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
        int64_t bit = 0;
        if (pred == PredEq) {
            bit = (c == 0) ? 1 : 0;
        } else if (pred == PredNe) {
            bit = (c != 0) ? 1 : 0;
        } else if (pred == PredLt) {
            bit = (c < 0) ? 1 : 0;
        } else if (pred == PredGt) {
            bit = (c > 0) ? 1 : 0;
        } else if (pred == PredLe) {
            bit = (c <= 0) ? 1 : 0;
        } else {
            bit = (c >= 0) ? 1 : 0;
        }
        out.append(bit);
    }
    return out;
}

// 整数相加。溢出抛 HS_ERR_INT_RANGE
int64_t add64(int64_t a, int64_t b)
{
    if (b > 0 && a > INT64_MAX - b) {
        raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
    }
    if (b < 0 && a < INT64_MIN - b) {
        raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
    }
    return a + b;
}

// 整数相减。溢出抛 HS_ERR_INT_RANGE
int64_t sub64(int64_t a, int64_t b)
{
    if (b > 0 && a < INT64_MIN + b) {
        raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
    }
    if (b < 0 && a > INT64_MAX + b) {
        raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
    }
    return a - b;
}

// 整数相乘。溢出抛 HS_ERR_INT_RANGE
int64_t mul64(int64_t a, int64_t b)
{
    if (a == 0 || b == 0) {
        return 0;
    }
    if (a == 1) {
        return b;
    }
    if (b == 1) {
        return a;
    }
    if (a == -1) {
        if (b == INT64_MIN) {
            raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
        }
        return -b;
    }
    if (b == -1) {
        if (a == INT64_MIN) {
            raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
        }
        return -a;
    }
    if (a > 0) {
        if (b > 0) {
            if (a > INT64_MAX / b) {
                raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
            }
        } else if (b < INT64_MIN / a) {
            raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
        }
    } else if (b > 0) {
        if (a < INT64_MIN / b) {
            raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
        }
    } else if (a < INT64_MAX / b) {
        raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
    }
    return a * b;
}

// 整数相除。除数是 0 抛类型错误。INT64_MIN / -1 溢出
int64_t div64(int64_t a, int64_t b)
{
    if (b == 0) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    if (a == INT64_MIN && b == -1) {
        raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
    }
    return a / b;
}

// 数字转成拼接用的文本。整数不带小数，浮点用紧凑格
std::string numText(const ElemSnap& e)
{
    if (e.cls == ElemNumLong) {
        return std::to_string(e.i);
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.15g", e.d);
    return std::string(buf);
}

// 一格的四则运算，结果追加到 out
void appendArith(HsTuple* out, const ElemSnap& a, const ElemSnap& b, ArithOp op)
{
    const bool aNum = a.cls == ElemNumLong || a.cls == ElemNumDouble;
    const bool bNum = b.cls == ElemNumLong || b.cls == ElemNumDouble;
    if (op == ArithAdd && (a.cls == ElemText || b.cls == ElemText)) {
        if (!aNum && a.cls != ElemText) {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
        if (!bNum && b.cls != ElemText) {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
        const std::string left = (a.cls == ElemText) ? a.text : numText(a);
        const std::string right = (b.cls == ElemText) ? b.text : numText(b);
        out->append(left + right);
        return;
    }
    if (!aNum || !bNum) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    if (a.cls == ElemNumLong && b.cls == ElemNumLong) {
        if (op == ArithAdd) {
            out->append(add64(a.i, b.i));
        } else if (op == ArithSub) {
            out->append(sub64(a.i, b.i));
        } else if (op == ArithMul) {
            out->append(mul64(a.i, b.i));
        } else {
            out->append(div64(a.i, b.i));
        }
        return;
    }
    const double da = (a.cls == ElemNumLong) ? static_cast<double>(a.i) : a.d;
    const double db = (b.cls == ElemNumLong) ? static_cast<double>(b.i) : b.d;
    if (op == ArithDiv && db == 0.0) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    double r = 0.0;
    if (op == ArithAdd) {
        r = da + db;
    } else if (op == ArithSub) {
        r = da - db;
    } else if (op == ArithMul) {
        r = da * db;
    } else {
        r = da / db;
    }
    out->append(r);
}

// 逐元素四则运算
HsTuple arithTuples(const HsTuple& a, const HsTuple& b, ArithOp op)
{
    bool empty = false;
    const int na = a.length();
    const int nb = b.length();
    const int n = broadcastCount(na, nb, &empty);
    if (empty) {
        return HsTuple();
    }
    HsTuple out;
    for (int i = 0; i < n; ++i) {
        int ia = 0;
        int ib = 0;
        pairIndex(i, na, nb, &ia, &ib);
        appendArith(&out, snapAt(a, ia), snapAt(b, ib), op);
    }
    return out;
}

}

// 整组相等。数字和字符串不能比时视为不相等
bool HsTuple::operator==(const HsTuple& other) const
{
    bool bad = false;
    const int c = cmpTuples(*this, other, &bad);
    if (bad) {
        return false;
    }
    return c == 0;
}

// 整组不等
bool HsTuple::operator!=(const HsTuple& other) const
{
    return !(*this == other);
}

// 字典序小于。数字与字符串不能比则抛类型错误
bool HsTuple::operator<(const HsTuple& other) const
{
    bool bad = false;
    const int c = cmpTuples(*this, other, &bad);
    if (bad) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return c < 0;
}

// 字典序大于
bool HsTuple::operator>(const HsTuple& other) const
{
    bool bad = false;
    const int c = cmpTuples(*this, other, &bad);
    if (bad) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return c > 0;
}

// 字典序小于等于
bool HsTuple::operator<=(const HsTuple& other) const
{
    bool bad = false;
    const int c = cmpTuples(*this, other, &bad);
    if (bad) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return c <= 0;
}

// 字典序大于等于
bool HsTuple::operator>=(const HsTuple& other) const
{
    bool bad = false;
    const int c = cmpTuples(*this, other, &bad);
    if (bad) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return c >= 0;
}

// 逐元素相等
HsTuple HsTuple::equalElem(const HsTuple& other) const
{
    return elemPred(*this, other, PredEq);
}

// 逐元素不等
HsTuple HsTuple::notEqualElem(const HsTuple& other) const
{
    return elemPred(*this, other, PredNe);
}

// 逐元素小于
HsTuple HsTuple::lessElem(const HsTuple& other) const
{
    return elemPred(*this, other, PredLt);
}

// 逐元素大于
HsTuple HsTuple::greaterElem(const HsTuple& other) const
{
    return elemPred(*this, other, PredGt);
}

// 逐元素小于等于
HsTuple HsTuple::lessEqualElem(const HsTuple& other) const
{
    return elemPred(*this, other, PredLe);
}

// 逐元素大于等于
HsTuple HsTuple::greaterEqualElem(const HsTuple& other) const
{
    return elemPred(*this, other, PredGe);
}

// 逐元素加
HsTuple HsTuple::operator+(const HsTuple& other) const
{
    return arithTuples(*this, other, ArithAdd);
}

// 逐元素减
HsTuple HsTuple::operator-(const HsTuple& other) const
{
    return arithTuples(*this, other, ArithSub);
}

// 逐元素乘
HsTuple HsTuple::operator*(const HsTuple& other) const
{
    return arithTuples(*this, other, ArithMul);
}

// 逐元素除
HsTuple HsTuple::operator/(const HsTuple& other) const
{
    return arithTuples(*this, other, ArithDiv);
}

// 逐元素取负
HsTuple HsTuple::operator-() const
{
    HsTuple out;
    const int n = length();
    for (int i = 0; i < n; ++i) {
        const ElemSnap s = snapAt(*this, i);
        if (s.cls == ElemNumLong) {
            if (s.i == INT64_MIN) {
                raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
            }
            out.append(-s.i);
        } else if (s.cls == ElemNumDouble) {
            out.append(-s.d);
        } else {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
    }
    return out;
}


// 负下标归一化到 [0, length())
int HsTuple::resolveIndex(int index) const
{
    const int n = length();
    if (index < 0) {
        index += n;
    }
    if (index < 0 || index >= n) {
        raiseException(HsTupleAccessException(HS_ERR_INDEX));
    }
    return index;
}

// 插入下标允许等于 length()
static int resolveInsertIndex(const HsTuple& t, int index)
{
    const int n = t.length();
    if (index < 0) {
        index += n;
    }
    if (index < 0 || index > n) {
        raiseException(HsTupleAccessException(HS_ERR_INDEX));
    }
    return index;
}

// 读下标元组中的整数下标
static int readIndexAt(const HsTuple& indices, int i, int bound)
{
    int64_t v = 0;
    if (!indices.getLong(i, &v)) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    if (v < INT_MIN || v > INT_MAX) {
        raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
    }
    int idx = static_cast<int>(v);
    if (idx < 0) {
        idx += bound;
    }
    if (idx < 0 || idx >= bound) {
        raiseException(HsTupleAccessException(HS_ERR_INDEX));
    }
    return idx;
}

// 掩码格是否非 0
static bool maskOn(const HsTuple& mask, int i)
{
    int64_t li = 0;
    double d = 0.0;
    if (mask.getLong(i, &li)) {
        return li != 0;
    }
    if (mask.getDouble(i, &d)) {
        return d != 0.0;
    }
    raiseException(HsTupleAccessException(HS_ERR_TYPE));
    return false;
}

// 把第 index 格追加到 out
static void appendAt(const HsTuple& src, int index, HsTuple* out)
{
    int64_t li = 0;
    double d = 0.0;
    std::string text;
    if (src.getLong(index, &li)) {
        out->append(li);
    } else if (src.getDouble(index, &d)) {
        out->append(d);
    } else if (src.getString(index, &text)) {
        out->append(text);
    } else {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
}

HsTuple HsTuple::genSequence(int64_t start, int64_t end, int64_t step)
{
    if (step == 0) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    HsTuple out;
    if (step > 0) {
        if (start > end) {
            return out;
        }
        for (int64_t v = start;;) {
            out.append(v);
            if (v > end - step) {
                break;
            }
            v += step;
        }
    } else {
        if (start < end) {
            return out;
        }
        for (int64_t v = start;;) {
            out.append(v);
            if (v < end - step) {
                break;
            }
            v += step;
        }
    }
    return out;
}

HsTuple HsTuple::genConst(int count, int64_t value)
{
    HsTuple out;
    for (int i = 0; i < count; ++i) {
        out.append(value);
    }
    return out;
}

HsTuple HsTuple::genConst(int count, double value)
{
    HsTuple out;
    for (int i = 0; i < count; ++i) {
        out.append(value);
    }
    return out;
}

HsTuple HsTuple::genConst(int count, const char* value)
{
    return genConst(count, std::string(value ? value : ""));
}

HsTuple HsTuple::genConst(int count, const std::string& value)
{
    HsTuple out;
    for (int i = 0; i < count; ++i) {
        out.append(value);
    }
    return out;
}

HsTuple HsTuple::select(const HsTuple& indices) const
{
    HsTuple out;
    const int n = length();
    const int m = indices.length();
    for (int i = 0; i < m; ++i) {
        appendAt(*this, readIndexAt(indices, i, n), &out);
    }
    return out;
}

HsTuple HsTuple::selectMask(const HsTuple& mask) const
{
    const int n = length();
    if (mask.length() != n) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    HsTuple out;
    for (int i = 0; i < n; ++i) {
        if (maskOn(mask, i)) {
            appendAt(*this, i, &out);
        }
    }
    return out;
}

void HsTuple::remove(const HsTuple& indices)
{
    const int n = length();
    std::vector<char> drop(static_cast<size_t>(n), 0);
    const int m = indices.length();
    for (int i = 0; i < m; ++i) {
        drop[static_cast<size_t>(readIndexAt(indices, i, n))] = 1;
    }
    HsTuple out;
    for (int i = 0; i < n; ++i) {
        if (!drop[static_cast<size_t>(i)]) {
            appendAt(*this, i, &out);
        }
    }
    *this = out;
}

void HsTuple::removeMask(const HsTuple& mask)
{
    const int n = length();
    if (mask.length() != n) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    HsTuple out;
    for (int i = 0; i < n; ++i) {
        if (!maskOn(mask, i)) {
            appendAt(*this, i, &out);
        }
    }
    *this = out;
}

void HsTuple::insert(int index, const HsTuple& values)
{
    index = resolveInsertIndex(*this, index);
    const int n = length();
    const int m = values.length();
    HsTuple out;
    for (int i = 0; i < index; ++i) {
        appendAt(*this, i, &out);
    }
    for (int i = 0; i < m; ++i) {
        appendAt(values, i, &out);
    }
    for (int i = index; i < n; ++i) {
        appendAt(*this, i, &out);
    }
    *this = out;
}

void HsTuple::replace(int index, const HsTuple& values)
{
    index = resolveIndex(index);
    const int n = length();
    const int m = values.length();
    HsTuple out;
    for (int i = 0; i < index; ++i) {
        appendAt(*this, i, &out);
    }
    for (int i = 0; i < m; ++i) {
        appendAt(values, i, &out);
    }
    for (int i = index + 1; i < n; ++i) {
        appendAt(*this, i, &out);
    }
    *this = out;
}

HsTuple HsTuple::sum() const
{
    const int n = length();
    if (n == 0) {
        return HsTuple(static_cast<int64_t>(0));
    }
    bool allLong = true;
    int64_t si = 0;
    double sd = 0.0;
    for (int i = 0; i < n; ++i) {
        int64_t li = 0;
        double d = 0.0;
        if (getLong(i, &li)) {
            if (allLong) {
                if ((si > 0 && li > INT64_MAX - si) || (si < 0 && li < INT64_MIN - si)) {
                    allLong = false;
                    sd = static_cast<double>(si) + static_cast<double>(li);
                } else {
                    si += li;
                }
            } else {
                sd += static_cast<double>(li);
            }
        } else if (getDouble(i, &d)) {
            if (allLong) {
                allLong = false;
                sd = static_cast<double>(si) + d;
            } else {
                sd += d;
            }
        } else {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
    }
    if (allLong) {
        return HsTuple(si);
    }
    return HsTuple(sd);
}

HsTuple HsTuple::min() const
{
    const int n = length();
    if (n == 0) {
        raiseException(HsTupleAccessException(HS_ERR_INDEX));
    }
    int best = 0;
    double bestD = 0.0;
    bool bestLong = false;
    int64_t bestL = 0;
    {
        int64_t li = 0;
        double d = 0.0;
        if (getLong(0, &li)) {
            bestLong = true;
            bestL = li;
            bestD = static_cast<double>(li);
        } else if (getDouble(0, &d)) {
            bestD = d;
        } else {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
    }
    for (int i = 1; i < n; ++i) {
        int64_t li = 0;
        double d = 0.0;
        double cur = 0.0;
        bool curLong = false;
        if (getLong(i, &li)) {
            curLong = true;
            cur = static_cast<double>(li);
        } else if (getDouble(i, &d)) {
            cur = d;
        } else {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
        if (cur < bestD) {
            best = i;
            bestD = cur;
            bestLong = curLong;
            bestL = li;
        }
    }
    (void)best;
    if (bestLong && bestD == static_cast<double>(bestL)) {
        int64_t li = 0;
        if (getLong(best, &li)) {
            return HsTuple(li);
        }
    }
    return HsTuple(bestD);
}

HsTuple HsTuple::max() const
{
    const int n = length();
    if (n == 0) {
        raiseException(HsTupleAccessException(HS_ERR_INDEX));
    }
    int best = 0;
    double bestD = 0.0;
    bool bestLong = false;
    int64_t bestL = 0;
    {
        int64_t li = 0;
        double d = 0.0;
        if (getLong(0, &li)) {
            bestLong = true;
            bestL = li;
            bestD = static_cast<double>(li);
        } else if (getDouble(0, &d)) {
            bestD = d;
        } else {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
    }
    for (int i = 1; i < n; ++i) {
        int64_t li = 0;
        double d = 0.0;
        double cur = 0.0;
        bool curLong = false;
        if (getLong(i, &li)) {
            curLong = true;
            cur = static_cast<double>(li);
        } else if (getDouble(i, &d)) {
            cur = d;
        } else {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
        if (cur > bestD) {
            best = i;
            bestD = cur;
            bestLong = curLong;
            bestL = li;
        }
    }
    (void)bestLong;
    (void)bestL;
    int64_t li = 0;
    double d = 0.0;
    if (getLong(best, &li)) {
        return HsTuple(li);
    }
    if (getDouble(best, &d)) {
        return HsTuple(d);
    }
    raiseException(HsTupleAccessException(HS_ERR_TYPE));
    return HsTuple();
}

HsTuple HsTuple::mean() const
{
    const int n = length();
    if (n == 0) {
        raiseException(HsTupleAccessException(HS_ERR_INDEX));
    }
    const HsTuple s = sum();
    double v = 0.0;
    int64_t li = 0;
    if (s.getLong(0, &li)) {
        v = static_cast<double>(li);
    } else if (!s.getDouble(0, &v)) {
        raiseException(HsTupleAccessException(HS_ERR_TYPE));
    }
    return HsTuple(v / static_cast<double>(n));
}

HsTuple HsTuple::toLong() const
{
    HsTuple out;
    const int n = length();
    for (int i = 0; i < n; ++i) {
        int64_t li = 0;
        double d = 0.0;
        if (getLong(i, &li)) {
            out.append(li);
        } else if (getDouble(i, &d)) {
            out.append(static_cast<int64_t>(d));
        } else {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
    }
    return out;
}

HsTuple HsTuple::toDouble() const
{
    HsTuple out;
    const int n = length();
    for (int i = 0; i < n; ++i) {
        int64_t li = 0;
        double d = 0.0;
        if (getLong(i, &li)) {
            out.append(static_cast<double>(li));
        } else if (getDouble(i, &d)) {
            out.append(d);
        } else {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
    }
    return out;
}

HsTuple HsTuple::abs() const
{
    HsTuple out;
    const int n = length();
    for (int i = 0; i < n; ++i) {
        int64_t li = 0;
        double d = 0.0;
        if (getLong(i, &li)) {
            if (li == INT64_MIN) {
                raiseException(HsTupleAccessException(HS_ERR_INT_RANGE));
            }
            out.append(li < 0 ? -li : li);
        } else if (getDouble(i, &d)) {
            out.append(d < 0.0 ? -d : d);
        } else {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
    }
    return out;
}

HsTuple HsTuple::operator%(const HsTuple& other) const
{
    bool empty = false;
    const int na = length();
    const int nb = other.length();
    const int n = broadcastCount(na, nb, &empty);
    if (empty) {
        return HsTuple();
    }
    HsTuple out;
    for (int i = 0; i < n; ++i) {
        int ia = 0;
        int ib = 0;
        pairIndex(i, na, nb, &ia, &ib);
        int64_t a = 0;
        int64_t b = 0;
        if (!getLong(ia, &a) || !other.getLong(ib, &b)) {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
        if (b == 0) {
            raiseException(HsTupleAccessException(HS_ERR_TYPE));
        }
        out.append(a % b);
    }
    return out;
}

}


