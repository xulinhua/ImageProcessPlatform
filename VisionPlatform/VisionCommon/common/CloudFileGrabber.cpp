/**
 * @file CloudFileGrabber.cpp
 * @brief CloudFileGrabber 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CloudFileGrabber.h"
#include "ParamKeys.h"
#include "TextEncoding.h"
#include "VpUtil.h"
#include <algorithm>
#include <sstream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

std::string CloudFileGrabber::joinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) {
        return b;
    }
    const char c = a[a.size() - 1];
    if (c == '\\' || c == '/') {
        return a + b;
    }
    return a + "\\" + b;
}

std::string CloudFileGrabber::extLower(const std::string& n)
{
    const size_t dot = n.find_last_of('.');
    if (dot == std::string::npos) {
        return std::string();
    }
    return vpLower(n.substr(dot + 1));
}

bool CloudFileGrabber::isCloudName(const std::string& n)
{
    const std::string e = extLower(n);
    return e == "ply" || e == "pcd";
}

bool CloudFileGrabber::pathExists(const std::string& p)
{
    const DWORD a = GetFileAttributesW(utf8ToWide(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}

bool CloudFileGrabber::isDir(const std::string& p)
{
    const DWORD a = GetFileAttributesW(utf8ToWide(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::string CloudFileGrabber::trimLine(const std::string& line)
{
    return vpTrim(line);
}

bool CloudFileGrabber::splitDoubles(const std::string& line, std::vector<double>* nums)
{
    if (!nums) {
        return false;
    }
    nums->clear();
    std::istringstream in(line);
    double v = 0;
    while (in >> v) {
        nums->push_back(v);
    }
    return !nums->empty();
}

std::vector<std::string> CloudFileGrabber::cloudFiles(const std::string& path)
{
    std::vector<std::string> out;
    if (path.empty()) {
        return out;
    }
    if (!isDir(path)) {
        if (pathExists(path) && isCloudName(path)) {
            out.push_back(path);
        }
        return out;
    }
    const std::wstring pattern = utf8ToWide(joinPath(path, "*"));
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return out;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        const std::string name = wideToUtf8(fd.cFileName);
        if (isCloudName(name)) {
            out.push_back(joinPath(path, name));
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

bool CloudFileGrabber::readPly(FILE* fp, HsCommData::Cloud3* cloud, std::string* err)
{
    bool binary = false;
    int vertexCount = 0;
    bool inVertex = false;
    std::vector<std::string> props;
    char buf[4096];
    while (fgets(buf, static_cast<int>(sizeof(buf)), fp)) {
        const std::string line = trimLine(buf);
        if (line.empty()) {
            continue;
        }
        if (line.find("format binary") == 0) {
            binary = true;
        }
        if (line.find("element ") == 0) {
            inVertex = line.find("vertex") != std::string::npos;
            if (inVertex) {
                std::istringstream in(line);
                std::string element;
                std::string kind;
                in >> element >> kind >> vertexCount;
                props.clear();
            }
        }
        if (inVertex && line.find("property ") == 0) {
            std::istringstream in(line);
            std::string property;
            std::string type;
            std::string name;
            in >> property >> type >> name;
            props.push_back(name);
        }
        if (line == "end_header") {
            break;
        }
    }
    if (binary) {
        if (err) {
            *err = "只支持 ASCII 点云";
        }
        return false;
    }
    int ix = -1;
    int iy = -1;
    int iz = -1;
    for (int i = 0; i < static_cast<int>(props.size()); ++i) {
        if (props[static_cast<size_t>(i)] == "x") {
            ix = i;
        } else if (props[static_cast<size_t>(i)] == "y") {
            iy = i;
        } else if (props[static_cast<size_t>(i)] == "z") {
            iz = i;
        }
    }
    if (ix < 0 || iy < 0 || iz < 0 || vertexCount < 0) {
        if (err) {
            *err = "PLY 里没有 x y z";
        }
        return false;
    }
    cloud->xyz.clear();
    cloud->xyz.reserve(static_cast<size_t>(vertexCount));
    for (int n = 0; n < vertexCount; ++n) {
        if (!fgets(buf, static_cast<int>(sizeof(buf)), fp)) {
            if (err) {
                *err = "PLY 顶点不足";
            }
            return false;
        }
        std::vector<double> nums;
        if (!splitDoubles(buf, &nums) || iz >= static_cast<int>(nums.size())) {
            if (err) {
                *err = "PLY 顶点行无法解析";
            }
            return false;
        }
        cloud->xyz.push_back(HsCommData::Point3d(nums[static_cast<size_t>(ix)],
                                                 nums[static_cast<size_t>(iy)],
                                                 nums[static_cast<size_t>(iz)]));
    }
    cloud->width = 0;
    cloud->height = 0;
    return true;
}

bool CloudFileGrabber::readPcd(FILE* fp, HsCommData::Cloud3* cloud, std::string* err)
{
    std::vector<std::string> fields;
    int points = -1;
    int width = 0;
    int height = 0;
    bool dataAscii = false;
    bool sawData = false;
    char buf[4096];
    while (fgets(buf, static_cast<int>(sizeof(buf)), fp)) {
        const std::string line = trimLine(buf);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line.find("FIELDS") == 0 || line.find("FIELDS\t") == 0) {
            std::istringstream in(line);
            std::string tag;
            in >> tag;
            std::string name;
            while (in >> name) {
                fields.push_back(name);
            }
            continue;
        }
        if (line.find("POINTS") == 0) {
            std::istringstream in(line);
            std::string tag;
            in >> tag >> points;
            continue;
        }
        if (line.find("WIDTH") == 0) {
            std::istringstream in(line);
            std::string tag;
            in >> tag >> width;
            continue;
        }
        if (line.find("HEIGHT") == 0) {
            std::istringstream in(line);
            std::string tag;
            in >> tag >> height;
            continue;
        }
        if (line.find("DATA") == 0) {
            sawData = true;
            if (line.find("ascii") == std::string::npos) {
                if (err) {
                    *err = "只支持 ASCII 点云";
                }
                return false;
            }
            dataAscii = true;
            break;
        }
    }
    if (!sawData || !dataAscii) {
        if (err) {
            *err = "不是 ASCII PCD";
        }
        return false;
    }
    if (points < 0) {
        points = width * height;
    }
    int ix = -1;
    int iy = -1;
    int iz = -1;
    for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
        if (fields[static_cast<size_t>(i)] == "x") {
            ix = i;
        } else if (fields[static_cast<size_t>(i)] == "y") {
            iy = i;
        } else if (fields[static_cast<size_t>(i)] == "z") {
            iz = i;
        }
    }
    if (ix < 0 || iy < 0 || iz < 0 || points < 0) {
        if (err) {
            *err = "PCD 里没有 x y z";
        }
        return false;
    }
    cloud->xyz.clear();
    cloud->xyz.reserve(static_cast<size_t>(points));
    for (int n = 0; n < points; ++n) {
        if (!fgets(buf, static_cast<int>(sizeof(buf)), fp)) {
            if (err) {
                *err = "PCD 点数不足";
            }
            return false;
        }
        std::vector<double> nums;
        if (!splitDoubles(buf, &nums) || iz >= static_cast<int>(nums.size())) {
            if (err) {
                *err = "PCD 点行无法解析";
            }
            return false;
        }
        cloud->xyz.push_back(HsCommData::Point3d(nums[static_cast<size_t>(ix)],
                                                 nums[static_cast<size_t>(iy)],
                                                 nums[static_cast<size_t>(iz)]));
    }
    cloud->width = 0;
    cloud->height = 0;
    return true;
}

bool CloudFileGrabber::load(const std::string& file, HsCommData::Cloud3* out, std::string* err)
{
    if (!out) {
        return false;
    }
    if (file.empty()) {
        *out = HsCommData::Cloud3();
        return true;
    }
    const std::wstring wide = utf8ToWide(file);
    FILE* fp = 0;
    if (_wfopen_s(&fp, wide.c_str(), L"rb") != 0 || !fp) {
        if (err) {
            *err = "打不开点云文件";
        }
        return false;
    }
    char buf[4096];
    if (!fgets(buf, static_cast<int>(sizeof(buf)), fp)) {
        fclose(fp);
        if (err) {
            *err = "点云文件为空";
        }
        return false;
    }
    const std::string first = trimLine(buf);
    bool ok = false;
    if (first == "ply") {
        ok = readPly(fp, out, err);
    } else {
        fseek(fp, 0, SEEK_SET);
        ok = readPcd(fp, out, err);
    }
    fclose(fp);
    return ok;
}

bool CloudFileGrabber::grab(const std::string& path, const std::string& sourceMode,
                            ParamMap* runtime, HsCommData::Cloud3* out, std::string* err)
{
    if (!out) {
        return false;
    }
    ParamMap local;
    if (!runtime) {
        runtime = &local;
    }
    const std::string prevPath = runtimeGet(*runtime, RuntimeKey::GrabPath).toString();
    if (!path.empty() && path != prevPath) {
        runtimeSet(runtime, RuntimeKey::Index, 0);
        runtimeSet(runtime, RuntimeKey::GrabPath, path);
    }
    if (sourceMode == "file") {
        if (path.empty() || isDir(path) || !pathExists(path)) {
            if (err) {
                *err = "请指定有效的单个点云文件";
            }
            return false;
        }
        if (!load(path, out, err)) {
            return false;
        }
        runtimeSet(runtime, RuntimeKey::Index, 0);
        runtimeSet(runtime, RuntimeKey::LastFile, path);
        return true;
    }
    const std::vector<std::string> files = cloudFiles(path);
    if (files.empty()) {
        if (err) {
            *err = "没有可用点云。请指定文件夹或单个 PLY/PCD 文件。";
        }
        return false;
    }
    int idx = runtimeGet(*runtime, RuntimeKey::Index).toInt();
    if (idx < 0 || idx >= static_cast<int>(files.size())) {
        idx = 0;
    }
    if (!load(files[static_cast<size_t>(idx)], out, err)) {
        return false;
    }
    runtimeSet(runtime, RuntimeKey::Index, (idx + 1) % static_cast<int>(files.size()));
    runtimeSet(runtime, RuntimeKey::LastFile, files[static_cast<size_t>(idx)]);
    return true;
}

}
