/**
 * @file VisionVersion.cpp
 * @brief VisionVersion实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "VisionVersion.h"
#include "TextEncoding.h"

namespace Vision {

// 产品显示名
const char* appProductName()
{
    static const std::string name = "通用视觉平台";
    return name.c_str();
}

// 软件版本
const char* appVersion()
{
    return VISION_VERSION_STRING;
}

// 编译日期时间
std::string buildStamp()
{
    return std::string(__DATE__) + " " + __TIME__;
}

// 静态模块表（与当前链接的库对应）
std::vector<VersionInfo> moduleVersions()
{
    std::vector<VersionInfo> out;
    VersionInfo a;
    a.id = "vision.platform";
    a.name = "主程序 VisionPlatform";
    a.version = VISION_VERSION_STRING;
    out.push_back(a);

    VersionInfo core;
    core.id = "vision.core";
    core.name = "核心 VisionCore";
    core.version = VISION_MODULE_CORE_VERSION;
    out.push_back(core);

    VersionInfo comm;
    comm.id = "vision.comm";
    comm.name = "通信 VisionComm";
    comm.version = VISION_MODULE_COMM_VERSION;
    out.push_back(comm);

    VersionInfo base;
    base.id = "vision.algobase";
    base.name = "算法基类 VisionAlgToolBase";
    base.version = VISION_MODULE_ALGOBASE_VERSION;
    out.push_back(base);

    VersionInfo hal;
    hal.id = "vision.algohalcon";
    hal.name = "Halcon 算法插件";
    hal.version = VISION_MODULE_HALCON_VERSION;
    out.push_back(hal);

    VersionInfo grab;
    grab.id = "vision.grabfile";
    grab.name = "文件采集 VisionGrabFile";
    grab.version = VISION_MODULE_GRABFILE_VERSION;
    out.push_back(grab);

    VersionInfo img;
    img.id = "hsimage";
    img.name = "图像控件 HsImage";
    img.version = VISION_MODULE_HSIMAGE_VERSION;
    out.push_back(img);

    return out;
}


// 取四段里的一段。缺段当 0
static int verPart(const std::string& s, int index)
{
    int cur = 0;
    int val = 0;
    bool in = false;
    for (size_t i = 0; i <= s.size(); ++i) {
        const char c = (i < s.size()) ? s[i] : '.';
        if (c >= '0' && c <= '9') {
            val = val * 10 + (c - '0');
            in = true;
        } else if (c == '.') {
            if (cur == index) {
                return in ? val : 0;
            }
            ++cur;
            val = 0;
            in = false;
        }
    }
    return 0;
}

// 四段从左到右比较
static int cmpVerText(const std::string& a, const std::string& b)
{
    for (int i = 0; i < 4; ++i) {
        const int da = verPart(a, i);
        const int db = verPart(b, i);
        if (da < db) {
            return -1;
        }
        if (da > db) {
            return 1;
        }
    }
    return 0;
}

// 变更表转成界面用的 UTF-8
std::vector<VerNote> verNotesFrom(const VerNoteRaw* rows, int count)
{
    std::vector<VerNote> out;
    if (!rows || count <= 0) {
        return out;
    }
    for (int i = 0; i < count; ++i) {
        VerNote n;
        if (rows[i].ver) {
            n.ver = rows[i].ver;
        }
        if (rows[i].note) {
            n.note = rows[i].note;
        }
        out.push_back(n);
    }
    return out;
}

// 方案号之后、当前号为止的说明
std::string verNotesBetween(const std::vector<VerNote>& notes, const std::string& saved, const std::string& current)
{
    const std::string from = saved.empty() ? std::string("0.0.0.0") : saved;
    const std::string to = current.empty() ? std::string("0.0.0.0") : current;
    if (cmpVerText(from, to) > 0) {
        return "本程序只到 " + to;
    }
    std::string out;
    for (size_t i = 0; i < notes.size(); ++i) {
        if (cmpVerText(from, notes[i].ver) < 0 && cmpVerText(notes[i].ver, to) <= 0) {
            if (!out.empty()) {
                out += "\n";
            }
            out += notes[i].ver;
            out += "  ";
            out += notes[i].note;
        }
    }
    return out;
}



}