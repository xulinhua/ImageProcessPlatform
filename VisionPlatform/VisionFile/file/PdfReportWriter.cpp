/**
 * @file PdfReportWriter.cpp
 * @brief PdfReportWriter 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PdfReportWriter.h"
#include "FileDocument.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace Vision {

/************************************************************
 * 构造与入口
 ************************************************************/

PdfReportWriter::PdfReportWriter(const ReportDocument& doc)
    : m_doc(doc)
{
    m_contentW = m_pageW - m_margin * 2.0;
}

bool PdfReportWriter::save(const std::string& path, std::string* error)
{
    if (!prepareFont(error)) {
        return false;
    }
    if (!layout(error)) {
        return false;
    }
    std::vector<unsigned char> bytes;
    if (!buildFile(&bytes, error)) {
        return false;
    }
    return FileUtil::writeAtomic(path, std::string(bytes.begin(), bytes.end()), error);
}

/************************************************************
 * 码位与字体
 ************************************************************/

void PdfReportWriter::utf8ToCodepoints(const std::string& utf8, std::vector<uint32_t>* out)
{
    if (!out) {
        return;
    }
    size_t i = 0;
    while (i < utf8.size()) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        uint32_t cp = 0;
        size_t n = 1;
        if (c < 0x80) {
            cp = c;
            n = 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size()) {
            cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(utf8[i + 1]) & 0x3F);
            n = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < utf8.size()) {
            cp = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 6)
                | (static_cast<unsigned char>(utf8[i + 2]) & 0x3F);
            n = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < utf8.size()) {
            cp = ((c & 0x07) << 18) | ((static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 12)
                | ((static_cast<unsigned char>(utf8[i + 2]) & 0x3F) << 6)
                | (static_cast<unsigned char>(utf8[i + 3]) & 0x3F);
            n = 4;
        } else {
            cp = '?';
            n = 1;
        }
        out->push_back(cp);
        i += n;
    }
}

void PdfReportWriter::collectCodepoints(std::vector<uint32_t>* out) const
{
    utf8ToCodepoints(m_doc.title(), out);
    const std::vector<ReportBlock>& blocks = m_doc.blocks();
    for (size_t i = 0; i < blocks.size(); ++i) {
        const ReportBlock& b = blocks[i];
        utf8ToCodepoints(b.text, out);
        if (b.kind == ReportBlockKind::Table) {
            for (size_t h = 0; h < b.table.headers.size(); ++h) {
                utf8ToCodepoints(b.table.headers[h], out);
            }
            for (size_t r = 0; r < b.table.rows.size(); ++r) {
                for (size_t c = 0; c < b.table.rows[r].size(); ++c) {
                    utf8ToCodepoints(b.table.rows[r][c], out);
                }
            }
        } else if (b.kind == ReportBlockKind::Chart) {
            utf8ToCodepoints(b.chart.title, out);
            utf8ToCodepoints(b.chart.yLabel, out);
            for (size_t c = 0; c < b.chart.categories.size(); ++c) {
                utf8ToCodepoints(b.chart.categories[c], out);
            }
            for (size_t s = 0; s < b.chart.series.size(); ++s) {
                utf8ToCodepoints(b.chart.series[s].name, out);
            }
            // 刻度数字
            static const char* kDigits = "0123456789.-+eE";
            for (const char* p = kDigits; *p; ++p) {
                out->push_back(static_cast<uint32_t>(*p));
            }
        }
    }
    // 常用空白
    out->push_back(0x20);
    out->push_back(0x0A);
}

bool PdfReportWriter::prepareFont(std::string* error)
{
    std::vector<unsigned char> fontBytes;
    if (!TtfSubset::loadSystemCjkFont(&fontBytes, error)) {
        return false;
    }
    if (!m_font.load(fontBytes, error)) {
        return false;
    }
    std::vector<uint32_t> cps;
    collectCodepoints(&cps);
    // ASCII 基本集，避免刻度/标点缺字
    for (uint32_t c = 0x20; c <= 0x7E; ++c) {
        cps.push_back(c);
    }
    return m_font.build(cps, &m_subsetTtf, &m_gidOf, &m_advanceOf, &m_unitsPerEm, error);
}

