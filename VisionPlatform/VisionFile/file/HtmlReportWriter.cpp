/**
 * @file HtmlReportWriter.cpp
 * @brief HtmlReportWriter 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HtmlReportWriter.h"
#include "FileDocument.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace Vision {

/************************************************************
 * 构造与入口
 ************************************************************/

HtmlReportWriter::HtmlReportWriter(const ReportDocument& doc)
    : m_doc(doc)
{
}

bool HtmlReportWriter::save(const std::string& path, std::string* error)
{
    return TxtDocument::writeText(path, buildHtml(), true, error);
}

/************************************************************
 * HTML 组装
 ************************************************************/

std::string HtmlReportWriter::escape(const std::string& s)
{
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '&') {
            o += "&amp;";
        } else if (c == '<') {
            o += "&lt;";
        } else if (c == '>') {
            o += "&gt;";
        } else if (c == '"') {
            o += "&quot;";
        } else {
            o.push_back(c);
        }
    }
    return o;
}

const char* HtmlReportWriter::seriesColor(size_t index)
{
    static const char* kColors[] = {
        "#2563eb", "#dc2626", "#16a34a", "#ca8a04", "#7c3aed", "#0891b2", "#ea580c", "#4b5563"
    };
    return kColors[index % (sizeof(kColors) / sizeof(kColors[0]))];
}

std::string HtmlReportWriter::tableHtml(const ReportTable& t) const
{
    std::ostringstream os;
    os << "<table>\n";
    if (!t.headers.empty()) {
        os << "<thead><tr>";
        for (size_t i = 0; i < t.headers.size(); ++i) {
            os << "<th>" << escape(t.headers[i]) << "</th>";
        }
        os << "</tr></thead>\n";
    }
    os << "<tbody>\n";
    for (size_t r = 0; r < t.rows.size(); ++r) {
        os << "<tr>";
        const std::vector<std::string>& row = t.rows[r];
        const size_t cols = t.headers.empty() ? row.size() : t.headers.size();
        for (size_t c = 0; c < cols; ++c) {
            os << "<td>" << escape(c < row.size() ? row[c] : std::string()) << "</td>";
        }
        os << "</tr>\n";
    }
    os << "</tbody></table>\n";
    return os.str();
}

