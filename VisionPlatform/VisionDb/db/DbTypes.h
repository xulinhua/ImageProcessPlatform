/**
 * @file DbTypes.h
 * @brief DbTypes头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DBTYPES_H
#define VISION_DBTYPES_H
#include "VisionDbExport.h"

#include <stdint.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class DbValue
 * @brief SQL 绑定或读取的一个单元格
 */
struct VISION_DB_API DbValue {
    /** @brief 单元格存的类型 */
    enum Kind {
        Null = 0, ///< 空
        Int,      ///< 整数
        Text,     ///< 文本
        Blob      ///< 二进制
    };
    Kind kind;                       ///< 单元格类型
    int64_t i;                       ///< 整数值
    std::string text;                ///< 文本
    std::vector<unsigned char> blob; ///< 二进制
    /** @brief 空值 */
    DbValue() : kind(Null), i(0) {}
    /** @brief 整数单元格 */
    static DbValue fromInt(int64_t v)
    {
        DbValue d;
        d.kind = Int;
        d.i = v;
        return d;
    }
    /** @brief 文本单元格（UTF-8） */
    static DbValue fromText(const std::string& v)
    {
        DbValue d;
        d.kind = Text;
        d.text = v;
        return d;
    }
    /** @brief 二进制单元格 */
    static DbValue fromBlob(const std::vector<unsigned char>& v)
    {
        DbValue d;
        d.kind = Blob;
        d.blob = v;
        return d;
    }
};

/**
 * @class PageQuery
 * @brief 分页查询：页码、页大小、排序
 */
struct PageQuery {
    int page;            ///< 从 1 起
    int pageSize;        ///< 每页行数
    std::string orderBy; ///< 可空
    /** @brief 第 1 页，每页 50 行 */
    PageQuery() : page(1), pageSize(50) {}
};

/**
 * @class PageRst
 * @brief 分页结果：总行数和本页行
 */
struct PageRst {
    int total;                              ///< 符合条件的总行数
    std::vector<std::vector<DbValue> > rows; ///< 本页行
    /** @brief 空结果 */
    PageRst() : total(0) {}
};

}

#endif
