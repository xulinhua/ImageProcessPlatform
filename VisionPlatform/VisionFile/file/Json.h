/**
 * @file Json.h
 * @brief Json头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_JSON_H
#define VISION_JSON_H
#include "VisionFileExport.h"

#include <map>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class JsonValue
 * @brief 自研 JSON（C++11）。ClassLib 禁止 QJson。支持 null/bool/number/string/array/object
 */
class VISION_FILE_API JsonValue {
public:
    /** @brief JSON 值种类 */
    enum Type {
        Null = 0, ///< 空
        Bool,     ///< 布尔
        Number,   ///< 数字（内部 double）
        String,   ///< 文本
        Array,    ///< 数组
        Object    ///< 对象
    };

    JsonValue();                         ///< null
    JsonValue(bool v);                   ///< 布尔
    JsonValue(int v);                    ///< 整数（存成 number）
    JsonValue(double v);                 ///< 浮点
    JsonValue(const char* v);            ///< C 串
    JsonValue(const std::string& v);     ///< 文本

    /** @brief 当前种类 */
    Type type() const;
    /** @brief 是否 null */
    bool isNull() const;
    /** @brief 是否对象 */
    bool isObject() const;
    /** @brief 是否数组 */
    bool isArray() const;
    /** @brief 是否文本 */
    bool isString() const;
    /** @brief 是否数字 */
    bool isNumber() const;
    /** @brief 是否布尔 */
    bool isBool() const;

    /** @brief 转布尔；种类不对返回 def */
    bool toBool(bool def = false) const;
    /** @brief 转整数 */
    int toInt(int def = 0) const;
    /** @brief 转浮点 */
    double toDouble(double def = 0) const;
    /** @brief 转文本 */
    std::string toString() const;

    /** @brief 数组长度；非数组为 0 */
    int size() const;
    /** @brief 数组第 i 项；越界返回 null */
    JsonValue at(int i) const;
    /** @brief 变成空数组 */
    void makeArray();
    /** @brief 变成空对象 */
    void makeObject();
    /** @brief 追加到数组（必要时先 makeArray） */
    void append(const JsonValue& v);

    /** @brief 对象是否含键 */
    bool contains(const std::string& key) const;
    /** @brief 对象取值；缺键返回 null */
    JsonValue value(const std::string& key) const;
    /** @brief 对象写键（必要时先 makeObject） */
    void set(const std::string& key, const JsonValue& v);

    /** @brief 对象字段转浮点；缺键用 def */
    double number(const std::string& key, double def = 0) const;
    /** @brief 对象字段转整数；缺键用 def */
    int integerOf(const std::string& key, int def = 0) const;
    /** @brief 对象字段转布尔；缺键用 def */
    bool flagOf(const std::string& key, bool def = false) const;
    /** @brief 对象字段转文本；缺键为空串 */
    std::string stringOf(const std::string& key) const;

    /** @brief 底层数组 */
    const std::vector<JsonValue>& array() const;
    /** @brief 底层对象 */
    const std::map<std::string, JsonValue>& object() const;

    /**
     * @brief 解析文本
     * @param error 失败原因
     */
    static JsonValue parse(const std::string& text, std::string* error);
    /**
     * @brief 序列化
     * @param pretty true 带缩进
     */
    static std::string stringify(const JsonValue& v, bool pretty = true);

private:
    Type m_type;                         ///< 种类
    bool m_bool;                         ///< 布尔
    double m_num;                        ///< 数字
    std::string m_str;                   ///< 文本
    std::vector<JsonValue> m_arr;        ///< 数组
    std::map<std::string, JsonValue> m_obj; ///< 对象
};

}

#endif