std::string HtmlReportWriter::chartSvg(const ReportChart& c) const
{
    const int W = 720;
    const int H = 320;
    const int L = 56;
    const int R = 24;
    const int T = c.title.empty() ? 24 : 40;
    const int B = 48;
    const int plotW = W - L - R;
    const int plotH = H - T - B;

    size_t nCat = c.categories.size();
    for (size_t s = 0; s < c.series.size(); ++s) {
        nCat = (std::max)(nCat, c.series[s].values.size());
    }
    if (nCat == 0) {
        nCat = 1;
    }

    double vmin = 0.0;
    double vmax = 0.0;
    bool any = false;
    for (size_t s = 0; s < c.series.size(); ++s) {
        for (size_t i = 0; i < c.series[s].values.size(); ++i) {
            const double v = c.series[s].values[i];
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

    std::ostringstream os;
    os << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << W << "\" height=\"" << H
       << "\" viewBox=\"0 0 " << W << " " << H << "\" class=\"chart\">";
    os << "<rect x=\"0\" y=\"0\" width=\"" << W << "\" height=\"" << H << "\" fill=\"#fff\" stroke=\"#e5e7eb\"/>";
    if (!c.title.empty()) {
        os << "<text x=\"" << (W / 2) << "\" y=\"22\" text-anchor=\"middle\" font-size=\"14\" font-weight=\"600\" fill=\"#111\">"
           << escape(c.title) << "</text>";
    }
    if (!c.yLabel.empty()) {
        os << "<text x=\"14\" y=\"" << (T + plotH / 2)
           << "\" text-anchor=\"middle\" font-size=\"11\" fill=\"#6b7280\" transform=\"rotate(-90 14 "
           << (T + plotH / 2) << ")\">" << escape(c.yLabel) << "</text>";
    }

    // 网格与 Y 刻度
    for (int g = 0; g <= 4; ++g) {
        const double t = g / 4.0;
        const int y = T + static_cast<int>(std::lround((1.0 - t) * plotH));
        const double val = vmin + span * t;
        os << "<line x1=\"" << L << "\" y1=\"" << y << "\" x2=\"" << (L + plotW) << "\" y2=\"" << y
           << "\" stroke=\"#f3f4f6\"/>";
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.4g", val);
        os << "<text x=\"" << (L - 6) << "\" y=\"" << (y + 4) << "\" text-anchor=\"end\" font-size=\"10\" fill=\"#6b7280\">"
           << buf << "</text>";
    }
    os << "<line x1=\"" << L << "\" y1=\"" << T << "\" x2=\"" << L << "\" y2=\"" << (T + plotH)
       << "\" stroke=\"#9ca3af\"/>";
    os << "<line x1=\"" << L << "\" y1=\"" << (T + plotH) << "\" x2=\"" << (L + plotW) << "\" y2=\"" << (T + plotH)
       << "\" stroke=\"#9ca3af\"/>";

    const size_t nSeries = c.series.size();
    if (c.type == ReportChartType::Bar) {
        const double groupW = plotW / static_cast<double>(nCat);
        const double barW = groupW / static_cast<double>((std::max)(size_t(1), nSeries) + 1.0);
        for (size_t s = 0; s < nSeries; ++s) {
            const ReportChartSeries& ser = c.series[s];
            for (size_t i = 0; i < nCat; ++i) {
                const double v = i < ser.values.size() ? ser.values[i] : 0.0;
                const double h = (v - vmin) / span * plotH;
                const double x = L + i * groupW + (s + 0.5) * barW;
                const double y = T + plotH - h;
                os << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << (barW * 0.9) << "\" height=\"" << h
                   << "\" fill=\"" << seriesColor(s) << "\"/>";
            }
        }
    } else {
        for (size_t s = 0; s < nSeries; ++s) {
            const ReportChartSeries& ser = c.series[s];
            os << "<polyline fill=\"none\" stroke=\"" << seriesColor(s) << "\" stroke-width=\"2\" points=\"";
            for (size_t i = 0; i < nCat; ++i) {
                const double v = i < ser.values.size() ? ser.values[i] : 0.0;
                const double x = L + (nCat == 1 ? plotW * 0.5 : (i * plotW / static_cast<double>(nCat - 1)));
                const double y = T + plotH - (v - vmin) / span * plotH;
                if (i) {
                    os << " ";
                }
                os << x << "," << y;
            }
            os << "\"/>";
            for (size_t i = 0; i < nCat; ++i) {
                const double v = i < ser.values.size() ? ser.values[i] : 0.0;
                const double x = L + (nCat == 1 ? plotW * 0.5 : (i * plotW / static_cast<double>(nCat - 1)));
                const double y = T + plotH - (v - vmin) / span * plotH;
                os << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"3\" fill=\"" << seriesColor(s) << "\"/>";
            }
        }
    }

    // X 类别
    for (size_t i = 0; i < nCat; ++i) {
        const double x = (c.type == ReportChartType::Bar)
            ? (L + (i + 0.5) * (plotW / static_cast<double>(nCat)))
            : (L + (nCat == 1 ? plotW * 0.5 : (i * plotW / static_cast<double>(nCat - 1))));
        const std::string label = i < c.categories.size() ? c.categories[i] : std::string();
        os << "<text x=\"" << x << "\" y=\"" << (T + plotH + 18) << "\" text-anchor=\"middle\" font-size=\"10\" fill=\"#374151\">"
           << escape(label) << "</text>";
    }

    // 图例
    int lx = L;
    const int ly = H - 14;
    for (size_t s = 0; s < nSeries; ++s) {
        os << "<rect x=\"" << lx << "\" y=\"" << (ly - 8) << "\" width=\"10\" height=\"10\" fill=\"" << seriesColor(s) << "\"/>";
        os << "<text x=\"" << (lx + 14) << "\" y=\"" << ly << "\" font-size=\"11\" fill=\"#374151\">"
           << escape(c.series[s].name) << "</text>";
        lx += 24 + static_cast<int>(c.series[s].name.size() * 7);
        if (lx > W - 80) {
            break;
        }
    }
    os << "</svg>\n";
    return os.str();
}

std::string HtmlReportWriter::blockHtml(const ReportBlock& b) const
{
    switch (b.kind) {
    case ReportBlockKind::Heading: {
        char tag[4] = { 'h', '0', 0, 0 };
        tag[1] = static_cast<char>('0' + b.headingLevel);
        return std::string("<") + tag + ">" + escape(b.text) + "</" + tag + ">\n";
    }
    case ReportBlockKind::Paragraph:
        return "<p>" + escape(b.text) + "</p>\n";
    case ReportBlockKind::Table:
        return tableHtml(b.table);
    case ReportBlockKind::Chart:
        return "<div class=\"chart-wrap\">" + chartSvg(b.chart) + "</div>\n";
    case ReportBlockKind::HRule:
        return "<hr/>\n";
    }
    return std::string();
}

std::string HtmlReportWriter::buildHtml() const
{
    std::ostringstream os;
    os << "<!DOCTYPE html>\n<html lang=\"zh-CN\"><head><meta charset=\"utf-8\"/>"
       << "<title>" << escape(m_doc.title()) << "</title>\n"
       << "<style>\n"
       << "body{font-family:\"Microsoft YaHei\",\"Segoe UI\",sans-serif;margin:24px;color:#111;line-height:1.55;}"
       << "h1{font-size:22px;margin:0 0 16px;}h2{font-size:18px;margin:20px 0 10px;}h3{font-size:15px;margin:16px 0 8px;}"
       << "p{margin:0 0 10px;}hr{border:none;border-top:1px solid #e5e7eb;margin:18px 0;}"
       << "table{border-collapse:collapse;width:100%;margin:8px 0 16px;font-size:13px;}"
       << "th,td{border:1px solid #d1d5db;padding:6px 10px;text-align:left;}"
       << "th{background:#f3f4f6;font-weight:600;}"
       << ".chart-wrap{margin:12px 0 20px;overflow-x:auto;}"
       << "svg.chart{max-width:100%;height:auto;}"
       << "</style></head><body>\n";
    if (!m_doc.title().empty()) {
        os << "<h1>" << escape(m_doc.title()) << "</h1>\n";
    }
    const std::vector<ReportBlock>& blocks = m_doc.blocks();
    for (size_t i = 0; i < blocks.size(); ++i) {
        os << blockHtml(blocks[i]);
    }
    os << "</body></html>\n";
    return os.str();
}

}
