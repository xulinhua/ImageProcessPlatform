/**
 * @file ParamValue.h
 * @brief ParamValue头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PARAMVALUE_H
#define VISION_PARAMVALUE_H

#include "VisionCommonExport.h"
#include <cstdlib>
#include <cstdio>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class ParamValue
 * @brief 工具或设备参数取值（无 Qt）
 */
class ParamValue {
public:
    /** @brief 存哪种字面量 */
    enum Kind {
        KindNull = 0, ///< 未设置
        KindBool,     ///< 布尔
        KindInt,      ///< 整数
        KindDouble,   ///< 浮点
        KindString    ///< 文本
    };

    ParamValue() : m_kind(KindNull), m_b(false), m_i(0), m_d(0) {}
    ParamValue(bool v) : m_kind(KindBool), m_b(v), m_i(v ? 1 : 0), m_d(v ? 1.0 : 0.0) {}
    ParamValue(int v) : m_kind(KindInt), m_b(v != 0), m_i(v), m_d(static_cast<double>(v)) {}
    ParamValue(double v) : m_kind(KindDouble), m_b(v != 0.0), m_i(static_cast<int>(v)), m_d(v) {}
    ParamValue(const char* v) : m_kind(KindString), m_b(false), m_i(0), m_d(0), m_s(v ? v : "") {}
    ParamValue(const std::string& v) : m_kind(KindString), m_b(false), m_i(0), m_d(0), m_s(v) {}

    /** @brief 当前种类 */
    Kind kind() const { return m_kind; }
    /** @brief 是否未设置 */
    bool isNull() const { return m_kind == KindNull; }

    /**
     * @brief 转布尔；空值返回 def。字符串认 "1"/"true"/"True"
     */
    VISION_COMMON_API bool toBool(bool def = false) const;

    /**
     * @brief 转整数；空值返回 def。字符串走 atoi
     */
    VISION_COMMON_API int toInt(int def = 0) const;

    /**
     * @brief 转浮点；空值返回 def。字符串走 atof
     */
    VISION_COMMON_API double toDouble(double def = 0) const;

    /** @brief 转文本；空值为空串，数字用 sprintf_s */
    VISION_COMMON_API std::string toString() const;


private:
    Kind m_kind;      ///< 当前种类
    bool m_b;         ///< 布尔/数值非零
    int m_i;          ///< 整数镜像
    double m_d;       ///< 浮点镜像
    std::string m_s;  ///< 文本
};

/**
 * @class ParamMap
 * @brief 参数表（key 到 ParamValue）。工具 params、设备 defaultParams 用此，替代 QVariantMap
 */
class ParamMap {
public:
    /** @brief 写入或覆盖 */
    void insert(const std::string& key, const ParamValue& v) { m[key] = v; }
    /** @brief 读取；缺键返回空 ParamValue */
    VISION_COMMON_API ParamValue value(const std::string& key) const;

    /** @brief 是否含键 */
    bool contains(const std::string& key) const { return m.find(key) != m.end(); }
    /** @brief 清空 */
    void clear() { m.clear(); }
    /** @brief 条目数 */
    int size() const { return static_cast<int>(m.size()); }
    /** @brief 全部键（map 序） */
    VISION_COMMON_API std::vector<std::string> keys() const;

    /** @brief 可写底层 map（序列化用） */
    std::map<std::string, ParamValue>& raw() { return m; }
    /** @brief 只读底层 map */
    const std::map<std::string, ParamValue>& raw() const { return m; }

private:
    std::map<std::string, ParamValue> m; ///< 键值表
};

}

#endif
