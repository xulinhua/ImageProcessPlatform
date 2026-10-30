/**
 * @file FileDocument.h
 * @brief FileDocument头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_FILEDOCUMENT_H
#define VISION_FILEDOCUMENT_H
#include "VisionFileExport.h"

#include <string>
#include <vector>

namespace Vision {

/**
 * @class ITableWriter
 * @brief 表格写出抽象：表头、行、保存到文件
 */
class ITableWriter {
public:
    /** @brief 虚析构 */
    virtual ~ITableWriter() {}
    /** @brief 设置列名 */
    virtual void setHeaders(const std::vector<std::string>& headers) = 0;
    /** @brief 追加一行 */
    virtual void addRow(const std::vector<std::string>& cells) = 0;
    /** @brief 写到路径；失败时填写 error */
    virtual bool save(const std::string& path, std::string* error) = 0;
};

/**
 * @class TxtDocument
 * @brief 制表符分隔的文本表，可带 UTF-8 BOM
 */
class VISION_FILE_API TxtDocument : public ITableWriter {
public:
    /** @brief 记下导出列名 */
    void setHeaders(const std::vector<std::string>& headers);
    /** @brief 缓存一行单元格 */
    void addRow(const std::vector<std::string>& cells);
    /** @brief 按制表符写出，并带 UTF-8 BOM */
    bool save(const std::string& path, std::string* error);
    /** @brief 覆盖写出 UTF-8 文本 */
    static bool writeText(const std::string& path, const std::string& utf8, bool withBom, std::string* error);
    /** @brief 在文件末尾追加 UTF-8 文本 */
    static bool appendText(const std::string& path, const std::string& utf8, std::string* error);

private:
    std::vector<std::string> m_headers; ///< 表头
    std::vector<std::vector<std::string> > m_rows; ///< 数据行
};

/**
 * @class CsvDocument
 * @brief 逗号分隔表
 */
class VISION_FILE_API CsvDocument : public ITableWriter {
public:
    /** @brief 记下导出列名 */
    void setHeaders(const std::vector<std::string>& headers);
    /** @brief 缓存一行单元格 */
    void addRow(const std::vector<std::string>& cells);
    /** @brief 按逗号写出 CSV */
    bool save(const std::string& path, std::string* error);
    /** @brief 含逗号、引号或换行时按 CSV 规则加引号 */
    static std::string escapeCell(const std::string& s);

private:
    std::vector<std::string> m_headers; ///< 表头
    std::vector<std::vector<std::string> > m_rows; ///< 数据行
};

/**
 * @class XlsxDocument
 * @brief 最小 xlsx：无压缩包加工作表 XML
 */
class VISION_FILE_API XlsxDocument : public ITableWriter {
public:
    /** @brief 记下导出列名 */
    void setHeaders(const std::vector<std::string>& headers);
    /** @brief 缓存一行单元格 */
    void addRow(const std::vector<std::string>& cells);
    /** @brief 打成 xlsx 文件 */
    bool save(const std::string& path, std::string* error);

private:
    std::vector<std::string> m_headers; ///< 表头
    std::vector<std::vector<std::string> > m_rows; ///< 数据行
};

/**
 * @class FileUtil
 * @brief 目录、原子写、按天清理。不依赖界面
 */
class VISION_FILE_API FileUtil {
public:
    /** @brief 目录不存在则逐级创建 */
    static bool ensureDir(const std::string& dir, std::string* error);
    /** @brief 先写临时文件再替换，避免半截内容 */
    static bool writeAtomic(const std::string& path, const std::string& bytes, std::string* error);
    /**
     * @brief 按最后写入时间删掉过期文件
     * @param pattern 文件名通配；空则 *.*
     * @return 删除的文件个数
     */
    static int deleteOlderThanDays(const std::string& dir, const std::string& pattern, int days);
};

}

#endif
