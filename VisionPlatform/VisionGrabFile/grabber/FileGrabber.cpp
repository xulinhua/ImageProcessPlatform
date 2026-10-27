/**
 * @file FileGrabber.cpp
 * @brief FileGrabber实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "FileGrabber.h"
#include "ParamKeys.h"
#include "TextEncoding.h"
#include "VpUtil.h"
#include <algorithm>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

// 拼接路径
static std::string joinPath(const std::string& a, const std::string& b)
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

// 扩展名转小写
static std::string extLower(const std::string& n)
{
    const size_t dot = n.find_last_of('.');
    if (dot == std::string::npos) {
        return std::string();
    }
    return vpLower(n.substr(dot + 1));
}

// 是否为常见图像名
static bool isImageName(const std::string& n)
{
    const std::string e = extLower(n);
    return e == "png" || e == "bmp" || e == "jpg" || e == "jpeg" || e == "tif" || e == "tiff";
}

// 路径是否存在（ACP 路径转宽字符）
static bool pathExists(const std::string& p)
{
    const DWORD a = GetFileAttributesW(utf8ToWide(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}

// 是否目录
static bool isDir(const std::string& p)
{
    const DWORD a = GetFileAttributesW(utf8ToWide(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// 枚举目录下图像，或单文件则只返回该文件
std::vector<std::string> FileGrabber::imageFiles(const std::string& path)
{
    std::vector<std::string> out;
    if (path.empty()) {
        return out;
    }
    if (!isDir(path)) {
        if (pathExists(path) && isImageName(path)) {
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
        if (isImageName(name)) {
            out.push_back(joinPath(path, name));
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

// sourceMode=file 固定单文件；否则目录循环（写 runtime）
static HsCommData::HsImage loadWithStb(const std::string& file, int64_t frameId, std::string* err);

HsCommData::HsImage FileGrabber::grab(const std::string& path, const std::string& sourceMode,
                                      ParamMap* runtime, int64_t frameId, std::string* err)
{
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
                *err = Vision::acpToUtf8("请指定有效的单个图像文件");
            }
            return HsCommData::HsImage();
        }
        HsCommData::HsImage fb = loadWithStb(path, frameId, err);
        if (!fb.isNull()) {
            runtimeSet(runtime, RuntimeKey::Index, 0);
            runtimeSet(runtime, RuntimeKey::LastFile, path);
        }
        return fb;
    }
    const std::vector<std::string> files = imageFiles(path);
    if (files.empty()) {
        if (err) {
            *err = Vision::acpToUtf8("没有可用图像。请在设备或相机工具里指定文件夹/文件。");
        }
        return HsCommData::HsImage();
    }
    int idx = runtimeGet(*runtime, RuntimeKey::Index).toInt();
    if (idx < 0 || idx >= static_cast<int>(files.size())) {
        idx = 0;
    }
    HsCommData::HsImage fb = loadWithStb(files[static_cast<size_t>(idx)], frameId, err);
    if (fb.isNull()) {
        return fb;
    }
    runtimeSet(runtime, RuntimeKey::Index, (idx + 1) % static_cast<int>(files.size()));
    runtimeSet(runtime, RuntimeKey::LastFile, files[static_cast<size_t>(idx)]);
    return fb;
}
static HsCommData::HsImage loadWithStb(const std::string& file, int64_t frameId, std::string* err)
{
    int w = 0, h = 0, n = 0;
    FILE* f = _wfopen(utf8ToWide(file).c_str(), L"rb");
    if (!f) {
        if (err) {
            *err = Vision::acpToUtf8("无法打开图像：") + file;
        }
        return HsCommData::HsImage();
    }
    unsigned char* data = stbi_load_from_file(f, &w, &h, &n, 0);
    fclose(f);
    if (!data || w <= 0 || h <= 0) {
        if (err) {
            *err = Vision::acpToUtf8("无法解码图像：") + file;
        }
        if (data) {
            stbi_image_free(data);
        }
        return HsCommData::HsImage();
    }
    HsCommData::PixelEncoding enc = HsCommData::EncodeUnknown;
    int dstN = n;
    if (n == 1) {
        enc = HsCommData::EncodeMono;
    } else if (n == 3) {
        enc = HsCommData::EncodeRgb;
    } else if (n == 4) {
        enc = HsCommData::EncodeRgba;
    } else if (n == 2) {
        enc = HsCommData::EncodeMono;
        dstN = 1;
    } else {
        enc = HsCommData::EncodeRgb;
        dstN = 3;
    }
    HsCommData::HsImage fb = HsCommData::HsImage::make(w, h, HsCommData::PixelByte, enc, frameId);
    if (n == dstN) {
        memcpy(fb.pixels.empty() ? 0 : &fb.pixels[0], data, static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(n));
    } else if (n == 2 && dstN == 1) {
        for (int i = 0; i < w * h; ++i) {
            fb.pixels[static_cast<size_t>(i)] = data[static_cast<size_t>(i) * 2];
        }
    }
    stbi_image_free(data);
    return fb;
}

}