/**
 * @file TtfSubset.cpp
 * @brief TtfSubset 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TtfSubset.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>

namespace Vision {

/************************************************************
 * 字节读写
 ************************************************************/

uint16_t TtfSubset::rd16(const unsigned char* p)
{
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

uint32_t TtfSubset::rd32(const unsigned char* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
        | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

void TtfSubset::wr16(std::vector<unsigned char>* o, uint16_t v)
{
    o->push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
    o->push_back(static_cast<unsigned char>(v & 0xFF));
}

void TtfSubset::wr32(std::vector<unsigned char>* o, uint32_t v)
{
    o->push_back(static_cast<unsigned char>((v >> 24) & 0xFF));
    o->push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
    o->push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
    o->push_back(static_cast<unsigned char>(v & 0xFF));
}

void TtfSubset::pad4(std::vector<unsigned char>* o)
{
    while (o->size() & 3u) {
        o->push_back(0);
    }
}

uint32_t TtfSubset::checksum(const unsigned char* data, uint32_t length)
{
    uint32_t sum = 0;
    uint32_t i = 0;
    for (; i + 4 <= length; i += 4) {
        sum += rd32(data + i);
    }
    if (i < length) {
        unsigned char tail[4] = { 0, 0, 0, 0 };
        for (uint32_t k = 0; i + k < length; ++k) {
            tail[k] = data[i + k];
        }
        sum += rd32(tail);
    }
    return sum;
}

/************************************************************
 * 系统字体
 ************************************************************/

bool TtfSubset::readFontFile(const wchar_t* path, std::vector<unsigned char>* bytes)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    const DWORD sz = GetFileSize(h, NULL);
    if (sz == INVALID_FILE_SIZE || sz == 0) {
        CloseHandle(h);
        return false;
    }
    bytes->assign(sz, 0);
    DWORD read = 0;
    const BOOL ok = ReadFile(h, bytes->data(), sz, &read, NULL);
    CloseHandle(h);
    return ok && read == sz;
}

bool TtfSubset::loadSystemCjkFont(std::vector<unsigned char>* bytes, std::string* error)
{
    if (!bytes) {
        if (error) {
            *error = "null bytes";
        }
        return false;
    }
    // 优先直接读 Windows Fonts 目录里的 TrueType / TTC
    static const wchar_t* kFiles[] = {
        L"C:\\Windows\\Fonts\\msyh.ttc",
        L"C:\\Windows\\Fonts\\msyh.ttf",
        L"C:\\Windows\\Fonts\\msyhbd.ttc",
        L"C:\\Windows\\Fonts\\simhei.ttf",
        L"C:\\Windows\\Fonts\\simsun.ttc",
        L"C:\\Windows\\Fonts\\simsun.ttf",
        L"C:\\Windows\\Fonts\\NotoSansCJK-Regular.ttc",
        L"C:\\Windows\\Fonts\\SourceHanSansSC-Regular.ttc"
    };
    for (size_t i = 0; i < sizeof(kFiles) / sizeof(kFiles[0]); ++i) {
        if (readFontFile(kFiles[i], bytes)) {
            return true;
        }
    }

    // 回退：GDI GetFontData
    static const wchar_t* kFaces[] = {
        L"Microsoft YaHei", L"微软雅黑", L"SimHei", L"黑体", L"SimSun", L"宋体", L"NSimSun"
    };
    HDC hdc = CreateCompatibleDC(NULL);
    if (!hdc) {
        if (error) {
            *error = "CreateCompatibleDC failed";
        }
        return false;
    }
    bool ok = false;
    for (size_t i = 0; i < sizeof(kFaces) / sizeof(kFaces[0]); ++i) {
        HFONT font = CreateFontW(-32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, kFaces[i]);
        if (!font) {
            continue;
        }
        HGDIOBJ old = SelectObject(hdc, font);
        const DWORD n = GetFontData(hdc, 0, 0, NULL, 0);
        if (n != GDI_ERROR && n > 0) {
            bytes->assign(n, 0);
            if (GetFontData(hdc, 0, 0, bytes->data(), n) == n) {
                ok = true;
            }
        }
        SelectObject(hdc, old);
        DeleteObject(font);
        if (ok) {
            break;
        }
    }
    DeleteDC(hdc);
    if (!ok) {
        if (error) {
            *error = "no system CJK TrueType font";
        }
        return false;
    }
    return true;
}

/************************************************************
 * 加载 SFNT / TTC
 ************************************************************/

const TtfSubset::Table* TtfSubset::findTable(uint32_t tag) const
{
    for (size_t i = 0; i < m_tables.size(); ++i) {
        if (m_tables[i].tag == tag) {
            return &m_tables[i];
        }
    }
    return 0;
}

bool TtfSubset::tableBytes(uint32_t tag, std::vector<unsigned char>* out, std::string* error) const
{
    const Table* t = findTable(tag);
    if (!t || t->offset + t->length > m_bytes.size()) {
        if (error) {
            *error = "missing font table";
        }
        return false;
    }
    out->assign(m_bytes.begin() + t->offset, m_bytes.begin() + t->offset + t->length);
    return true;
}

bool TtfSubset::loadFaceAt(const std::vector<unsigned char>& bytes, uint32_t faceOff, std::string* error)
{
    m_bytes.clear();
    m_tables.clear();
    m_numGlyphs = 0;
    m_unitsPerEm = 1000;
    m_indexToLocFormat = 0;
    m_numberOfHMetrics = 0;
    if (faceOff + 12 > bytes.size()) {
        if (error) {
            *error = "bad face offset";
        }
        return false;
    }
    const unsigned char* face = &bytes[faceOff];
    const uint32_t scaler = rd32(face);
    if (scaler != 0x00010000u && scaler != 0x74727565u) { // 1.0 or 'true'
        if (error) {
            *error = "unsupported SFNT";
        }
        return false;
    }
    const uint16_t numTables = rd16(face + 4);
    if (faceOff + 12 + static_cast<uint32_t>(numTables) * 16u > bytes.size()) {
        if (error) {
            *error = "table directory OOB";
        }
        return false;
    }
    m_bytes = bytes;
    m_tables.reserve(numTables);
    for (uint16_t i = 0; i < numTables; ++i) {
        const unsigned char* e = face + 12 + i * 16;
        Table t;
        t.tag = rd32(e);
        t.offset = rd32(e + 8);
        t.length = rd32(e + 12);
        if (static_cast<uint64_t>(t.offset) + t.length > m_bytes.size()) {
            if (error) {
                *error = "table OOB";
            }
            return false;
        }
        m_tables.push_back(t);
    }

    std::vector<unsigned char> head;
    if (!tableBytes(0x68656164u, &head, error) || head.size() < 54) { // head
        return false;
    }
    m_unitsPerEm = rd16(&head[18]);
    m_indexToLocFormat = rd16(&head[50]);

    std::vector<unsigned char> maxp;
    if (!tableBytes(0x6D617870u, &maxp, error) || maxp.size() < 6) { // maxp
        return false;
    }
    m_numGlyphs = rd16(&maxp[4]);
    if (m_numGlyphs == 0) {
        if (error) {
            *error = "no glyphs";
        }
        return false;
    }

    // 需要 glyf+loca，排除 CFF 脸
    if (!findTable(0x676C7966u) || !findTable(0x6C6F6361u)) {
        if (error) {
            *error = "not TrueType glyf face";
        }
        return false;
    }

    std::vector<unsigned char> hhea;
    if (!tableBytes(0x68686561u, &hhea, error) || hhea.size() < 36) { // hhea
        return false;
    }
    m_numberOfHMetrics = rd16(&hhea[34]);
    if (m_numberOfHMetrics == 0 || m_numberOfHMetrics > m_numGlyphs) {
        if (error) {
            *error = "bad hhea";
        }
        return false;
    }
    return true;
}

bool TtfSubset::load(const std::vector<unsigned char>& bytes, std::string* error)
{
    if (bytes.size() < 12) {
        if (error) {
            *error = "font too small";
        }
        return false;
    }
    const uint32_t tag0 = rd32(&bytes[0]);
    if (tag0 != 0x74746366u) { // 非 TTC，单脸
        return loadFaceAt(bytes, 0, error);
    }
    if (bytes.size() < 16) {
        if (error) {
            *error = "bad TTC";
        }
        return false;
    }
    const uint32_t numFonts = rd32(&bytes[8]);
    if (numFonts == 0 || bytes.size() < 12 + 4u * numFonts) {
        if (error) {
            *error = "empty TTC";
        }
        return false;
    }
    std::string lastErr = "no usable TTC face";
    for (uint32_t i = 0; i < numFonts; ++i) {
        const uint32_t faceOff = rd32(&bytes[12 + i * 4]);
        std::string err;
        if (loadFaceAt(bytes, faceOff, &err)) {
            return true;
        }
        lastErr = err;
    }
    if (error) {
        *error = lastErr;
    }
    return false;
}

/************************************************************
 * cmap / glyf / hmtx
 ************************************************************/

uint16_t TtfSubset::glyphForCode(uint32_t cp) const
{
    const Table* cmap = findTable(0x636D6170u);
    if (!cmap || cmap->length < 4) {
        return 0;
    }
    const unsigned char* base = &m_bytes[cmap->offset];
    const uint16_t numTables = rd16(base + 2);
    uint32_t bestOff = 0;
    int bestScore = -1;
    for (uint16_t i = 0; i < numTables; ++i) {
        if (4 + (i + 1u) * 8u > cmap->length) {
            break;
        }
        const unsigned char* r = base + 4 + i * 8;
        const uint16_t platform = rd16(r);
        const uint16_t encoding = rd16(r + 2);
        const uint32_t offset = rd32(r + 4);
        int score = -1;
        if (platform == 3 && encoding == 10) {
            score = 3; // Windows Unicode full
        } else if (platform == 3 && encoding == 1) {
            score = 2; // Windows BMP
        } else if (platform == 0) {
            score = 1; // Unicode
        }
        if (score > bestScore && offset + 2 <= cmap->length) {
            bestScore = score;
            bestOff = offset;
        }
    }
    if (bestScore < 0) {
        return 0;
    }
    const unsigned char* sub = base + bestOff;
    const uint16_t format = rd16(sub);
    if (format == 4) {
        if (cp > 0xFFFF) {
            return 0;
        }
        const uint16_t length = rd16(sub + 2);
        if (bestOff + length > cmap->length || length < 16) {
            return 0;
        }
        const uint16_t segCountX2 = rd16(sub + 6);
        const uint16_t segCount = segCountX2 / 2;
        const unsigned char* endCodes = sub + 14;
        const unsigned char* startCodes = endCodes + segCountX2 + 2;
        const unsigned char* idDeltas = startCodes + segCountX2;
        const unsigned char* idRangeOffsets = idDeltas + segCountX2;
        for (uint16_t i = 0; i < segCount; ++i) {
            const uint16_t end = rd16(endCodes + i * 2);
            const uint16_t start = rd16(startCodes + i * 2);
            if (cp < start || cp > end) {
                continue;
            }
            const uint16_t idRangeOffset = rd16(idRangeOffsets + i * 2);
            const int16_t idDelta = static_cast<int16_t>(rd16(idDeltas + i * 2));
            if (idRangeOffset == 0) {
                return static_cast<uint16_t>((cp + idDelta) & 0xFFFF);
            }
            const uint16_t* ro = reinterpret_cast<const uint16_t*>(idRangeOffsets + i * 2);
            (void)ro;
            const size_t index = idRangeOffset / 2 + (cp - start) - (segCount - i);
            const unsigned char* glyphIdPtr = idRangeOffsets + i * 2 + idRangeOffset + (cp - start) * 2;
            if (glyphIdPtr + 2 > sub + length) {
                return 0;
            }
            const uint16_t gid = rd16(glyphIdPtr);
            if (gid == 0) {
                return 0;
            }
            return static_cast<uint16_t>((gid + idDelta) & 0xFFFF);
        }
        return 0;
    }
    if (format == 12) {
        if (bestOff + 16 > cmap->length) {
            return 0;
        }
        const uint32_t nGroups = rd32(sub + 12);
        if (bestOff + 16 + nGroups * 12u > cmap->length) {
            return 0;
        }
        for (uint32_t i = 0; i < nGroups; ++i) {
            const unsigned char* g = sub + 16 + i * 12;
            const uint32_t start = rd32(g);
            const uint32_t end = rd32(g + 4);
            const uint32_t startGlyph = rd32(g + 8);
            if (cp >= start && cp <= end) {
                return static_cast<uint16_t>(startGlyph + (cp - start));
            }
        }
        return 0;
    }
    return 0;
}

bool TtfSubset::glyphData(uint16_t gid, std::vector<unsigned char>* out, std::string* error) const
{
    out->clear();
    if (gid >= m_numGlyphs) {
        if (error) {
            *error = "gid OOB";
        }
        return false;
    }
    const Table* loca = findTable(0x6C6F6361u);
    const Table* glyf = findTable(0x676C7966u);
    if (!loca || !glyf) {
        if (error) {
            *error = "missing loca/glyf";
        }
        return false;
    }
    uint32_t off = 0;
    uint32_t next = 0;
    if (m_indexToLocFormat == 0) {
        if (loca->offset + (gid + 1u) * 2u + 2u > m_bytes.size()) {
            return false;
        }
        off = static_cast<uint32_t>(rd16(&m_bytes[loca->offset + gid * 2])) * 2u;
        next = static_cast<uint32_t>(rd16(&m_bytes[loca->offset + (gid + 1) * 2])) * 2u;
    } else {
        if (loca->offset + (gid + 1u) * 4u + 4u > m_bytes.size()) {
            return false;
        }
        off = rd32(&m_bytes[loca->offset + gid * 4]);
        next = rd32(&m_bytes[loca->offset + (gid + 1) * 4]);
    }
    if (next < off || glyf->offset + next > m_bytes.size()) {
        if (error) {
            *error = "bad loca";
        }
        return false;
    }
    out->assign(m_bytes.begin() + glyf->offset + off, m_bytes.begin() + glyf->offset + next);
    return true;
}

void TtfSubset::collectComposite(uint16_t gid, std::map<uint16_t, bool>* used) const
{
    if (!used || used->count(gid)) {
        return;
    }
    (*used)[gid] = true;
    std::vector<unsigned char> data;
    if (!glyphData(gid, &data, 0) || data.size() < 10) {
        return;
    }
    const int16_t numContours = static_cast<int16_t>(rd16(&data[0]));
    if (numContours >= 0) {
        return;
    }
    size_t p = 10;
    for (;;) {
        if (p + 4 > data.size()) {
            break;
        }
        const uint16_t flags = rd16(&data[p]);
        const uint16_t comp = rd16(&data[p + 2]);
        p += 4;
        collectComposite(comp, used);
        if (flags & 1) { // ARG_1_AND_2_ARE_WORDS
            p += 4;
        } else {
            p += 2;
        }
        if (flags & 8) { // WE_HAVE_A_SCALE
            p += 2;
        } else if (flags & 64) { // WE_HAVE_AN_X_AND_Y_SCALE
            p += 4;
        } else if (flags & 128) { // WE_HAVE_A_TWO_BY_TWO
            p += 8;
        }
        if ((flags & 32) == 0) { // MORE_COMPONENTS
            break;
        }
    }
}

uint16_t TtfSubset::advance(uint16_t gid) const
{
    const Table* hmtx = findTable(0x686D7478u);
    if (!hmtx) {
        return 0;
    }
    uint16_t metricGid = gid;
    if (metricGid >= m_numberOfHMetrics) {
        metricGid = static_cast<uint16_t>(m_numberOfHMetrics - 1);
    }
    const uint32_t off = hmtx->offset + metricGid * 4u;
    if (off + 2 > m_bytes.size()) {
        return 0;
    }
    return rd16(&m_bytes[off]);
}

bool TtfSubset::remapComposite(std::vector<unsigned char>* glyf,
                               const std::map<uint16_t, uint16_t>& oldToNew,
                               std::string* error)
{
    if (!glyf || glyf->size() < 10) {
        return true;
    }
    const int16_t numContours = static_cast<int16_t>(rd16(&(*glyf)[0]));
    if (numContours >= 0) {
        return true;
    }
    size_t p = 10;
    for (;;) {
        if (p + 4 > glyf->size()) {
            if (error) {
                *error = "bad composite";
            }
            return false;
        }
        const uint16_t flags = rd16(&(*glyf)[p]);
        const uint16_t old = rd16(&(*glyf)[p + 2]);
        std::map<uint16_t, uint16_t>::const_iterator it = oldToNew.find(old);
        if (it == oldToNew.end()) {
            if (error) {
                *error = "missing composite component";
            }
            return false;
        }
        (*glyf)[p + 2] = static_cast<unsigned char>((it->second >> 8) & 0xFF);
        (*glyf)[p + 3] = static_cast<unsigned char>(it->second & 0xFF);
        p += 4;
        if (flags & 1) {
            p += 4;
        } else {
            p += 2;
        }
        if (flags & 8) {
            p += 2;
        } else if (flags & 64) {
            p += 4;
        } else if (flags & 128) {
            p += 8;
        }
        if ((flags & 32) == 0) {
            break;
        }
    }
    return true;
}

/************************************************************
 * 生成子集
 ************************************************************/

bool TtfSubset::build(const std::vector<uint32_t>& codepoints,
                      std::vector<unsigned char>* outTtf,
                      std::map<uint32_t, uint16_t>* gidOf,
                      std::map<uint16_t, uint16_t>* advanceOf,
                      uint16_t* unitsPerEm,
                      std::string* error) const
{
    if (!outTtf || !gidOf || !advanceOf || !unitsPerEm) {
        if (error) {
            *error = "null out";
        }
        return false;
    }
    outTtf->clear();
    gidOf->clear();
    advanceOf->clear();
    *unitsPerEm = m_unitsPerEm;

    std::map<uint16_t, bool> used;
    used[0] = true;
    for (size_t i = 0; i < codepoints.size(); ++i) {
        const uint32_t cp = codepoints[i];
        if (cp == 0) {
            continue;
        }
        const uint16_t gid = glyphForCode(cp);
        if (gid == 0 && cp != 0) {
            // 缺字时映射到 .notdef
            (*gidOf)[cp] = 0;
            continue;
        }
        collectComposite(gid, &used);
        (*gidOf)[cp] = gid; // 先记原始，稍后改成新 id
    }

    std::vector<uint16_t> oldGids;
    for (std::map<uint16_t, bool>::const_iterator it = used.begin(); it != used.end(); ++it) {
        oldGids.push_back(it->first);
    }
    std::sort(oldGids.begin(), oldGids.end());
    // 保证 0 在首位
    if (oldGids.empty() || oldGids[0] != 0) {
        oldGids.insert(oldGids.begin(), 0);
    }

    std::map<uint16_t, uint16_t> oldToNew;
    for (size_t i = 0; i < oldGids.size(); ++i) {
        oldToNew[oldGids[i]] = static_cast<uint16_t>(i);
    }
    for (std::map<uint32_t, uint16_t>::iterator it = gidOf->begin(); it != gidOf->end(); ++it) {
        std::map<uint16_t, uint16_t>::const_iterator m = oldToNew.find(it->second);
        it->second = (m == oldToNew.end()) ? 0 : m->second;
    }

    const uint16_t newGlyphCount = static_cast<uint16_t>(oldGids.size());
    std::vector<std::vector<unsigned char> > glyphs(newGlyphCount);
    for (size_t i = 0; i < oldGids.size(); ++i) {
        if (!glyphData(oldGids[i], &glyphs[i], error)) {
            return false;
        }
        if (!remapComposite(&glyphs[i], oldToNew, error)) {
            return false;
        }
        (*advanceOf)[static_cast<uint16_t>(i)] = advance(oldGids[i]);
    }

    // 组装各表
    std::vector<unsigned char> glyf;
    std::vector<unsigned char> loca;
    const bool longLoca = true;
    for (uint16_t i = 0; i < newGlyphCount; ++i) {
        wr32(&loca, static_cast<uint32_t>(glyf.size()));
        glyf.insert(glyf.end(), glyphs[i].begin(), glyphs[i].end());
        pad4(&glyf);
    }
    wr32(&loca, static_cast<uint32_t>(glyf.size()));

    std::vector<unsigned char> hmtx;
    for (uint16_t i = 0; i < newGlyphCount; ++i) {
        wr16(&hmtx, (*advanceOf)[i]);
        wr16(&hmtx, 0);
    }

    // cmap format 4（BMP）+ format 12（全）
    std::vector<std::pair<uint32_t, uint16_t> > cps;
    for (std::map<uint32_t, uint16_t>::const_iterator it = gidOf->begin(); it != gidOf->end(); ++it) {
        cps.push_back(*it);
    }
    std::sort(cps.begin(), cps.end());

    std::vector<unsigned char> cmap4;
    {
        std::vector<uint16_t> starts, ends, deltas, rangeOffsets;
        uint16_t segStart = 0;
        uint16_t segEnd = 0;
        uint16_t segGid = 0;
        bool open = false;
        for (size_t i = 0; i < cps.size(); ++i) {
            if (cps[i].first > 0xFFFF) {
                continue;
            }
            const uint16_t c = static_cast<uint16_t>(cps[i].first);
            const uint16_t g = cps[i].second;
            if (!open) {
                segStart = segEnd = c;
                segGid = g;
                open = true;
            } else if (c == segEnd + 1 && g == segGid + (c - segStart)) {
                segEnd = c;
            } else {
                starts.push_back(segStart);
                ends.push_back(segEnd);
                deltas.push_back(static_cast<uint16_t>(segGid - segStart));
                rangeOffsets.push_back(0);
                segStart = segEnd = c;
                segGid = g;
            }
        }
        if (open) {
            starts.push_back(segStart);
            ends.push_back(segEnd);
            deltas.push_back(static_cast<uint16_t>(segGid - segStart));
            rangeOffsets.push_back(0);
        }
        starts.push_back(0xFFFF);
        ends.push_back(0xFFFF);
        deltas.push_back(1);
        rangeOffsets.push_back(0);
        const uint16_t segCount = static_cast<uint16_t>(starts.size());
        const uint16_t segCountX2 = static_cast<uint16_t>(segCount * 2);
        uint16_t searchRange = 2;
        uint16_t entrySelector = 0;
        while (searchRange * 2 <= segCountX2) {
            searchRange = static_cast<uint16_t>(searchRange * 2);
            ++entrySelector;
        }
        const uint16_t rangeShift = static_cast<uint16_t>(segCountX2 - searchRange);
        wr16(&cmap4, 4);
        wr16(&cmap4, 0); // length 占位
        wr16(&cmap4, 0);
        wr16(&cmap4, segCountX2);
        wr16(&cmap4, searchRange);
        wr16(&cmap4, entrySelector);
        wr16(&cmap4, rangeShift);
        for (uint16_t i = 0; i < segCount; ++i) {
            wr16(&cmap4, ends[i]);
        }
        wr16(&cmap4, 0);
        for (uint16_t i = 0; i < segCount; ++i) {
            wr16(&cmap4, starts[i]);
        }
        for (uint16_t i = 0; i < segCount; ++i) {
            wr16(&cmap4, deltas[i]);
        }
        for (uint16_t i = 0; i < segCount; ++i) {
            wr16(&cmap4, rangeOffsets[i]);
        }
        const uint16_t len = static_cast<uint16_t>(cmap4.size());
        cmap4[2] = static_cast<unsigned char>((len >> 8) & 0xFF);
        cmap4[3] = static_cast<unsigned char>(len & 0xFF);
    }

    std::vector<unsigned char> cmap12;
    {
        wr16(&cmap12, 12);
        wr16(&cmap12, 0);
        wr32(&cmap12, 0); // length 占位
        wr32(&cmap12, 0);
        wr32(&cmap12, 0); // nGroups 占位
        uint32_t nGroups = 0;
        bool open = false;
        uint32_t segStart = 0, segEnd = 0, segGid = 0;
        for (size_t i = 0; i < cps.size(); ++i) {
            const uint32_t c = cps[i].first;
            const uint32_t g = cps[i].second;
            if (!open) {
                segStart = segEnd = c;
                segGid = g;
                open = true;
            } else if (c == segEnd + 1 && g == segGid + (c - segStart)) {
                segEnd = c;
            } else {
                wr32(&cmap12, segStart);
                wr32(&cmap12, segEnd);
                wr32(&cmap12, segGid);
                ++nGroups;
                segStart = segEnd = c;
                segGid = g;
            }
        }
        if (open) {
            wr32(&cmap12, segStart);
            wr32(&cmap12, segEnd);
            wr32(&cmap12, segGid);
            ++nGroups;
        }
        const uint32_t len = static_cast<uint32_t>(cmap12.size());
        cmap12[4] = static_cast<unsigned char>((len >> 24) & 0xFF);
        cmap12[5] = static_cast<unsigned char>((len >> 16) & 0xFF);
        cmap12[6] = static_cast<unsigned char>((len >> 8) & 0xFF);
        cmap12[7] = static_cast<unsigned char>(len & 0xFF);
        cmap12[12] = static_cast<unsigned char>((nGroups >> 24) & 0xFF);
        cmap12[13] = static_cast<unsigned char>((nGroups >> 16) & 0xFF);
        cmap12[14] = static_cast<unsigned char>((nGroups >> 8) & 0xFF);
        cmap12[15] = static_cast<unsigned char>(nGroups & 0xFF);
    }

    std::vector<unsigned char> cmap;
    wr16(&cmap, 0);
    wr16(&cmap, 2);
    wr16(&cmap, 3);
    wr16(&cmap, 1);
    wr32(&cmap, 20);
    wr16(&cmap, 3);
    wr16(&cmap, 10);
    wr32(&cmap, 20 + static_cast<uint32_t>(cmap4.size()));
    cmap.insert(cmap.end(), cmap4.begin(), cmap4.end());
    cmap.insert(cmap.end(), cmap12.begin(), cmap12.end());

    // 复制并改写 head / hhea / maxp
    std::vector<unsigned char> head;
    if (!tableBytes(0x68656164u, &head, error)) {
        return false;
    }
    head[8] = head[9] = head[10] = head[11] = 0; // checkSumAdjustment
    head[50] = 0;
    head[51] = 1; // indexToLocFormat = 1

    std::vector<unsigned char> hhea;
    if (!tableBytes(0x68686561u, &hhea, error)) {
        return false;
    }
    hhea[34] = static_cast<unsigned char>((newGlyphCount >> 8) & 0xFF);
    hhea[35] = static_cast<unsigned char>(newGlyphCount & 0xFF);

    std::vector<unsigned char> maxp;
    if (!tableBytes(0x6D617870u, &maxp, error)) {
        return false;
    }
    maxp[4] = static_cast<unsigned char>((newGlyphCount >> 8) & 0xFF);
    maxp[5] = static_cast<unsigned char>(newGlyphCount & 0xFF);

    // name / OS/2 / post 尽量原样拷贝（有则拷）
    struct OutTable {
        uint32_t tag;
        std::vector<unsigned char> data;
    };
    std::vector<OutTable> outs;
    OutTable ot;
    ot.tag = 0x68656164u;
    ot.data = head;
    outs.push_back(ot);
    ot.tag = 0x68686561u;
    ot.data = hhea;
    outs.push_back(ot);
    ot.tag = 0x6D617870u;
    ot.data = maxp;
    outs.push_back(ot);
    ot.tag = 0x686D7478u;
    ot.data = hmtx;
    outs.push_back(ot);
    ot.tag = 0x6C6F6361u;
    ot.data = loca;
    outs.push_back(ot);
    ot.tag = 0x676C7966u;
    ot.data = glyf;
    outs.push_back(ot);
    ot.tag = 0x636D6170u;
    ot.data = cmap;
    outs.push_back(ot);

    const uint32_t optionalTags[] = { 0x6E616D65u, 0x4F532F32u, 0x706F7374u }; // name OS/2 post
    for (size_t i = 0; i < sizeof(optionalTags) / sizeof(optionalTags[0]); ++i) {
        std::vector<unsigned char> data;
        std::string ignore;
        if (tableBytes(optionalTags[i], &data, &ignore)) {
            ot.tag = optionalTags[i];
            ot.data.swap(data);
            outs.push_back(ot);
        }
    }

    std::sort(outs.begin(), outs.end(), [](const OutTable& a, const OutTable& b) { return a.tag < b.tag; });

    const uint16_t numTables = static_cast<uint16_t>(outs.size());
    uint16_t entrySelector = 0;
    uint16_t searchRange = 16;
    while ((searchRange << 1) <= numTables * 16u) {
        searchRange = static_cast<uint16_t>(searchRange << 1);
        ++entrySelector;
    }
    const uint16_t rangeShift = static_cast<uint16_t>(numTables * 16 - searchRange);

    std::vector<unsigned char> file;
    wr32(&file, 0x00010000u);
    wr16(&file, numTables);
    wr16(&file, searchRange);
    wr16(&file, entrySelector);
    wr16(&file, rangeShift);

    const uint32_t dirStart = static_cast<uint32_t>(file.size());
    for (uint16_t i = 0; i < numTables; ++i) {
        wr32(&file, 0);
        wr32(&file, 0);
        wr32(&file, 0);
        wr32(&file, 0);
    }

    std::vector<uint32_t> offsets(numTables);
    std::vector<uint32_t> lengths(numTables);
    for (uint16_t i = 0; i < numTables; ++i) {
        pad4(&file);
        offsets[i] = static_cast<uint32_t>(file.size());
        lengths[i] = static_cast<uint32_t>(outs[i].data.size());
        file.insert(file.end(), outs[i].data.begin(), outs[i].data.end());
    }
    pad4(&file);

    for (uint16_t i = 0; i < numTables; ++i) {
        unsigned char* e = &file[dirStart + i * 16];
        e[0] = static_cast<unsigned char>((outs[i].tag >> 24) & 0xFF);
        e[1] = static_cast<unsigned char>((outs[i].tag >> 16) & 0xFF);
        e[2] = static_cast<unsigned char>((outs[i].tag >> 8) & 0xFF);
        e[3] = static_cast<unsigned char>(outs[i].tag & 0xFF);
        const uint32_t sum = checksum(file.data() + offsets[i], lengths[i]);
        e[4] = static_cast<unsigned char>((sum >> 24) & 0xFF);
        e[5] = static_cast<unsigned char>((sum >> 16) & 0xFF);
        e[6] = static_cast<unsigned char>((sum >> 8) & 0xFF);
        e[7] = static_cast<unsigned char>(sum & 0xFF);
        e[8] = static_cast<unsigned char>((offsets[i] >> 24) & 0xFF);
        e[9] = static_cast<unsigned char>((offsets[i] >> 16) & 0xFF);
        e[10] = static_cast<unsigned char>((offsets[i] >> 8) & 0xFF);
        e[11] = static_cast<unsigned char>(offsets[i] & 0xFF);
        e[12] = static_cast<unsigned char>((lengths[i] >> 24) & 0xFF);
        e[13] = static_cast<unsigned char>((lengths[i] >> 16) & 0xFF);
        e[14] = static_cast<unsigned char>((lengths[i] >> 8) & 0xFF);
        e[15] = static_cast<unsigned char>(lengths[i] & 0xFF);
    }

    // head.checkSumAdjustment
    uint32_t whole = checksum(file.data(), static_cast<uint32_t>(file.size()));
    uint32_t adjust = 0xB1B0AFBAu - whole;
    // 找到 head 表
    for (uint16_t i = 0; i < numTables; ++i) {
        if (outs[i].tag == 0x68656164u) {
            unsigned char* p = &file[offsets[i] + 8];
            p[0] = static_cast<unsigned char>((adjust >> 24) & 0xFF);
            p[1] = static_cast<unsigned char>((adjust >> 16) & 0xFF);
            p[2] = static_cast<unsigned char>((adjust >> 8) & 0xFF);
            p[3] = static_cast<unsigned char>(adjust & 0xFF);
            break;
        }
    }

    outTtf->swap(file);
    (void)longLoca;
    return true;
}

}
