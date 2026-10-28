/**
 * @file XlsxDocument.cpp
 * @brief XlsxDocument实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FileDocument.h"
#include "TextEncoding.h"
#include <cstdio>
#include <stdint.h>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

// ZIP 本地头用的 CRC32
uint32_t crc32Of(const void* data, size_t n)
{
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        init = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < n; ++i) {
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// 按小端写入 16 位
void put16(std::string* o, uint16_t v)
{
    o->push_back(static_cast<char>(v & 0xFF));
    o->push_back(static_cast<char>((v >> 8) & 0xFF));
}

// 按小端写入 32 位
void put32(std::string* o, uint32_t v)
{
    o->push_back(static_cast<char>(v & 0xFF));
    o->push_back(static_cast<char>((v >> 8) & 0xFF));
    o->push_back(static_cast<char>((v >> 16) & 0xFF));
    o->push_back(static_cast<char>((v >> 24) & 0xFF));
}

struct ZipEntry {
    std::string name; ///< 包内路径
    std::string data; ///< 文件内容
    uint32_t crc;     ///< CRC32
    uint32_t offset;  ///< 本地头偏移
};

// 转义工作表 XML
std::string xmlEscape(const std::string& s)
{
    std::string o;
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

// 无压缩打成 ZIP
std::string buildZip(const std::vector<ZipEntry>& in)
{
    std::vector<ZipEntry> files = in;
    std::string out;
    for (size_t i = 0; i < files.size(); ++i) {
        files[i].crc = crc32Of(files[i].data.data(), files[i].data.size());
        files[i].offset = static_cast<uint32_t>(out.size());
        const uint32_t sz = static_cast<uint32_t>(files[i].data.size());
        out.append("PK\x03\x04", 4);
        put16(&out, 20);
        put16(&out, 0);
        put16(&out, 0);
        put16(&out, 0);
        put16(&out, 0);
        put32(&out, files[i].crc);
        put32(&out, sz);
        put32(&out, sz);
        put16(&out, static_cast<uint16_t>(files[i].name.size()));
        put16(&out, 0);
        out += files[i].name;
        out += files[i].data;
    }
    const uint32_t cdStart = static_cast<uint32_t>(out.size());
    for (size_t i = 0; i < files.size(); ++i) {
        const uint32_t sz = static_cast<uint32_t>(files[i].data.size());
        out.append("PK\x01\x02", 4);
        put16(&out, 20);
        put16(&out, 20);
        put16(&out, 0);
        put16(&out, 0);
        put16(&out, 0);
        put16(&out, 0);
        put32(&out, files[i].crc);
        put32(&out, sz);
        put32(&out, sz);
        put16(&out, static_cast<uint16_t>(files[i].name.size()));
        put16(&out, 0);
        put16(&out, 0);
        put16(&out, 0);
        put16(&out, 0);
        put32(&out, 0);
        put32(&out, files[i].offset);
        out += files[i].name;
    }
    const uint32_t cdSize = static_cast<uint32_t>(out.size()) - cdStart;
    out.append("PK\x05\x06", 4);
    put16(&out, 0);
    put16(&out, 0);
    put16(&out, static_cast<uint16_t>(files.size()));
    put16(&out, static_cast<uint16_t>(files.size()));
    put32(&out, cdSize);
    put32(&out, cdStart);
    put16(&out, 0);
    return out;
}

// 列号转成 Excel 列名
std::string colName(int c)
{
    std::string s;
    int n = c + 1;
    while (n > 0) {
        --n;
        s.insert(s.begin(), static_cast<char>('A' + (n % 26)));
        n /= 26;
    }
    return s;
}

} // namespace

// 记下导出列名
void XlsxDocument::setHeaders(const std::vector<std::string>& headers)
{
    m_headers = headers;
}

// 缓存一行单元格
void XlsxDocument::addRow(const std::vector<std::string>& cells)
{
    m_rows.push_back(cells);
}

// 打成 xlsx 并原子写出
bool XlsxDocument::save(const std::string& path, std::string* error)
{
    std::string sheet;
    sheet += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
    sheet += "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>";
    int rowIndex = 1;
    if (!m_headers.empty()) {
        sheet += "<row r=\"";
        char nbuf[16];
        sprintf(nbuf, "%d", rowIndex);
        sheet += nbuf;
        sheet += "\">";
        for (size_t i = 0; i < m_headers.size(); ++i) {
            sheet += "<c r=\"";
            sheet += colName(static_cast<int>(i));
            sheet += nbuf;
            sheet += "\" t=\"inlineStr\"><is><t>";
            sheet += xmlEscape(m_headers[i]);
            sheet += "</t></is></c>";
        }
        sheet += "</row>";
        ++rowIndex;
    }
    for (size_t r = 0; r < m_rows.size(); ++r) {
        char nbuf[16];
        sprintf(nbuf, "%d", rowIndex);
        sheet += "<row r=\"";
        sheet += nbuf;
        sheet += "\">";
        const std::vector<std::string>& row = m_rows[r];
        for (size_t i = 0; i < row.size(); ++i) {
            sheet += "<c r=\"";
            sheet += colName(static_cast<int>(i));
            sheet += nbuf;
            sheet += "\" t=\"inlineStr\"><is><t>";
            sheet += xmlEscape(row[i]);
            sheet += "</t></is></c>";
        }
        sheet += "</row>";
        ++rowIndex;
    }
    sheet += "</sheetData></worksheet>";

    std::vector<ZipEntry> z;
    ZipEntry e;
    e.name = "[Content_Types].xml";
    e.data = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
             "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
             "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
             "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
             "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
             "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
             "</Types>";
    z.push_back(e);
    e.name = "_rels/.rels";
    e.data = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
             "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
             "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
             "</Relationships>";
    z.push_back(e);
    e.name = "xl/_rels/workbook.xml.rels";
    e.data = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
             "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
             "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
             "</Relationships>";
    z.push_back(e);
    e.name = "xl/workbook.xml";
    e.data = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
             "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
             "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
             "<sheets><sheet name=\"Sheet1\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>";
    z.push_back(e);
    e.name = "xl/worksheets/sheet1.xml";
    e.data = sheet;
    z.push_back(e);
    return FileUtil::writeAtomic(path, buildZip(z), error);
}

}