/************************************************************
 * 排版辅助
 ************************************************************/

std::string PdfReportWriter::hexCidString(const std::string& utf8) const
{
    std::vector<uint32_t> cps;
    utf8ToCodepoints(utf8, &cps);
    std::string o;
    o.push_back('<');
    for (size_t i = 0; i < cps.size(); ++i) {
        uint16_t gid = 0;
        std::map<uint32_t, uint16_t>::const_iterator it = m_gidOf.find(cps[i]);
        if (it != m_gidOf.end()) {
            gid = it->second;
        }
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%04X", gid);
        o += buf;
    }
    o.push_back('>');
    return o;
}

double PdfReportWriter::textWidth(const std::string& utf8, double fontSize) const
{
    std::vector<uint32_t> cps;
    utf8ToCodepoints(utf8, &cps);
    double w = 0.0;
    for (size_t i = 0; i < cps.size(); ++i) {
        uint16_t gid = 0;
        std::map<uint32_t, uint16_t>::const_iterator it = m_gidOf.find(cps[i]);
        if (it != m_gidOf.end()) {
            gid = it->second;
        }
        uint16_t adv = 0;
        std::map<uint16_t, uint16_t>::const_iterator a = m_advanceOf.find(gid);
        if (a != m_advanceOf.end()) {
            adv = a->second;
        }
        w += adv * fontSize / static_cast<double>(m_unitsPerEm);
    }
    return w;
}

