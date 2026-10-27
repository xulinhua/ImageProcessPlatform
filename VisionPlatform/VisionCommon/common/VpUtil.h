/**
 * @file VpUtil.h
 * @brief VpUtil头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_VPUTIL_H
#define VISION_VPUTIL_H

#include <cctype>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace Vision {

/// 去掉首尾空白
inline std::string vpTrim(const std::string& s)
{
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) {
        ++a;
    }
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) {
        --b;
    }
    return s.substr(a, b - a);
}

/** @brief ASCII 小写化 */
inline std::string vpLower(const std::string& s)
{
    std::string o = s;
    for (size_t i = 0; i < o.size(); ++i) {
        o[i] = static_cast<char>(tolower(static_cast<unsigned char>(o[i])));
    }
    return o;
}

/// 在 index 处插入
template<typename T>
inline void vpInsertAt(std::vector<T>& v, int i, const T& x)
{
    if (i < 0) {
        i = 0;
    }
    if (i > static_cast<int>(v.size())) {
        i = static_cast<int>(v.size());
    }
    v.insert(v.begin() + static_cast<size_t>(i), x);
}

/** @brief 按下标删除；越界忽略 */
template<typename T>
inline void vpRemoveAt(std::vector<T>& v, int i)
{
    if (i >= 0 && i < static_cast<int>(v.size())) {
        v.erase(v.begin() + static_cast<size_t>(i));
    }
}

/** @brief map 取值，缺键返回 def */
template<typename K, typename V>
inline V vpMapValue(const std::map<K, V>& m, const K& k, const V& def)
{
    typename std::map<K, V>::const_iterator it = m.find(k);
    if (it == m.end()) {
        return def;
    }
    return it->second;
}

}

#endif
