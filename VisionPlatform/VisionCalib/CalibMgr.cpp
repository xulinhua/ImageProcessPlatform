/**
 * @file CalibMgr.cpp
 * @brief CalibMgr实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CalibMgr.h"
#include "CalibDb.h"
#include "CalibJson.h"
#include "CalibMath.h"
#include "SolutionDataPaths.h"
#include "TextEncoding.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <map>
#include <mutex>
#include <sstream>
#include <stdlib.h>

namespace Vision {

namespace {

std::string U(const wchar_t* w)
{
    std::string o;
    if (!w) {
        return o;
    }
    for (; *w; ++w) {
        unsigned int c = (unsigned int)(*w);
        if (c < 0x80u) {
            o.push_back((char)c);
        } else if (c < 0x800u) {
            o.push_back((char)(0xC0u | (c >> 6)));
            o.push_back((char)(0x80u | (c & 0x3Fu)));
        } else {
            o.push_back((char)(0xE0u | (c >> 12)));
            o.push_back((char)(0x80u | ((c >> 6) & 0x3Fu)));
            o.push_back((char)(0x80u | (c & 0x3Fu)));
        }
    }
    return o;
}

std::string nowStamp()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[32];
    sprintf_s(buf, "%04u%02u%02u%02u%02u%02u",
              (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
              (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond);
    return buf;
}

bool safeId(const std::string& id)
{
    if (id.empty() || id == "." || id == "..") {
        return false;
    }
    const char* bad = "\\/:*?\"<>|";
    return id.find_first_of(bad) == std::string::npos;
}

std::string kindFile(CalibKind kind)
{
    if (kind == CalibKindPlate) {
        return "camera.json";
    }
    if (kind == CalibKindRotate) {
        return "rotate.json";
    }
    return "plane.json";
}

bool readDoubles(const std::string& line, std::vector<double>* nums)
{
    nums->clear();
    std::string cur;
    for (size_t i = 0; i <= line.size(); ++i) {
        const char c = i < line.size() ? line[i] : ',';
        if (c == ',' || c == ';' || c == '\t' || c == ' ') {
            if (!cur.empty()) {
                char* end = nullptr;
                const double v = strtod(cur.c_str(), &end);
                if (!end || *end != '\0') {
                    return false;
                }
                nums->push_back(v);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    return true;
}

struct Slot {
    ObjectId id;
    std::string name;
    int imageWidth;
    int imageHeight;
    CalibParam param;
    CamModel camera;
    PlaneMap plane;
    RotateCenter rotate;
    CamModel cameraDraft;
    PlaneMap planeDraft;
    RotateCenter rotateDraft;

    Slot()
        : imageWidth(0)
        , imageHeight(0)
    {}
};

}

struct CalibMgr::Impl {
    std::string dir;
    std::map<std::string, Slot> slots;
    mutable std::mutex mu;

    std::string root() const
    {
        if (dir.empty()) {
            return std::string();
        }
        return pathJoin(dir, "calib");
    }

    std::string camDir(const std::string& id) const
    {
        return pathJoin(root(), id);
    }

    Slot* find(const std::string& id)
    {
        std::map<std::string, Slot>::iterator it = slots.find(id);
        return it == slots.end() ? 0 : &it->second;
    }

    const Slot* find(const std::string& id) const
    {
        std::map<std::string, Slot>::const_iterator it = slots.find(id);
        return it == slots.end() ? 0 : &it->second;
    }

    Slot& ensure(const std::string& id)
    {
        Slot* s = find(id);
        if (s) {
            return *s;
        }
        Slot created;
        created.id = ObjectId(id);
        slots[id] = created;
        return slots[id];
    }

    bool archive(const std::string& id, CalibKind kind, bool wasValid);
};

CalibMgr& CalibMgr::instance()
{
    static CalibMgr inst;
    return inst;
}

CalibMgr::CalibMgr()
    : m_impl(new Impl())
{}

CalibMgr::~CalibMgr()
{
    if (m_impl) {
        delete m_impl;
        m_impl = nullptr;
    }
}

void CalibMgr::unload()
{
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->slots.clear();
    m_impl->dir.clear();
}

void CalibMgr::setSolutionDir(const std::string& solutionDir)
{
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->dir = solutionDir;
}

bool CalibMgr::load(const std::string& solutionDir, std::string* err)
{
    if (!m_impl) {
        if (err) {
            *err = U(L"\u6807\u5B9A\u7BA1\u7406\u672A\u521D\u59CB\u5316");
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->slots.clear();
    m_impl->dir = solutionDir;
    if (solutionDir.empty()) {
        return true;
    }
    const std::string root = m_impl->root();
    if (!pathFileExists(root) && GetFileAttributesW(utf8ToWide(root).c_str()) == INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    const std::wstring pattern = utf8ToWide(pathJoin(root, "*"));
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return true;
    }
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }
        char nameBuf[512];
        WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nameBuf, (int)sizeof(nameBuf), 0, 0);
        const std::string id = nameBuf;
        if (!safeId(id) || id == "history") {
            continue;
        }
        Slot slot;
        slot.id = ObjectId(id);
        const std::string folder = m_impl->camDir(id);
        std::string text;
        if (calibReadText(pathJoin(folder, "param.json"), &text)) {
            std::string perr;
            JsonValue j = JsonValue::parse(text, &perr);
            paramFromJson(j, &slot.name, &slot.imageWidth, &slot.imageHeight, &slot.param);
        }
        text.clear();
        if (calibReadText(pathJoin(folder, "camera.json"), &text)) {
            std::string perr;
            cameraFromJson(JsonValue::parse(text, &perr), &slot.camera);
        }
        text.clear();
        if (calibReadText(pathJoin(folder, "plane.json"), &text)) {
            std::string perr;
            planeFromJson(JsonValue::parse(text, &perr), &slot.plane);
        }
        text.clear();
        if (calibReadText(pathJoin(folder, "rotate.json"), &text)) {
            std::string perr;
            rotateFromJson(JsonValue::parse(text, &perr), &slot.rotate);
        }
        text.clear();
        if (calibReadText(pathJoin(folder, "camera.draft.json"), &text)) {
            std::string perr;
            cameraFromJson(JsonValue::parse(text, &perr), &slot.cameraDraft);
        }
        text.clear();
        if (calibReadText(pathJoin(folder, "plane.draft.json"), &text)) {
            std::string perr;
            planeFromJson(JsonValue::parse(text, &perr), &slot.planeDraft);
        }
        text.clear();
        if (calibReadText(pathJoin(folder, "rotate.draft.json"), &text)) {
            std::string perr;
            rotateFromJson(JsonValue::parse(text, &perr), &slot.rotateDraft);
        }
        m_impl->slots[id] = slot;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (err) {
        err->clear();
    }
    return true;
}

bool CalibMgr::save(std::string* err)
{
    if (!m_impl) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    if (m_impl->dir.empty()) {
        if (err) {
            *err = U(L"\u8FD8\u6CA1\u6709\u6253\u5F00\u65B9\u6848\uFF0C\u6807\u5B9A\u65E0\u6CD5\u4FDD\u5B58");
        }
        return false;
    }
    if (!ensureDirectoryTree(m_impl->root())) {
        if (err) {
            *err = U(L"\u65E0\u6CD5\u521B\u5EFA\u6807\u5B9A\u76EE\u5F55");
        }
        return false;
    }
    for (std::map<std::string, Slot>::iterator it = m_impl->slots.begin(); it != m_impl->slots.end(); ++it) {
        const Slot& slot = it->second;
        const std::string folder = m_impl->camDir(it->first);
        if (!ensureDirectoryTree(folder)) {
            if (err) {
                *err = U(L"\u65E0\u6CD5\u521B\u5EFA\u76F8\u673A\u6807\u5B9A\u76EE\u5F55");
            }
            return false;
        }
        if (!calibWriteText(pathJoin(folder, "param.json"),
                            JsonValue::stringify(paramToJson(slot.name, slot.imageWidth, slot.imageHeight, slot.param)))) {
            if (err) {
                *err = U(L"\u5199\u5165\u6807\u5B9A\u53C2\u6570\u5931\u8D25");
            }
            return false;
        }
        if (slot.camera.present) {
            if (!calibWriteText(pathJoin(folder, "camera.json"), JsonValue::stringify(cameraToJson(slot.camera)))) {
                if (err) {
                    *err = U(L"\u5199\u5165\u76F8\u673A\u6A21\u578B\u5931\u8D25");
                }
                return false;
            }
        }
        if (slot.plane.present) {
            if (!calibWriteText(pathJoin(folder, "plane.json"), JsonValue::stringify(planeToJson(slot.plane)))) {
                if (err) {
                    *err = U(L"\u5199\u5165\u5E73\u9762\u6620\u5C04\u5931\u8D25");
                }
                return false;
            }
        }
        if (slot.rotate.present) {
            if (!calibWriteText(pathJoin(folder, "rotate.json"), JsonValue::stringify(rotateToJson(slot.rotate)))) {
                if (err) {
                    *err = U(L"\u5199\u5165\u65CB\u8F6C\u4E2D\u5FC3\u5931\u8D25");
                }
                return false;
            }
        }
        if (slot.cameraDraft.present) {
            calibWriteText(pathJoin(folder, "camera.draft.json"), JsonValue::stringify(cameraToJson(slot.cameraDraft)));
        }
        if (slot.planeDraft.present) {
            calibWriteText(pathJoin(folder, "plane.draft.json"), JsonValue::stringify(planeToJson(slot.planeDraft)));
        }
        if (slot.rotateDraft.present) {
            calibWriteText(pathJoin(folder, "rotate.draft.json"), JsonValue::stringify(rotateToJson(slot.rotateDraft)));
        }
    }
    if (err) {
        err->clear();
    }
    return true;
}

std::vector<ObjectId> CalibMgr::cameras() const
{
    std::vector<ObjectId> ids;
    if (!m_impl) {
        return ids;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    for (std::map<std::string, Slot>::const_iterator it = m_impl->slots.begin(); it != m_impl->slots.end(); ++it) {
        ids.push_back(it->second.id);
    }
    return ids;
}

CalibParam CalibMgr::param(const ObjectId& cameraId) const
{
    if (!m_impl) {
        return CalibParam();
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    const Slot* s = m_impl->find(cameraId.toString());
    return s ? s->param : CalibParam();
}

bool CalibMgr::setParam(const ObjectId& cameraId, const CalibParam& value, std::string* err)
{
    if (!m_impl || !safeId(cameraId.toString())) {
        if (err) {
            *err = U(L"\u76F8\u673A\u6807\u8BC6\u65E0\u6548");
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    Slot& s = m_impl->ensure(cameraId.toString());
    s.id = cameraId;
    s.param = value;
    if (err) {
        err->clear();
    }
    return true;
}

std::string CalibMgr::cameraName(const ObjectId& cameraId) const
{
    if (!m_impl) {
        return std::string();
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    const Slot* s = m_impl->find(cameraId.toString());
    return s ? s->name : std::string();
}

bool CalibMgr::setCameraName(const ObjectId& cameraId, const std::string& name, std::string* err)
{
    if (!m_impl || !safeId(cameraId.toString())) {
        if (err) {
            *err = U(L"\u76F8\u673A\u6807\u8BC6\u65E0\u6548");
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    Slot& s = m_impl->ensure(cameraId.toString());
    s.id = cameraId;
    s.name = name;
    return true;
}

void CalibMgr::imageSize(const ObjectId& cameraId, int* width, int* height) const
{
    int w = 0;
    int h = 0;
    if (m_impl) {
        std::lock_guard<std::mutex> lock(m_impl->mu);
        const Slot* s = m_impl->find(cameraId.toString());
        if (s) {
            w = s->imageWidth;
            h = s->imageHeight;
        }
    }
    if (width) {
        *width = w;
    }
    if (height) {
        *height = h;
    }
}

bool CalibMgr::setImageSize(const ObjectId& cameraId, int width, int height, std::string* err)
{
    if (!m_impl || !safeId(cameraId.toString()) || width < 0 || height < 0) {
        if (err) {
            *err = U(L"\u56FE\u50CF\u5C3A\u5BF8\u65E0\u6548");
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    Slot& s = m_impl->ensure(cameraId.toString());
    s.id = cameraId;
    const bool changed = s.imageWidth > 0 && s.imageHeight > 0
        && (s.imageWidth != width || s.imageHeight != height);
    s.imageWidth = width;
    s.imageHeight = height;
    if (changed) {
        if (s.camera.present && s.camera.status == CalibValid) {
            s.camera.status = CalibExpired;
        }
        if (s.plane.present && s.plane.status == CalibValid) {
            s.plane.status = CalibExpired;
        }
        if (s.rotate.present && s.rotate.status == CalibValid) {
            s.rotate.status = CalibExpired;
        }
    }
    if (err) {
        err->clear();
    }
    return true;
}

CalibRst CalibMgr::active(const ObjectId& cameraId) const
{
    CalibRst r;
    if (!m_impl) {
        return r;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    const Slot* s = m_impl->find(cameraId.toString());
    if (!s) {
        return r;
    }
    r.camera = s->camera;
    r.plane = s->plane;
    r.rotate = s->rotate;
    return r;
}

// 超阈值草稿，不作为生效结果
CalibRst CalibMgr::draft(const ObjectId& cameraId) const
{
    CalibRst r;
    if (!m_impl) {
        return r;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    const Slot* s = m_impl->find(cameraId.toString());
    if (!s) {
        return r;
    }
    r.camera = s->cameraDraft;
    r.plane = s->planeDraft;
    r.rotate = s->rotateDraft;
    return r;
}

// 离线图片目录 calib/<cameraId>/offline
std::string CalibMgr::offlineDir(const ObjectId& cameraId) const
{
    if (!m_impl || m_impl->dir.empty() || !safeId(cameraId.toString())) {
        return std::string();
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return pathJoin(m_impl->camDir(cameraId.toString()), "offline");
}

// 读 Halcon 相机参数文件
std::string CalibMgr::readHalconDat(const ObjectId& cameraId) const
{
    if (!m_impl || !safeId(cameraId.toString())) {
        return std::string();
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::string text;
    calibReadText(pathJoin(m_impl->camDir(cameraId.toString()), "camera.halcon.dat"), &text);
    return text;
}

// 写 Halcon 相机参数文件
bool CalibMgr::writeHalconDat(const ObjectId& cameraId, const std::string& text, std::string* err)
{
    if (!m_impl || !safeId(cameraId.toString()) || m_impl->dir.empty()) {
        if (err) {
            *err = U(L"\u76F8\u673A\u6807\u8BC6\u65E0\u6548");
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    const std::string folder = m_impl->camDir(cameraId.toString());
    if (!ensureDirectoryTree(folder)) {
        if (err) {
            *err = U(L"\u65E0\u6CD5\u521B\u5EFA\u76F8\u673A\u6807\u5B9A\u76EE\u5F55");
        }
        return false;
    }
    if (!calibWriteText(pathJoin(folder, "camera.halcon.dat"), text)) {
        if (err) {
            *err = U(L"\u5199\u5165\u76F8\u673A\u6A21\u578B\u5931\u8D25");
        }
        return false;
    }
    if (err) {
        err->clear();
    }
    return true;
}

std::vector<CalibHistItem> CalibMgr::history(const ObjectId& cameraId, CalibKind kind) const
{
    std::vector<CalibHistItem> items;
    if (!m_impl) {
        return items;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    const std::string folder = pathJoin(m_impl->camDir(cameraId.toString()), "history");
    const std::string prefix = kindFile(kind);
    const std::wstring pattern = utf8ToWide(pathJoin(folder, "*"));
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return items;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        char nameBuf[512];
        WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nameBuf, (int)sizeof(nameBuf), 0, 0);
        const std::string fileName = nameBuf;
        if (fileName.find(prefix) == std::string::npos) {
            continue;
        }
        CalibHistItem item;
        item.kind = kind;
        item.fileName = fileName;
        item.status = CalibExpired;
        std::string text;
        if (calibReadText(pathJoin(folder, fileName), &text)) {
            std::string perr;
            JsonValue j = JsonValue::parse(text, &perr);
            item.updatedAt = j.stringOf("updatedAt");
            item.status = (CalibStatus)j.integerOf("status", (int)CalibExpired);
        }
        items.push_back(item);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return items;
}

namespace {
}

bool CalibMgr::Impl::archive(const std::string& id, CalibKind kind, bool wasValid)
{
    if (!wasValid) {
        return true;
    }
    const std::string folder = camDir(id);
    const std::string src = pathJoin(folder, kindFile(kind));
    if (GetFileAttributesW(utf8ToWide(src).c_str()) == INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    const std::string hist = pathJoin(folder, "history");
    if (!ensureDirectoryTree(hist)) {
        return false;
    }
    const std::string dstName = kindFile(kind) + std::string("-") + nowStamp() + ".json";
    const std::string dst = pathJoin(hist, dstName);
    if (!CopyFileW(utf8ToWide(src).c_str(), utf8ToWide(dst).c_str(), FALSE)) {
        return false;
    }
    std::string text;
    if (!calibReadText(dst, &text)) {
        return true;
    }
    std::string perr;
    JsonValue j = JsonValue::parse(text, &perr);
    if (!j.isObject()) {
        return true;
    }
    j.set("status", JsonValue(static_cast<int>(CalibExpired)));
    return calibWriteText(dst, JsonValue::stringify(j));
}

namespace {

void expirePlaneAndRotate(Slot* s)
{
    if (s->plane.present && s->plane.status == CalibValid) {
        if (!s->camera.present || s->camera.status != CalibValid
            || s->plane.parentModelStamp != s->camera.updatedAt) {
            s->plane.status = CalibExpired;
        }
    }
    if (s->rotate.present && s->rotate.status == CalibValid && s->rotate.worldValid) {
        if (!s->plane.present || s->plane.status != CalibValid
            || s->rotate.parentPlaneStamp != s->plane.updatedAt) {
            s->rotate.status = CalibExpired;
        }
    }
}

}

bool CalibMgr::commitCamera(const ObjectId& cameraId, CamModel model, std::string* err)
{
    if (!m_impl || !safeId(cameraId.toString())) {
        if (err) {
            *err = U(L"\u76F8\u673A\u6807\u8BC6\u65E0\u6548");
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    Slot& s = m_impl->ensure(cameraId.toString());
    s.id = cameraId;
    if (model.updatedAt.empty()) {
        model.updatedAt = nowStamp();
    }
    const std::string halconSrc = pathJoin(m_impl->camDir(cameraId.toString()), "camera.halcon.dat");
    const std::string offlineDir = pathJoin(m_impl->camDir(cameraId.toString()), "offline");
    if (model.rms > s.param.residualMaxPx || model.maxResidual > s.param.residualMaxPx) {
        model.present = true;
        model.status = CalibDraft;
        int64_t rowId = 0;
        if (!CalibDb::instance().recordPlate(m_impl->dir, cameraId.toString(), s.name,
                s.imageWidth, s.imageHeight, s.param, model, halconSrc, offlineDir, &rowId, err)) {
            if (err && err->empty()) {
                *err = U(L"\u6807\u5B9A\u8BB0\u5F55\u6CA1\u6709\u5199\u5165\u6570\u636E\u5E93");
            }
            return false;
        }
        s.cameraDraft = model;
        if (err) {
            *err = U(L"\u91CD\u6295\u5F71\u6B8B\u5DEE\u8D85\u8FC7\u9608\u503C\uFF0C\u672A\u5199\u6210\u751F\u6548");
        }
        return false;
    }
    model.present = true;
    model.status = CalibValid;
    int64_t rowId = 0;
    if (!CalibDb::instance().recordPlate(m_impl->dir, cameraId.toString(), s.name,
            s.imageWidth, s.imageHeight, s.param, model, halconSrc, offlineDir, &rowId, err)) {
        if (err && err->empty()) {
            *err = U(L"\u6807\u5B9A\u8BB0\u5F55\u6CA1\u6709\u5199\u5165\u6570\u636E\u5E93");
        }
        return false;
    }
    const bool was = s.camera.present && s.camera.status == CalibValid;
    if (!m_impl->archive(cameraId.toString(), CalibKindPlate, was)) {
        if (err) {
            *err = U(L"\u65E0\u6CD5\u5F52\u6863\u65E7\u7684\u76F8\u673A\u6A21\u578B");
        }
        return false;
    }
    s.camera = model;
    CalibDb::instance().markApplied("plate", rowId);
    expirePlaneAndRotate(&s);
    if (err) {
        err->clear();
    }
    return true;
}

bool CalibMgr::commitPlane(const ObjectId& cameraId, PlaneMap model, std::string* err)
{
    if (!m_impl || !safeId(cameraId.toString())) {
        if (err) {
            *err = U(L"\u76F8\u673A\u6807\u8BC6\u65E0\u6548");
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    Slot& s = m_impl->ensure(cameraId.toString());
    s.id = cameraId;
    if (model.updatedAt.empty()) {
        model.updatedAt = nowStamp();
    }
    if (model.rms > s.param.residualMaxMm || model.maxResidual > s.param.residualMaxMm) {
        model.present = true;
        model.status = CalibDraft;
        int64_t rowId = 0;
        if (!CalibDb::instance().recordPlane(m_impl->dir, cameraId.toString(), s.name, s.param, model, &rowId, err)) {
            if (err && err->empty()) {
                *err = U(L"\u6807\u5B9A\u8BB0\u5F55\u6CA1\u6709\u5199\u5165\u6570\u636E\u5E93");
            }
            return false;
        }
        s.planeDraft = model;
        if (err) {
            *err = U(L"\u6BEB\u7C73\u6B8B\u5DEE\u8D85\u8FC7\u9608\u503C\uFF0C\u672A\u5199\u6210\u751F\u6548");
        }
        return false;
    }
    if (s.camera.present && s.camera.status == CalibValid) {
        model.parentModelStamp = s.camera.updatedAt;
    }
    model.present = true;
    model.status = CalibValid;
    int64_t rowId = 0;
    if (!CalibDb::instance().recordPlane(m_impl->dir, cameraId.toString(), s.name, s.param, model, &rowId, err)) {
        if (err && err->empty()) {
            *err = U(L"\u6807\u5B9A\u8BB0\u5F55\u6CA1\u6709\u5199\u5165\u6570\u636E\u5E93");
        }
        return false;
    }
    const bool was = s.plane.present && s.plane.status == CalibValid;
    if (!m_impl->archive(cameraId.toString(), CalibKindPlane, was)) {
        if (err) {
            *err = U(L"\u65E0\u6CD5\u5F52\u6863\u65E7\u7684\u5E73\u9762\u6620\u5C04");
        }
        return false;
    }
    s.plane = model;
    CalibDb::instance().markApplied("plane", rowId);
    expirePlaneAndRotate(&s);
    if (err) {
        err->clear();
    }
    return true;
}

bool CalibMgr::commitRotate(const ObjectId& cameraId, RotateCenter model, std::string* err)
{
    if (!m_impl || !safeId(cameraId.toString())) {
        if (err) {
            *err = U(L"\u76F8\u673A\u6807\u8BC6\u65E0\u6548");
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    Slot& s = m_impl->ensure(cameraId.toString());
    s.id = cameraId;
    if (model.updatedAt.empty()) {
        model.updatedAt = nowStamp();
    }
    if (model.rms > s.param.residualMaxPx || model.maxResidual > s.param.residualMaxPx || model.radius < 1.0) {
        model.present = true;
        model.status = CalibDraft;
        int64_t rowId = 0;
        if (!CalibDb::instance().recordRotate(m_impl->dir, cameraId.toString(), s.name, s.param, model, &rowId, err)) {
            if (err && err->empty()) {
                *err = U(L"\u6807\u5B9A\u8BB0\u5F55\u6CA1\u6709\u5199\u5165\u6570\u636E\u5E93");
            }
            return false;
        }
        s.rotateDraft = model;
        if (err) {
            *err = model.radius < 1.0
                ? U(L"\u65CB\u8F6C\u534A\u5F84\u8FC7\u5C0F\uFF0C\u672A\u5199\u6210\u751F\u6548")
                : U(L"\u65CB\u8F6C\u6B8B\u5DEE\u8D85\u8FC7\u9608\u503C\uFF0C\u672A\u5199\u6210\u751F\u6548");
        }
        return false;
    }
    model.worldValid = false;
    if (s.plane.present && s.plane.status == CalibValid) {
        std::vector<RotateSample> worldSamples;
        worldSamples.reserve(model.samples.size());
        for (size_t i = 0; i < model.samples.size(); ++i) {
            double wx = 0.0;
            double wy = 0.0;
            if (!mapImageToWorld(s.plane.imageToWorld, model.samples[i].imageX, model.samples[i].imageY, &wx, &wy)) {
                worldSamples.clear();
                break;
            }
            RotateSample ws;
            ws.angleDeg = model.samples[i].angleDeg;
            ws.imageX = wx;
            ws.imageY = wy;
            worldSamples.push_back(ws);
        }
        RotateCenter worldFit;
        if (worldSamples.size() >= 3 && fitRotateCenter(worldSamples, &worldFit, 0)) {
            model.worldX = worldFit.imageX;
            model.worldY = worldFit.imageY;
            model.worldValid = true;
            model.parentPlaneStamp = s.plane.updatedAt;
        }
    }
    model.present = true;
    model.status = CalibValid;
    if (model.updatedAt.empty()) {
        model.updatedAt = nowStamp();
    }
    int64_t rowId = 0;
    if (!CalibDb::instance().recordRotate(m_impl->dir, cameraId.toString(), s.name, s.param, model, &rowId, err)) {
        if (err && err->empty()) {
            *err = U(L"\u6807\u5B9A\u8BB0\u5F55\u6CA1\u6709\u5199\u5165\u6570\u636E\u5E93");
        }
        return false;
    }
    const bool was = s.rotate.present && s.rotate.status == CalibValid;
    if (!m_impl->archive(cameraId.toString(), CalibKindRotate, was)) {
        if (err) {
            *err = U(L"\u65E0\u6CD5\u5F52\u6863\u65E7\u7684\u65CB\u8F6C\u4E2D\u5FC3");
        }
        return false;
    }
    s.rotate = model;
    CalibDb::instance().markApplied("rotate", rowId);
    if (err) {
        err->clear();
    }
    return true;
}

bool CalibMgr::importPointTable(const ObjectId& cameraId, const std::string& csvPath, std::string* err)
{
    if (!safeId(cameraId.toString())) {
        if (err) {
            *err = U(L"\u76F8\u673A\u6807\u8BC6\u65E0\u6548");
        }
        return false;
    }
    std::string text;
    if (!calibReadText(csvPath, &text)) {
        if (err) {
            *err = U(L"\u65E0\u6CD5\u8BFB\u53D6\u70B9\u8868");
        }
        return false;
    }
    CalibParam fitParam = param(cameraId);
    std::vector<PlaneSample> samples;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::vector<double> nums;
        if (!readDoubles(line, &nums) || nums.size() < 4) {
            continue;
        }
        PlaneSample sample;
        sample.imageX = nums[0];
        sample.imageY = nums[1];
        sample.worldX = nums[2];
        sample.worldY = nums[3];
        sample.used = true;
        samples.push_back(sample);
    }
    PlaneMap map;
    if (!fitPlaneMap(fitParam.planeFit, samples, &map, err)) {
        return false;
    }
    map.source = CalibOfflineTable;
    return commitPlane(cameraId, map, err);
}

bool CalibMgr::importResult(const ObjectId& cameraId, const std::string& filePath, std::string* err)
{
    if (!safeId(cameraId.toString())) {
        if (err) {
            *err = U(L"\u76F8\u673A\u6807\u8BC6\u65E0\u6548");
        }
        return false;
    }
    std::string text;
    if (!calibReadText(filePath, &text)) {
        if (err) {
            *err = U(L"\u65E0\u6CD5\u8BFB\u53D6\u7ED3\u679C\u6587\u4EF6");
        }
        return false;
    }
    std::string perr;
    JsonValue j = JsonValue::parse(text, &perr);
    if (!j.isObject()) {
        if (err) {
            *err = U(L"\u7ED3\u679C\u6587\u4EF6\u4E0D\u662F\u6807\u5B9A") + " json";
        }
        return false;
    }
    if (j.contains("fx")) {
        CamModel model;
        if (!cameraFromJson(j, &model)) {
            if (err) {
                *err = U(L"\u76F8\u673A\u6A21\u578B\u4E0D\u5B8C\u6574");
            }
            return false;
        }
        model.source = CalibImport;
        model.updatedAt.clear();
        return commitCamera(cameraId, model, err);
    }
    if (j.contains("imageToWorld")) {
        PlaneMap model;
        if (!planeFromJson(j, &model)) {
            if (err) {
                *err = U(L"\u5E73\u9762\u6620\u5C04\u4E0D\u5B8C\u6574");
            }
            return false;
        }
        model.source = CalibImport;
        model.updatedAt.clear();
        return commitPlane(cameraId, model, err);
    }
    if (j.contains("radius")) {
        RotateCenter model;
        if (!rotateFromJson(j, &model)) {
            if (err) {
                *err = U(L"\u65CB\u8F6C\u4E2D\u5FC3\u4E0D\u5B8C\u6574");
            }
            return false;
        }
        model.source = CalibImport;
        model.updatedAt.clear();
        return commitRotate(cameraId, model, err);
    }
    if (err) {
        *err = U(L"\u8BA4\u4E0D\u51FA\u8FD9\u662F\u54EA\u4E00\u79CD\u6807\u5B9A\u7ED3\u679C");
    }
    return false;
}

bool CalibMgr::bindCamera(const ObjectId& fromId, const ObjectId& deviceId, std::string* err)
{
    if (!m_impl || !safeId(fromId.toString()) || !safeId(deviceId.toString())) {
        if (err) {
            *err = U(L"\u76F8\u673A\u6807\u8BC6\u65E0\u6548");
        }
        return false;
    }
    if (fromId == deviceId) {
        return true;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    Slot* src = m_impl->find(fromId.toString());
    if (!src) {
        if (err) {
            *err = U(L"\u627E\u4E0D\u5230\u8981\u7ED1\u5B9A\u7684\u6807\u5B9A");
        }
        return false;
    }
    if (m_impl->find(deviceId.toString())) {
        if (err) {
            *err = U(L"\u76EE\u6807\u76F8\u673A\u5DF2\u7ECF\u6709\u6807\u5B9A");
        }
        return false;
    }
    if (!m_impl->dir.empty()) {
        const std::string oldDir = m_impl->camDir(fromId.toString());
        const std::string newDir = m_impl->camDir(deviceId.toString());
        if (GetFileAttributesW(utf8ToWide(oldDir).c_str()) != INVALID_FILE_ATTRIBUTES) {
            if (!MoveFileW(utf8ToWide(oldDir).c_str(), utf8ToWide(newDir).c_str())) {
                if (err) {
                    *err = U(L"\u6807\u5B9A\u76EE\u5F55\u6539\u540D\u5931\u8D25");
                }
                return false;
            }
        }
    }
    Slot moved = *src;
    moved.id = deviceId;
    m_impl->slots.erase(fromId.toString());
    m_impl->slots[deviceId.toString()] = moved;
    if (err) {
        err->clear();
    }
    return true;
}

bool CalibMgr::mapPose(const ObjectId& cameraId, const CalibPose& in, CalibPose* out, std::string* err) const
{
    if (!out) {
        if (err) {
            *err = U(L"\u6CA1\u6709\u8F93\u51FA");
        }
        return false;
    }
    *out = in;
    if (!m_impl) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    const Slot* s = m_impl->find(cameraId.toString());
    if (!s || !s->plane.present || s->plane.status != CalibValid) {
        if (err) {
            *err = U(L"\u8FD9\u8DEF\u76F8\u673A\u6CA1\u6709\u751F\u6548\u7684\u5E73\u9762\u6620\u5C04");
        }
        return false;
    }
    double x = 0.0;
    double y = 0.0;
    if (!mapImageToWorld(s->plane.imageToWorld, in.x, in.y, &x, &y)) {
        if (err) {
            *err = U(L"\u5E73\u9762\u6620\u5C04\u5931\u8D25");
        }
        return false;
    }
    double angle = in.angle;
    if (s->rotate.present && s->rotate.status == CalibValid && s->rotate.worldValid) {
        const double delta = in.angle - s->param.angleRefDeg;
        rotatePoint(s->rotate.worldX, s->rotate.worldY, delta, x, y, &x, &y);
        angle = delta;
    }
    out->x = x;
    out->y = y;
    out->angle = angle;
    if (err) {
        err->clear();
    }
    return true;
}

}
