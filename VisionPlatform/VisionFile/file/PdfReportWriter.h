/**
 * @file PdfReportWriter.h
 * @brief 将 ReportDocument 写成嵌入中文字体子集的 PDF
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PDFREPORTWRITER_H
#define VISION_PDFREPORTWRITER_H

#include "ReportDocument.h"
#include "TtfSubset.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class PdfReportWriter
 * @brief ReportDocument → PDF 1.4 写出器
 */
class PdfReportWriter {
public:
    /** @brief 绑定要写出的文档 */
    explicit PdfReportWriter(const ReportDocument& doc);
    /** @brief 写出到 path；失败时填写 error */
    bool save(const std::string& path, std::string* error);

private:
    /** @brief 一页内容流与用到的资源 */
    struct Page {
        std::string content; ///< 内容流
    };

    /** @brief UTF-8 转 UTF-32 码位 */
    static void utf8ToCodepoints(const std::string& utf8, std::vector<uint32_t>* out);
    /** @brief 收集文档全部码位 */
    void collectCodepoints(std::vector<uint32_t>* out) const;
    /** @brief 准备字体子集 */
    bool prepareFont(std::string* error);
    /** @brief 排版并生成各页内容流 */
    bool layout(std::string* error);
    /** @brief 组装 PDF 文件字节 */
    bool buildFile(std::vector<unsigned char>* out, std::string* error) const;
    /** @brief 字符串转 PDF 十六进制 CID 串 */
    std::string hexCidString(const std::string& utf8) const;
    /** @brief 一段文本的排版宽度（点） */
    double textWidth(const std::string& utf8, double fontSize) const;
    /** @brief 按宽度折行 */
    void wrapText(const std::string& utf8, double fontSize, double maxWidth,
                  std::vector<std::string>* lines) const;
    /** @brief 向当前页写一段文本；必要时翻页 */
    void ensureSpace(double need);
    /** @brief 新开一页 */
    void newPage();
    /** @brief 在当前 y 画文本行并下移 */
    void drawTextLine(const std::string& utf8, double fontSize, bool bold);
    /** @brief 画表格 */
    void drawTable(const ReportTable& table);
    /** @brief 画图表 */
    void drawChart(const ReportChart& chart);
    /** @brief 序列颜色 RGB 0..1 */
    static void seriesRgb(size_t index, double* r, double* g, double* b);
    /** @brief 追加格式化浮点到内容流 */
    static void appendNum(std::string* s, double v);

    const ReportDocument& m_doc;                 ///< 源文档
    TtfSubset m_font;                            ///< 字体解析器
    std::vector<unsigned char> m_subsetTtf;      ///< 子集 TTF
    std::map<uint32_t, uint16_t> m_gidOf;        ///< 码位 → CID
    std::map<uint16_t, uint16_t> m_advanceOf;    ///< CID → 前进宽度
    uint16_t m_unitsPerEm = 1000;                ///< unitsPerEm
    std::vector<Page> m_pages;                   ///< 页面
    double m_pageW = 595.0;                      ///< A4 宽
    double m_pageH = 842.0;                      ///< A4 高
    double m_margin = 48.0;                      ///< 页边距
    double m_y = 0.0;                            ///< 当前基线 y（自上而下排，存 PDF 坐标）
    double m_contentW = 0.0;                     ///< 内容区宽度
};

}

#endif
