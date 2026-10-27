/**
 * @file ObjectId.h
 * @brief ObjectId头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OBJECTID_H
#define VISION_OBJECTID_H
#include "VisionCommonExport.h"

#include <string>

namespace Vision {

/**
 * @class ObjectId
 * @brief 对象稳定标识，内部为去掉花括号的 UUID 文本
 */
class VISION_COMMON_API ObjectId {
public:
    ObjectId() {}
    /** @brief 用字符串构造，并去掉花括号 */
    explicit ObjectId(const std::string& s);

    /** @brief 生成新的 UUID */
    static ObjectId create();
    /** @brief 从字符串还原 */
    static ObjectId fromString(const std::string& s);

    bool isNull() const;
    /** @brief 标识文本 */
    const std::string& toString() const;
    /** @brief 文本是否相同 */
    bool operator==(const ObjectId& o) const;
    /** @brief 文本是否不同 */
    bool operator!=(const ObjectId& o) const;
    /** @brief 按文本排序 */
    bool operator<(const ObjectId& o) const;

private:
    /** @brief 去掉花括号 */
    static std::string stripBraces(std::string s);
    std::string m_s; ///< 标识文本
};

}

#endif