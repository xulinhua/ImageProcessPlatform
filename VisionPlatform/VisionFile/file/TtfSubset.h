/**
 * @file TtfSubset.h
 * @brief TrueType / TTC 解析与按码位子集化，供 PDF 嵌入
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TTFSUBSET_H
#define VISION_TTFSUBSET_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class TtfSubset
 * @brief 从系统字体字节中取出用到的字形，生成可嵌入的 TTF 子集
 */
class TtfSubset {
public:
    /**
     * @brief 从 Win32 GetFontData 得到的字节加载；支持 TTF 与 TTC（取第一张脸）
     * @return 成功返回 true
     */
    bool load(const std::vector<unsigned char>& bytes, std::string* error);
    /**
     * @brief 按码位生成子集；始终包含 .notdef
     * @param codepoints UTF-32 码位列表（可重复）
     * @param outTtf 输出子集 TTF
     * @param gidOf 码位 → 子集内 glyph id（Identity-H 的 CID）
     * @param advanceOf 子集 glyph id → 前进宽度（font units）
     * @param unitsPerEm 输出 unitsPerEm
     */
    bool build(const std::vector<uint32_t>& codepoints,
               std::vector<unsigned char>* outTtf,
               std::map<uint32_t, uint16_t>* gidOf,
               std::map<uint16_t, uint16_t>* advanceOf,
               uint16_t* unitsPerEm,
               std::string* error) const;

    /** @brief 从系统取“微软雅黑/黑体/宋体”之一的字体文件字节 */
    static bool loadSystemCjkFont(std::vector<unsigned char>* bytes, std::string* error);
    /** @brief 读整个字体文件到内存 */
    static bool readFontFile(const wchar_t* path, std::vector<unsigned char>* bytes);

private:
    /** @brief SFNT 表目录项 */
    struct Table {
        uint32_t tag = 0;    ///< 四字符标签
        uint32_t offset = 0; ///< 相对字体起始的偏移
        uint32_t length = 0; ///< 长度
    };

    /** @brief 大端读 16 位 */
    static uint16_t rd16(const unsigned char* p);
    /** @brief 大端读 32 位 */
    static uint32_t rd32(const unsigned char* p);
    /** @brief 大端写 16 位 */
    static void wr16(std::vector<unsigned char>* o, uint16_t v);
    /** @brief 大端写 32 位 */
    static void wr32(std::vector<unsigned char>* o, uint32_t v);
    /** @brief 对齐到 4 字节 */
    static void pad4(std::vector<unsigned char>* o);
    /** @brief 表校验和 */
    static uint32_t checksum(const unsigned char* data, uint32_t length);
    /** @brief 在表目录中查找 */
    const Table* findTable(uint32_t tag) const;
    /** @brief 表内容切片 */
    bool tableBytes(uint32_t tag, std::vector<unsigned char>* out, std::string* error) const;
    /** @brief 加载 SFNT 单脸（偏移相对 bytes 文件头） */
    bool loadFaceAt(const std::vector<unsigned char>& bytes, uint32_t faceOff, std::string* error);
    /** @brief cmap 查码位对应的原始 glyph id；找不到返回 0 */
    uint16_t glyphForCode(uint32_t cp) const;
    /** @brief 收集复合字形依赖到 used 集合 */
    void collectComposite(uint16_t gid, std::map<uint16_t, bool>* used) const;
    /** @brief 取原始字形的 glyf 字节 */
    bool glyphData(uint16_t gid, std::vector<unsigned char>* out, std::string* error) const;
    /** @brief 原始前进宽度 */
    uint16_t advance(uint16_t gid) const;
    /** @brief 重写复合字形中的 component glyph id */
    static bool remapComposite(std::vector<unsigned char>* glyf,
                               const std::map<uint16_t, uint16_t>& oldToNew,
                               std::string* error);

    std::vector<unsigned char> m_bytes; ///< 单张 SFNT 字体字节
    std::vector<Table> m_tables;        ///< 表目录
    uint16_t m_numGlyphs = 0;           ///< maxp 字形数
    uint16_t m_unitsPerEm = 1000;       ///< head.unitsPerEm
    uint16_t m_indexToLocFormat = 0;    ///< 0=short loca，1=long
    uint16_t m_numberOfHMetrics = 0;    ///< hhea.numberOfHMetrics
};

}

#endif