void PdfReportWriter::wrapText(const std::string& utf8, double fontSize, double maxWidth,
                               std::vector<std::string>* lines) const
{
    lines->clear();
    if (utf8.empty()) {
        lines->push_back(std::string());
        return;
    }
    std::vector<uint32_t> cps;
    utf8ToCodepoints(utf8, &cps);
    std::string line;
    double lineW = 0.0;
    size_t i = 0;
    while (i < cps.size()) {
        // 取出一个码位的 UTF-8
        const uint32_t cp = cps[i];
        std::string ch;
        if (cp < 0x80) {
            ch.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            ch.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            ch.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            ch.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            ch.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            ch.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            ch.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            ch.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            ch.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            ch.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        const double cw = textWidth(ch, fontSize);
        const bool breakable = (cp >= 0x4E00) || cp == 0x20 || cp == 0x09;
        if (!line.empty() && lineW + cw > maxWidth && breakable) {
            if (cp == 0x20 || cp == 0x09) {
                lines->push_back(line);
                line.clear();
                lineW = 0.0;
                ++i;
                continue;
            }
            lines->push_back(line);
            line = ch;
            lineW = cw;
            ++i;
            continue;
        }
        if (!line.empty() && lineW + cw > maxWidth) {
            lines->push_back(line);
            line = ch;
            lineW = cw;
        } else {
            line += ch;
            lineW += cw;
        }
        ++i;
    }
    if (!line.empty() || lines->empty()) {
        lines->push_back(line);
    }
}

void PdfReportWriter::appendNum(std::string* s, double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.3f", v);
    *s += buf;
}

void PdfReportWriter::seriesRgb(size_t index, double* r, double* g, double* b)
{
    static const double kRgb[][3] = {
        { 0.145, 0.388, 0.922 }, { 0.863, 0.149, 0.149 }, { 0.086, 0.639, 0.290 },
        { 0.792, 0.541, 0.016 }, { 0.486, 0.227, 0.929 }, { 0.031, 0.569, 0.698 },
        { 0.918, 0.345, 0.047 }, { 0.294, 0.333, 0.388 }
    };
    const size_t n = sizeof(kRgb) / sizeof(kRgb[0]);
    *r = kRgb[index % n][0];
    *g = kRgb[index % n][1];
    *b = kRgb[index % n][2];
}

void PdfReportWriter::newPage()
{
    Page p;
    m_pages.push_back(p);
    m_y = m_pageH - m_margin;
}

void PdfReportWriter::ensureSpace(double need)
{
    if (m_pages.empty()) {
        newPage();
    }
    if (m_y - need < m_margin) {
        newPage();
    }
}

void PdfReportWriter::drawTextLine(const std::string& utf8, double fontSize, bool /*bold*/)
{
    ensureSpace(fontSize + 4.0);
    std::string& c = m_pages.back().content;
    c += "BT /F1 ";
    appendNum(&c, fontSize);
    c += " Tf ";
    appendNum(&c, m_margin);
    c += " ";
    appendNum(&c, m_y - fontSize);
    c += " Td ";
    c += hexCidString(utf8);
    c += " Tj ET\n";
    m_y -= fontSize + 4.0;
}

/************************************************************
 * 表格与图表
 ************************************************************/

void PdfReportWriter::drawTable(const ReportTable& table)
{
    size_t cols = table.headers.size();
    for (size_t r = 0; r < table.rows.size(); ++r) {
        cols = (std::max)(cols, table.rows[r].size());
    }
    if (cols == 0) {
        return;
    }
    const double fontSize = 9.0;
    const double rowH = 18.0;
    const double colW = m_contentW / static_cast<double>(cols);
    const size_t totalRows = (table.headers.empty() ? 0 : 1) + table.rows.size();
    ensureSpace(rowH * static_cast<double>(totalRows) + 8.0);

    auto drawRow = [&](const std::vector<std::string>& cells, bool header) {
        ensureSpace(rowH);
        std::string& c = m_pages.back().content;
        const double yBottom = m_y - rowH;
        // 背景
        if (header) {
            c += "0.953 0.957 0.965 rg ";
            appendNum(&c, m_margin);
            c += " ";
            appendNum(&c, yBottom);
            c += " ";
            appendNum(&c, m_contentW);
            c += " ";
            appendNum(&c, rowH);
            c += " re f\n";
        }
        // 网格
        c += "0.82 0.835 0.859 RG 0.5 w\n";
        for (size_t i = 0; i <= cols; ++i) {
            const double x = m_margin + i * colW;
            appendNum(&c, x);
            c += " ";
            appendNum(&c, yBottom);
            c += " m ";
            appendNum(&c, x);
            c += " ";
            appendNum(&c, m_y);
            c += " l S\n";
        }
        appendNum(&c, m_margin);
        c += " ";
        appendNum(&c, yBottom);
        c += " m ";
        appendNum(&c, m_margin + m_contentW);
        c += " ";
        appendNum(&c, yBottom);
        c += " l S\n";
        appendNum(&c, m_margin);
        c += " ";
        appendNum(&c, m_y);
        c += " m ";
        appendNum(&c, m_margin + m_contentW);
        c += " ";
        appendNum(&c, m_y);
        c += " l S\n";

        for (size_t i = 0; i < cols; ++i) {
            const std::string text = i < cells.size() ? cells[i] : std::string();
            // 裁到列宽
            std::string shown = text;
            while (shown.size() > 0 && textWidth(shown, fontSize) > colW - 8.0) {
                // 去掉最后一个码位
                std::vector<uint32_t> cps;
                utf8ToCodepoints(shown, &cps);
                if (cps.empty()) {
                    break;
                }
                cps.pop_back();
                shown.clear();
                for (size_t k = 0; k < cps.size(); ++k) {
                    const uint32_t cp = cps[k];
                    if (cp < 0x80) {
                        shown.push_back(static_cast<char>(cp));
                    } else if (cp < 0x800) {
                        shown.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        shown.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else if (cp < 0x10000) {
                        shown.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        shown.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        shown.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else {
                        shown.push_back('?');
                    }
                }
            }
            c += "BT /F1 ";
            appendNum(&c, fontSize);
            c += " Tf ";
            appendNum(&c, m_margin + i * colW + 4.0);
            c += " ";
            appendNum(&c, yBottom + 5.0);
            c += " Td ";
            c += hexCidString(shown);
            c += " Tj ET\n";
        }
        m_y = yBottom;
    };

    if (!table.headers.empty()) {
        drawRow(table.headers, true);
    }
    for (size_t r = 0; r < table.rows.size(); ++r) {
        drawRow(table.rows[r], false);
    }
    m_y -= 8.0;
}

void PdfReportWriter::drawChart(const ReportChart& chart)
{
    const double chartH = 220.0;
    const double titleH = chart.title.empty() ? 0.0 : 18.0;
    ensureSpace(chartH + titleH + 24.0);
    if (!chart.title.empty()) {
        drawTextLine(chart.title, 11.0, true);
    }

    const double L = m_margin + 40.0;
    const double R = m_margin + m_contentW;
    const double T = m_y;
    const double B = m_y - chartH + 28.0;
    const double plotW = R - L;
    const double plotH = T - B - 20.0;

    size_t nCat = chart.categories.size();
    for (size_t s = 0; s < chart.series.size(); ++s) {
        nCat = (std::max)(nCat, chart.series[s].values.size());
    }
    if (nCat == 0) {
        nCat = 1;
    }

    double vmin = 0.0;
    double vmax = 0.0;
    bool any = false;
    for (size_t s = 0; s < chart.series.size(); ++s) {
        for (size_t i = 0; i < chart.series[s].values.size(); ++i) {
            const double v = chart.series[s].values[i];
            if (!any || v < vmin) {
                vmin = v;
            }
            if (!any || v > vmax) {
                vmax = v;
            }
            any = true;
        }
    }
    if (!any) {
        vmax = 1.0;
    }
    if (vmin > 0.0) {
        vmin = 0.0;
    }
    if (vmax <= vmin) {
        vmax = vmin + 1.0;
    }
    const double span = vmax - vmin;

    std::string& c = m_pages.back().content;
    // 坐标轴
    c += "0.61 0.64 0.69 RG 1 w\n";
    appendNum(&c, L);
    c += " ";
    appendNum(&c, B);
    c += " m ";
    appendNum(&c, L);
    c += " ";
    appendNum(&c, B + plotH);
    c += " l S\n";
    appendNum(&c, L);
    c += " ";
    appendNum(&c, B);
    c += " m ";
    appendNum(&c, L + plotW);
    c += " ";
    appendNum(&c, B);
    c += " l S\n";

    for (int g = 0; g <= 4; ++g) {
        const double t = g / 4.0;
        const double y = B + t * plotH;
        c += "0.95 0.95 0.96 RG 0.5 w ";
        appendNum(&c, L);
        c += " ";
        appendNum(&c, y);
        c += " m ";
        appendNum(&c, L + plotW);
        c += " ";
        appendNum(&c, y);
        c += " l S\n";
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.4g", vmin + span * t);
        c += "BT /F1 8 Tf ";
        appendNum(&c, L - 4.0 - textWidth(buf, 8.0));
        c += " ";
        appendNum(&c, y - 3.0);
        c += " Td ";
        c += hexCidString(buf);
        c += " Tj ET\n";
    }

    const size_t nSeries = chart.series.size();
    if (chart.type == ReportChartType::Bar) {
        const double groupW = plotW / static_cast<double>(nCat);
        const double barW = groupW / static_cast<double>((std::max)(size_t(1), nSeries) + 1.0);
        for (size_t s = 0; s < nSeries; ++s) {
            double r, g, b;
            seriesRgb(s, &r, &g, &b);
            appendNum(&c, r);
            c += " ";
            appendNum(&c, g);
            c += " ";
            appendNum(&c, b);
            c += " rg\n";
            for (size_t i = 0; i < nCat; ++i) {
                const double v = i < chart.series[s].values.size() ? chart.series[s].values[i] : 0.0;
                const double h = (v - vmin) / span * plotH;
                const double x = L + i * groupW + (s + 0.5) * barW;
                appendNum(&c, x);
                c += " ";
                appendNum(&c, B);
                c += " ";
                appendNum(&c, barW * 0.9);
                c += " ";
                appendNum(&c, h);
                c += " re f\n";
            }
        }
    } else {
        for (size_t s = 0; s < nSeries; ++s) {
            double r, g, b;
            seriesRgb(s, &r, &g, &b);
            appendNum(&c, r);
            c += " ";
            appendNum(&c, g);
            c += " ";
            appendNum(&c, b);
            c += " RG 1.5 w\n";
            bool first = true;
            for (size_t i = 0; i < nCat; ++i) {
                const double v = i < chart.series[s].values.size() ? chart.series[s].values[i] : 0.0;
                const double x = L + (nCat == 1 ? plotW * 0.5 : (i * plotW / static_cast<double>(nCat - 1)));
                const double y = B + (v - vmin) / span * plotH;
                appendNum(&c, x);
                c += " ";
                appendNum(&c, y);
                c += first ? " m " : " l ";
                first = false;
            }
            c += "S\n";
            appendNum(&c, r);
            c += " ";
            appendNum(&c, g);
            c += " ";
            appendNum(&c, b);
            c += " rg\n";
            for (size_t i = 0; i < nCat; ++i) {
                const double v = i < chart.series[s].values.size() ? chart.series[s].values[i] : 0.0;
                const double x = L + (nCat == 1 ? plotW * 0.5 : (i * plotW / static_cast<double>(nCat - 1)));
                const double y = B + (v - vmin) / span * plotH;
                appendNum(&c, x - 2.0);
                c += " ";
                appendNum(&c, y - 2.0);
                c += " 4 4 re f\n";
            }
        }
    }

    // X 标签
    for (size_t i = 0; i < nCat; ++i) {
        const std::string label = i < chart.categories.size() ? chart.categories[i] : std::string();
        const double x = (chart.type == ReportChartType::Bar)
            ? (L + (i + 0.5) * (plotW / static_cast<double>(nCat)))
            : (L + (nCat == 1 ? plotW * 0.5 : (i * plotW / static_cast<double>(nCat - 1))));
        c += "BT /F1 8 Tf ";
        appendNum(&c, x - textWidth(label, 8.0) * 0.5);
        c += " ";
        appendNum(&c, B - 12.0);
        c += " Td ";
        c += hexCidString(label);
        c += " Tj ET\n";
    }

    // 图例
    double lx = L;
    const double ly = B - 26.0;
    for (size_t s = 0; s < nSeries; ++s) {
        double r, g, b;
        seriesRgb(s, &r, &g, &b);
        appendNum(&c, r);
        c += " ";
        appendNum(&c, g);
        c += " ";
        appendNum(&c, b);
        c += " rg ";
        appendNum(&c, lx);
        c += " ";
        appendNum(&c, ly);
        c += " 8 8 re f\n";
        c += "BT /F1 9 Tf ";
        appendNum(&c, lx + 12.0);
        c += " ";
        appendNum(&c, ly + 1.0);
        c += " Td ";
        c += hexCidString(chart.series[s].name);
        c += " Tj ET\n";
        lx += 20.0 + textWidth(chart.series[s].name, 9.0);
        if (lx > R - 40.0) {
            break;
        }
    }

    m_y = B - 36.0;
}

/************************************************************
 * 排版主流程
 ************************************************************/

bool PdfReportWriter::layout(std::string* /*error*/)
{
    // 首页与文档标题
    m_pages.clear();
    newPage();
    if (!m_doc.title().empty()) {
        drawTextLine(m_doc.title(), 18.0, true);
        m_y -= 6.0;
    }

    // 按块类型排版：标题折行、段落、表格、图表、分隔线
    const std::vector<ReportBlock>& blocks = m_doc.blocks();
    for (size_t i = 0; i < blocks.size(); ++i) {
        const ReportBlock& b = blocks[i];
        switch (b.kind) {
        case ReportBlockKind::Heading: {
            const double sizes[] = { 0, 16.0, 13.0, 11.0 };
            m_y -= 6.0;
            std::vector<std::string> lines;
            wrapText(b.text, sizes[b.headingLevel], m_contentW, &lines);
            for (size_t L = 0; L < lines.size(); ++L) {
                drawTextLine(lines[L], sizes[b.headingLevel], true);
            }
            break;
        }
        case ReportBlockKind::Paragraph: {
            std::vector<std::string> lines;
            wrapText(b.text, 10.5, m_contentW, &lines);
            for (size_t L = 0; L < lines.size(); ++L) {
                drawTextLine(lines[L], 10.5, false);
            }
            m_y -= 4.0;
            break;
        }
        case ReportBlockKind::Table:
            drawTable(b.table);
            break;
        case ReportBlockKind::Chart:
            drawChart(b.chart);
            break;
        case ReportBlockKind::HRule: {
            ensureSpace(12.0);
            std::string& c = m_pages.back().content;
            c += "0.9 0.91 0.92 RG 1 w ";
            appendNum(&c, m_margin);
            c += " ";
            appendNum(&c, m_y - 4.0);
            c += " m ";
            appendNum(&c, m_margin + m_contentW);
            c += " ";
            appendNum(&c, m_y - 4.0);
            c += " l S\n";
            m_y -= 12.0;
            break;
        }
        }
    }
    if (m_pages.empty()) {
        newPage();
    }
    return true;
}

/************************************************************
 * PDF 文件组装
 ************************************************************/

bool PdfReportWriter::buildFile(std::vector<unsigned char>* out, std::string* error) const
{
    if (!out) {
        if (error) {
            *error = "null out";
        }
        return false;
    }
    // 对象编号规划：
    // 1 Catalog, 2 Pages, 3 Font(Type0), 4 CIDFont, 5 FontDescriptor, 6 FontFile2,
    // 7 ToUnicode, 然后每页 Page + Content
    const int objCatalog = 1;
    const int objPages = 2;
    const int objFont = 3;
    const int objCid = 4;
    const int objDesc = 5;
    const int objFontFile = 6;
    const int objToUnicode = 7;
    const int firstPageObj = 8;
    const int pageCount = static_cast<int>(m_pages.size());

    std::vector<std::string> objs(1); // 1-based，占位

    auto addObj = [&](const std::string& body) -> int {
        objs.push_back(body);
        return static_cast<int>(objs.size() - 1);
    };
    (void)addObj;

    // 预先填满到 ToUnicode
    objs.resize(objToUnicode + 1);

    // Catalog
    {
        std::ostringstream os;
        os << "<< /Type /Catalog /Pages " << objPages << " 0 R >>";
        objs[objCatalog] = os.str();
    }
    // Pages kids
    {
        std::ostringstream os;
        os << "<< /Type /Pages /Count " << pageCount << " /Kids [";
        for (int i = 0; i < pageCount; ++i) {
            const int pageObj = firstPageObj + i * 2;
            os << " " << pageObj << " 0 R";
        }
        os << " ] >>";
        objs[objPages] = os.str();
    }

    // Widths array for CIDFont
    uint16_t maxGid = 0;
    for (std::map<uint16_t, uint16_t>::const_iterator it = m_advanceOf.begin(); it != m_advanceOf.end(); ++it) {
        if (it->first > maxGid) {
            maxGid = it->first;
        }
    }
    std::ostringstream wos;
    wos << "[ 0 [";
    for (uint16_t g = 0; g <= maxGid; ++g) {
        uint16_t adv = 0;
        std::map<uint16_t, uint16_t>::const_iterator a = m_advanceOf.find(g);
        if (a != m_advanceOf.end()) {
            adv = a->second;
        }
        // PDF 宽度单位：1000 = 1em，需换算
        const int w = static_cast<int>(std::lround(adv * 1000.0 / m_unitsPerEm));
        wos << " " << w;
    }
    wos << " ] ]";

    objs[objFont] = "<< /Type /Font /Subtype /Type0 /BaseFont /AAAAAA+YaHei /Encoding /Identity-H "
                    "/DescendantFonts [ " + std::to_string(objCid) + " 0 R ] /ToUnicode "
                    + std::to_string(objToUnicode) + " 0 R >>";
    objs[objCid] = "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /AAAAAA+YaHei "
                   "/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> "
                   "/FontDescriptor " + std::to_string(objDesc) + " 0 R /W " + wos.str()
        + " /CIDToGIDMap /Identity >>";
    objs[objDesc] = "<< /Type /FontDescriptor /FontName /AAAAAA+YaHei /Flags 32 "
                    "/FontBBox [ -100 -200 1000 900 ] /ItalicAngle 0 /Ascent 800 /Descent -200 "
                    "/CapHeight 700 /StemV 80 /FontFile2 "
        + std::to_string(objFontFile) + " 0 R >>";

    {
        std::ostringstream os;
        os << "<< /Length " << m_subsetTtf.size() << " /Length1 " << m_subsetTtf.size() << " >>\nstream\n";
        std::string head = os.str();
        std::string body;
        body.reserve(head.size() + m_subsetTtf.size() + 16);
        body += head;
        body.append(reinterpret_cast<const char*>(m_subsetTtf.data()), m_subsetTtf.size());
        body += "\nendstream";
        objs[objFontFile] = body;
    }

    // ToUnicode CMap
    {
        std::ostringstream cmap;
        cmap << "/CIDInit /ProcSet findresource begin\n"
                "12 dict begin\nbegincmap\n"
                "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
                "/CMapName /Adobe-Identity-UCS def\n/CMapType 2 def\n"
                "1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n";
        std::vector<std::pair<uint16_t, uint32_t> > pairs;
        for (std::map<uint32_t, uint16_t>::const_iterator it = m_gidOf.begin(); it != m_gidOf.end(); ++it) {
            pairs.push_back(std::make_pair(it->second, it->first));
        }
        std::sort(pairs.begin(), pairs.end());
        // 去重：同一 gid 保留一个
        std::vector<std::pair<uint16_t, uint32_t> > uniq;
        for (size_t i = 0; i < pairs.size(); ++i) {
            if (!uniq.empty() && uniq.back().first == pairs[i].first) {
                continue;
            }
            uniq.push_back(pairs[i]);
        }
        const size_t n = uniq.size();
        size_t idx = 0;
        while (idx < n) {
            const size_t chunk = (std::min)(size_t(100), n - idx);
            cmap << chunk << " beginbfchar\n";
            for (size_t k = 0; k < chunk; ++k) {
                char a[16];
                char b[16];
                std::snprintf(a, sizeof(a), "%04X", uniq[idx + k].first);
                const uint32_t cp = uniq[idx + k].second;
                if (cp <= 0xFFFF) {
                    std::snprintf(b, sizeof(b), "%04X", cp);
                } else {
                    // UTF-16 surrogate
                    const uint32_t u = cp - 0x10000;
                    std::snprintf(b, sizeof(b), "%04X%04X", 0xD800 + (u >> 10), 0xDC00 + (u & 0x3FF));
                }
                cmap << "<" << a << "> <" << b << ">\n";
            }
            cmap << "endbfchar\n";
            idx += chunk;
        }
        cmap << "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n";
        const std::string cmapStr = cmap.str();
        objs[objToUnicode] = "<< /Length " + std::to_string(cmapStr.size()) + " >>\nstream\n" + cmapStr + "\nendstream";
    }

    // Pages + contents
    objs.resize(firstPageObj + pageCount * 2);
    for (int i = 0; i < pageCount; ++i) {
        const int pageObj = firstPageObj + i * 2;
        const int contentObj = pageObj + 1;
        {
            std::ostringstream os;
            os << "<< /Type /Page /Parent " << objPages << " 0 R /MediaBox [ 0 0 "
               << static_cast<int>(m_pageW) << " " << static_cast<int>(m_pageH)
               << " ] /Resources << /Font << /F1 " << objFont << " 0 R >> >> /Contents "
               << contentObj << " 0 R >>";
            objs[pageObj] = os.str();
        }
        {
            const std::string& stream = m_pages[i].content;
            objs[contentObj] = "<< /Length " + std::to_string(stream.size()) + " >>\nstream\n" + stream + "\nendstream";
        }
    }

    // 写交叉引用
    std::string file = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::vector<size_t> offsets(objs.size(), 0);
    for (size_t i = 1; i < objs.size(); ++i) {
        offsets[i] = file.size();
        file += std::to_string(i);
        file += " 0 obj\n";
        file += objs[i];
        file += "\nendobj\n";
    }
    const size_t xrefPos = file.size();
    file += "xref\n0 ";
    file += std::to_string(objs.size());
    file += "\n";
    file += "0000000000 65535 f \n";
    for (size_t i = 1; i < objs.size(); ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%010u 00000 n \n", static_cast<unsigned>(offsets[i]));
        file += buf;
    }
    file += "trailer\n<< /Size ";
    file += std::to_string(objs.size());
    file += " /Root ";
    file += std::to_string(objCatalog);
    file += " 0 R >>\nstartxref\n";
    file += std::to_string(xrefPos);
    file += "\n%%EOF\n";

    out->assign(file.begin(), file.end());
    return true;
}

}
