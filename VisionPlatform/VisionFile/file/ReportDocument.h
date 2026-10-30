/**
 * @file ReportDocument.h
 * @brief 富文档：标题、段落、表格、折线/柱状图，可导出 HTML 与 PDF
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REPORTDOCUMENT_H
#define VISION_REPORTDOCUMENT_H
#include "VisionFileExport.h"

#include <string>
#include <vector>

namespace Vision {

/**
 * @class ReportChartType
 * @brief 图表类型
 */
enum class ReportChartType {
    Line = 0, ///< 折线图
    Bar = 1   ///< 柱状图
};

/**
 * @class ReportChartSeries
 * @brief 图表中的一条序列
 */
struct ReportChartSeries {
    std::string name;            ///< 图例名
    std::vector<double> values;  ///< 与 categories 对齐的数值
};

/**
 * @class ReportChart
 * @brief 折线或柱状图数据
 */
struct ReportChart {
    ReportChartType type = ReportChartType::Bar; ///< 图表类型
    std::string title;                           ///< 图标题，可空
    std::string yLabel;                          ///< Y 轴说明，可空
    std::vector<std::string> categories;         ///< X 轴类别
    std::vector<ReportChartSeries> series;       ///< 一条或多条序列
};

/**
 * @class ReportTable
 * @brief 报告中的一张表
 */
struct ReportTable {
    std::vector<std::string> headers;                 ///< 表头
    std::vector<std::vector<std::string> > rows;      ///< 数据行
};

/**
 * @class ReportBlockKind
 * @brief 报告块类型
 */
enum class ReportBlockKind {
    Heading = 0,   ///< 标题
    Paragraph = 1, ///< 段落
    Table = 2,     ///< 表格
    Chart = 3,     ///< 图表
    HRule = 4      ///< 分隔线
};

/**
 * @class ReportBlock
 * @brief 报告中的一块内容
 */
struct ReportBlock {
    ReportBlockKind kind = ReportBlockKind::Paragraph; ///< 块类型
    int headingLevel = 1;                              ///< 标题级别 1..3
    std::string text;                                  ///< 标题或段落文本
    ReportTable table;                                 ///< 表格，kind 为 Table 时有效
    ReportChart chart;                                 ///< 图表，kind 为 Chart 时有效
};

/**
 * @class ReportDocument
 * @brief 组装富文档并导出为 HTML / PDF。PDF 嵌入系统中文字体子集，不依赖第三方库
 */
class VISION_FILE_API ReportDocument {
public:
    /** @brief 清空标题与全部块 */
    void clear();
    /** @brief 设置文档标题，写入 HTML title 与 PDF 首页大标题 */
    void setTitle(const std::string& title);
    /** @brief 追加标题；level 限制在 1..3 */
    void addHeading(int level, const std::string& text);
    /** @brief 追加段落 */
    void addParagraph(const std::string& text);
    /** @brief 追加表格 */
    void addTable(const ReportTable& table);
    /** @brief 追加折线或柱状图 */
    void addChart(const ReportChart& chart);
    /** @brief 追加水平分隔线 */
    void addHRule();

    /** @brief 当前标题 */
    const std::string& title() const { return m_title; }
    /** @brief 全部内容块 */
    const std::vector<ReportBlock>& blocks() const { return m_blocks; }

    /** @brief 写出 UTF-8 HTML；图表为内联 SVG */
    bool saveHtml(const std::string& path, std::string* error) const;
    /** @brief 写出 PDF 1.4；中文用系统字体子集嵌入 */
    bool savePdf(const std::string& path, std::string* error) const;

private:
    std::string m_title;               ///< 文档标题
    std::vector<ReportBlock> m_blocks; ///< 内容块
};

}

#endif
