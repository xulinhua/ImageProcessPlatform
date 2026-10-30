/**
 * @file ReportDocument.cpp
 * @brief ReportDocument 内容组装
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ReportDocument.h"
#include "HtmlReportWriter.h"
#include "PdfReportWriter.h"

namespace Vision {

// 清空标题与全部块
void ReportDocument::clear()
{
    m_title.clear();
    m_blocks.clear();
}

// 设置文档标题
void ReportDocument::setTitle(const std::string& title)
{
    m_title = title;
}

// 追加标题；level 限制在 1..3
void ReportDocument::addHeading(int level, const std::string& text)
{
    ReportBlock b;
    b.kind = ReportBlockKind::Heading;
    b.headingLevel = level < 1 ? 1 : (level > 3 ? 3 : level);
    b.text = text;
    m_blocks.push_back(b);
}

// 追加段落
void ReportDocument::addParagraph(const std::string& text)
{
    ReportBlock b;
    b.kind = ReportBlockKind::Paragraph;
    b.text = text;
    m_blocks.push_back(b);
}

// 追加表格
void ReportDocument::addTable(const ReportTable& table)
{
    ReportBlock b;
    b.kind = ReportBlockKind::Table;
    b.table = table;
    m_blocks.push_back(b);
}

// 追加折线或柱状图
void ReportDocument::addChart(const ReportChart& chart)
{
    ReportBlock b;
    b.kind = ReportBlockKind::Chart;
    b.chart = chart;
    m_blocks.push_back(b);
}

// 追加水平分隔线
void ReportDocument::addHRule()
{
    ReportBlock b;
    b.kind = ReportBlockKind::HRule;
    m_blocks.push_back(b);
}

// 写出 UTF-8 HTML
bool ReportDocument::saveHtml(const std::string& path, std::string* error) const
{
    HtmlReportWriter w(*this);
    return w.save(path, error);
}

// 写出 PDF
bool ReportDocument::savePdf(const std::string& path, std::string* error) const
{
    PdfReportWriter w(*this);
    return w.save(path, error);
}

}
