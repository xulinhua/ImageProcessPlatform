/**
 * @file Xml.h
 * @brief Xml头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_XML_H
#define VISION_XML_H
#include "VisionFileExport.h"

#include <map>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class XmlValue
 * @brief 一个 XML 元素。名字按原文保留，不展开命名空间
 */
class VISION_FILE_API XmlValue {
public:
    XmlValue(); ///< 空元素，没有名字

    /**
     * @brief 解析文档并取根元素
     * @param error 失败原因；可为 0
     * @return 失败时为空元素
     */
    static XmlValue parse(const std::string& text, std::string* error);
    /**
     * @brief 序列化成 UTF-8 文档，带 XML 声明
     * @param pretty true 时子元素换行缩进
     */
    static std::string stringify(const XmlValue& v, bool pretty = true);

    /** @brief 还没有元素名 */
    bool isEmpty() const;
    /** @brief 元素名 */
    const std::string& name() const;
    /** @brief 设置元素名 */
    void setName(const std::string& elementName);

    /** @brief 元素文本，两端空白已去掉 */
    const std::string& text() const;
    /** @brief 设置元素文本 */
    void setText(const std::string& text);

    /** @brief 是否有该属性 */
    bool hasAttr(const std::string& key) const;
    /** @brief 读属性；没有则返回 def */
    std::string attr(const std::string& key, const std::string& def = std::string()) const;
    /** @brief 写属性 */
    void setAttr(const std::string& key, const std::string& value);
    /** @brief 全部属性 */
    const std::map<std::string, std::string>& attrs() const;

    /** @brief 子元素个数 */
    int childCount() const;
    /** @brief 第 i 个子元素；越界返回空元素 */
    XmlValue childAt(int i) const;
    /** @brief 第一个同名子元素；没有则空元素 */
    XmlValue child(const std::string& elementName) const;
    /** @brief 是否有同名子元素 */
    bool hasChild(const std::string& elementName) const;
    /** @brief 追加子元素 */
    void appendChild(const XmlValue& child);

private:
    std::string m_name; ///< 元素名
    std::string m_text; ///< 文本
    std::map<std::string, std::string> m_attr; ///< 属性
    std::vector<XmlValue> m_children; ///< 子元素
};

}

#endif