/**
 * @file HtmlReportWriter.h
 * @brief 将 ReportDocument 写成 UTF-8 HTML（图表为 SVG）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HTMLREPORTWRITER_H
#define VISION_HTMLREPORTWRITER_H

#include "ReportDocument.h"
#include <string>

namespace Vision {

/**
 * @class HtmlReportWriter
 * @brief ReportDocument → HTML 写出器
 */
class HtmlReportWriter {
public:
    /** @brief 绑定要写出的文档 */
    explicit HtmlReportWriter(const ReportDocument& doc);
    /** @brief 写出到 path；失败时填写 error */
    bool save(const std::string& path, std::string* error);

private:
    /** @brief 组装完整 HTML 字符串 */
    std::string buildHtml() const;
    /** @brief 转义 HTML 特殊字符 */
    static std::string escape(const std::string& s);
    /** @brief 一块内容写成 HTML */
    std::string blockHtml(const ReportBlock& b) const;
    /** @brief 表格写成 HTML */
    std::string tableHtml(const ReportTable& t) const;
    /** @brief 图表写成内联 SVG */
    std::string chartSvg(const ReportChart& c) const;
    /** @brief 序列调色板颜色 */
    static const char* seriesColor(size_t index);

    const ReportDocument& m_doc; ///< 源文档
};

}

#endif
