/**
 * @file RawArchive.cpp
 * @brief 原图按工站和日期落盘
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RawArchive.h"
#include "TextEncoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>

namespace Vision {

namespace {

void ensureDir(const std::wstring& path)
{
    CreateDirectoryW(path.c_str(), NULL);
}

}

void RawArchive::setRoot(const std::string& rawDir)
{
    m_root = rawDir;
}

std::string RawArchive::save(const std::string& stationId, uint32_t frameSeq, const HsCommData::HsImage& image) const
{
    if (m_root.empty() || image.isNull() || stationId.empty()) {
        return std::string();
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    char day[16];
    sprintf_s(day, "%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
    char name[32];
    sprintf_s(name, "%u.png", frameSeq);
    const std::string dir = m_root + "/" + stationId + "/" + day;
    ensureDir(utf8ToWide(m_root));
    ensureDir(utf8ToWide(m_root + "/" + stationId));
    ensureDir(utf8ToWide(dir));
    const std::string path = dir + "/" + name;
    if (!image.save(path, HsCommData::ImageFormatPng)) {
        return std::string();
    }
    return path;
}

}
