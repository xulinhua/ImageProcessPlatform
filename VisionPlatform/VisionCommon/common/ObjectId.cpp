/**
 * @file ObjectId.cpp
 * @brief ObjectId实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ObjectId.h"
#include <rpc.h>
#include <windows.h>

#pragma comment(lib, "rpcrt4.lib")

namespace Vision {

// 去掉 GUID 字符串两端的花括号
std::string ObjectId::stripBraces(std::string s)
{
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '{' && s[i] != '}') {
            o.push_back(s[i]);
        }
    }
    return o;
}

// 用去掉花括号后的字符串构造
ObjectId::ObjectId(const std::string& s)
    : m_s(stripBraces(s))
{
}

// 从字符串构造 ObjectId
ObjectId ObjectId::fromString(const std::string& s)
{
    return ObjectId(s);
}

// 用 Windows UUID 生成新 ID
ObjectId ObjectId::create()
{
    UUID u;
    UuidCreate(&u);
    RPC_CSTR str = 0;
    if (UuidToStringA(&u, &str) != RPC_S_OK || !str) {
        return ObjectId();
    }
    const std::string s(reinterpret_cast<char*>(str));
    RpcStringFreeA(&str);
    return ObjectId(s);
}

// 是否为空
bool ObjectId::isNull() const
{
    return m_s.empty();
}

// 转为文本
const std::string& ObjectId::toString() const
{
    return m_s;
}

// 相等
bool ObjectId::operator==(const ObjectId& o) const
{
    return m_s == o.m_s;
}

// 不等
bool ObjectId::operator!=(const ObjectId& o) const
{
    return m_s != o.m_s;
}

// 小于
bool ObjectId::operator<(const ObjectId& o) const
{
    return m_s < o.m_s;
}



}
