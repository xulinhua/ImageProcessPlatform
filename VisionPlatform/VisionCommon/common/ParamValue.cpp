/**
 * @file ParamValue.cpp
 * @brief ParamValue实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ParamValue.h"

namespace Vision {

// 转布尔；空值返回 def。字符串认 "1"/"true"/"True"
bool ParamValue::toBool(bool def) const
{
        if (m_kind == KindNull) {
            return def;
        }
        if (m_kind == KindString) {
            return m_s == "1" || m_s == "true" || m_s == "True";
        }
        return m_b;
    }

// 转整数；空值返回 def。字符串走 atoi
int ParamValue::toInt(int def) const
{
        if (m_kind == KindNull) {
            return def;
        }
        if (m_kind == KindString) {
            return atoi(m_s.c_str());
        }
        return m_i;
    }

// 转浮点；空值返回 def。字符串走 atof
double ParamValue::toDouble(double def) const
{
        if (m_kind == KindNull) {
            return def;
        }
        if (m_kind == KindString) {
            return atof(m_s.c_str());
        }
        return m_d;
    }

// 转文本；空值为空串，数字用 sprintf_s
std::string ParamValue::toString() const
{
        if (m_kind == KindString) {
            return m_s;
        }
        if (m_kind == KindBool) {
            return m_b ? "true" : "false";
        }
        if (m_kind == KindInt) {
            char buf[32];
            sprintf_s(buf, "%d", m_i);
            return std::string(buf);
        }
        if (m_kind == KindDouble) {
            char buf[64];
            sprintf_s(buf, "%.17g", m_d);
            return std::string(buf);
        }
        return std::string();
    }

// 读取；缺键返回空 ParamValue
ParamValue ParamMap::value(const std::string& key) const
{
        std::map<std::string, ParamValue>::const_iterator it = m.find(key);
        if (it == m.end()) {
            return ParamValue();
        }
        return it->second;
    }

// 全部键（map 序）
std::vector<std::string> ParamMap::keys() const
{
        std::vector<std::string> k;
        for (std::map<std::string, ParamValue>::const_iterator it = m.begin(); it != m.end(); ++it) {
            k.push_back(it->first);
        }
        return k;
    }

}
