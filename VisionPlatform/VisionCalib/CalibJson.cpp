/**
 * @file CalibJson.cpp
 * @brief CalibJson实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CalibJson.h"
#include "SolutionDataPaths.h"
#include "TextEncoding.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

namespace Vision {

namespace {

// 9 个数写成 JSON 数组
JsonValue arr9(const double* m)
{
    JsonValue j;
    j.makeArray();
    for (int i = 0; i < 9; ++i) {
        j.append(JsonValue(m ? m[i] : 0.0));
    }
    return j;
}

// 从对象字段读 9 个数；长度不对返回 false
bool read9(const JsonValue& j, const std::string& key, double* m)
{
    const JsonValue a = j.value(key);
    if (!m || !a.isArray() || a.size() != 9) {
        return false;
    }
    for (int i = 0; i < 9; ++i) {
        m[i] = a.at(i).toDouble(0.0);
    }
    return true;
}

}

// 读整个文本文件
bool calibReadText(const std::string& path, std::string* text)
{
    if (!text || path.empty()) {
        return false;
    }
    FILE* f = nullptr;
    const std::wstring w = utf8ToWide(path);
    if (_wfopen_s(&f, w.c_str(), L"rb") != 0 || !f) {
        return false;
    }
    text->clear();
    char buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        text->append(buf, n);
    }
    fclose(f);
    return true;
}

// 写文本文件；先保证父目录存在
bool calibWriteText(const std::string& path, const std::string& text)
{
    if (path.empty()) {
        return false;
    }
    const std::string dir = pathDirName(path);
    if (!dir.empty() && !ensureDirectoryTree(dir)) {
        return false;
    }
    FILE* f = nullptr;
    const std::wstring w = utf8ToWide(path);
    if (_wfopen_s(&f, w.c_str(), L"wb") != 0 || !f) {
        return false;
    }
    const size_t n = fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return n == text.size();
}

// 标定参数写成 JSON 对象
JsonValue paramToJson(const std::string& name, int imageWidth, int imageHeight, const CalibParam& p)
{
    JsonValue j;
    j.makeObject();
    j.set("name", JsonValue(name));
    j.set("imageWidth", JsonValue(imageWidth));
    j.set("imageHeight", JsonValue(imageHeight));
    j.set("plateKind", JsonValue(static_cast<int>(p.plateKind)));
    j.set("rows", JsonValue(p.rows));
    j.set("cols", JsonValue(p.cols));
    j.set("spacingMm", JsonValue(p.spacingMm));
    j.set("thicknessMm", JsonValue(p.thicknessMm));
    j.set("planeFit", JsonValue(static_cast<int>(p.planeFit)));
    j.set("residualMaxPx", JsonValue(p.residualMaxPx));
    j.set("residualMaxMm", JsonValue(p.residualMaxMm));
    j.set("angleRefDeg", JsonValue(p.angleRefDeg));
    j.set("minPlateImages", JsonValue(p.minPlateImages));
    j.set("minRotateAngles", JsonValue(p.minRotateAngles));
    return j;
}

// 从 JSON 对象填标定参数
bool paramFromJson(const JsonValue& j, std::string* name, int* imageWidth, int* imageHeight, CalibParam* p)
{
    if (!p || !j.isObject()) {
        return false;
    }
    if (name) {
        *name = j.stringOf("name");
    }
    if (imageWidth) {
        *imageWidth = j.integerOf("imageWidth", 0);
    }
    if (imageHeight) {
        *imageHeight = j.integerOf("imageHeight", 0);
    }
    p->plateKind = static_cast<CalibPlateKind>(j.integerOf("plateKind", 0));
    p->rows = j.integerOf("rows", 0);
    p->cols = j.integerOf("cols", 0);
    p->spacingMm = j.number("spacingMm", 1.0);
    p->thicknessMm = j.number("thicknessMm", 0.0);
    p->planeFit = static_cast<CalibPlaneFit>(j.integerOf("planeFit", static_cast<int>(CalibFitAffine)));
    p->residualMaxPx = j.number("residualMaxPx", 0.5);
    p->residualMaxMm = j.number("residualMaxMm", 0.5);
    p->angleRefDeg = j.number("angleRefDeg", 0.0);
    p->minPlateImages = j.integerOf("minPlateImages", 10);
    p->minRotateAngles = j.integerOf("minRotateAngles", 3);
    return true;
}

// 相机模型写成 JSON 对象
JsonValue cameraToJson(const CamModel& m)
{
    JsonValue j;
    j.makeObject();
    j.set("status", JsonValue(static_cast<int>(m.status)));
    j.set("source", JsonValue(static_cast<int>(m.source)));
    j.set("rms", JsonValue(m.rms));
    j.set("maxResidual", JsonValue(m.maxResidual));
    j.set("updatedAt", JsonValue(m.updatedAt));
    j.set("fx", JsonValue(m.fx));
    j.set("fy", JsonValue(m.fy));
    j.set("cx", JsonValue(m.cx));
    j.set("cy", JsonValue(m.cy));
    j.set("k1", JsonValue(m.k1));
    j.set("k2", JsonValue(m.k2));
    j.set("k3", JsonValue(m.k3));
    j.set("p1", JsonValue(m.p1));
    j.set("p2", JsonValue(m.p2));
    j.set("plateKind", JsonValue(static_cast<int>(m.plateKind)));
    j.set("rows", JsonValue(m.rows));
    j.set("cols", JsonValue(m.cols));
    j.set("spacingMm", JsonValue(m.spacingMm));
    j.set("count", JsonValue(m.count));
    return j;
}

// 从 JSON 对象填相机模型；缺 fx 视为不是相机模型
bool cameraFromJson(const JsonValue& j, CamModel* m)
{
    if (!m || !j.isObject() || !j.contains("fx")) {
        return false;
    }
    *m = CamModel();
    m->present = true;
    m->status = static_cast<CalibStatus>(j.integerOf("status", static_cast<int>(CalibValid)));
    m->source = static_cast<CalibSource>(j.integerOf("source", static_cast<int>(CalibImport)));
    m->rms = j.number("rms", 0.0);
    m->maxResidual = j.number("maxResidual", 0.0);
    m->updatedAt = j.stringOf("updatedAt");
    m->fx = j.number("fx", 0.0);
    m->fy = j.number("fy", 0.0);
    m->cx = j.number("cx", 0.0);
    m->cy = j.number("cy", 0.0);
    m->k1 = j.number("k1", 0.0);
    m->k2 = j.number("k2", 0.0);
    m->k3 = j.number("k3", 0.0);
    m->p1 = j.number("p1", 0.0);
    m->p2 = j.number("p2", 0.0);
    m->plateKind = static_cast<CalibPlateKind>(j.integerOf("plateKind", 0));
    m->rows = j.integerOf("rows", 0);
    m->cols = j.integerOf("cols", 0);
    m->spacingMm = j.number("spacingMm", 0.0);
    m->count = j.integerOf("count", 0);
    return true;
}

// 平面映射写成 JSON 对象
JsonValue planeToJson(const PlaneMap& m)
{
    JsonValue j;
    j.makeObject();
    j.set("status", JsonValue(static_cast<int>(m.status)));
    j.set("source", JsonValue(static_cast<int>(m.source)));
    j.set("rms", JsonValue(m.rms));
    j.set("maxResidual", JsonValue(m.maxResidual));
    j.set("updatedAt", JsonValue(m.updatedAt));
    j.set("fit", JsonValue(static_cast<int>(m.fit)));
    j.set("imageToWorld", arr9(m.imageToWorld));
    j.set("worldToImage", arr9(m.worldToImage));
    j.set("mmPerPixel", JsonValue(m.mmPerPixel));
    j.set("parentModelStamp", JsonValue(m.parentModelStamp));
    JsonValue arr;
    arr.makeArray();
    for (size_t i = 0; i < m.samples.size(); ++i) {
        JsonValue s;
        s.makeObject();
        s.set("imageX", JsonValue(m.samples[i].imageX));
        s.set("imageY", JsonValue(m.samples[i].imageY));
        s.set("worldX", JsonValue(m.samples[i].worldX));
        s.set("worldY", JsonValue(m.samples[i].worldY));
        s.set("used", JsonValue(m.samples[i].used));
        arr.append(s);
    }
    j.set("samples", arr);
    return j;
}

// 从 JSON 对象填平面映射；两个 3x3 矩阵都必须在
bool planeFromJson(const JsonValue& j, PlaneMap* m)
{
    if (!m || !j.isObject() || !j.contains("imageToWorld")) {
        return false;
    }
    *m = PlaneMap();
    m->present = true;
    m->status = static_cast<CalibStatus>(j.integerOf("status", static_cast<int>(CalibValid)));
    m->source = static_cast<CalibSource>(j.integerOf("source", static_cast<int>(CalibImport)));
    m->rms = j.number("rms", 0.0);
    m->maxResidual = j.number("maxResidual", 0.0);
    m->updatedAt = j.stringOf("updatedAt");
    m->fit = static_cast<CalibPlaneFit>(j.integerOf("fit", static_cast<int>(CalibFitAffine)));
    if (!read9(j, "imageToWorld", m->imageToWorld)) {
        return false;
    }
    if (!read9(j, "worldToImage", m->worldToImage)) {
        return false;
    }
    m->mmPerPixel = j.number("mmPerPixel", 0.0);
    m->parentModelStamp = j.stringOf("parentModelStamp");
    const JsonValue arr = j.value("samples");
    if (arr.isArray()) {
        for (int i = 0; i < arr.size(); ++i) {
            const JsonValue s = arr.at(i);
            if (!s.isObject()) {
                continue;
            }
            PlaneSample sample;
            sample.imageX = s.number("imageX", 0.0);
            sample.imageY = s.number("imageY", 0.0);
            sample.worldX = s.number("worldX", 0.0);
            sample.worldY = s.number("worldY", 0.0);
            sample.used = s.flagOf("used", true);
            m->samples.push_back(sample);
        }
    }
    return true;
}

// 旋转中心写成 JSON 对象
JsonValue rotateToJson(const RotateCenter& m)
{
    JsonValue j;
    j.makeObject();
    j.set("status", JsonValue(static_cast<int>(m.status)));
    j.set("source", JsonValue(static_cast<int>(m.source)));
    j.set("rms", JsonValue(m.rms));
    j.set("maxResidual", JsonValue(m.maxResidual));
    j.set("updatedAt", JsonValue(m.updatedAt));
    j.set("imageX", JsonValue(m.imageX));
    j.set("imageY", JsonValue(m.imageY));
    j.set("worldX", JsonValue(m.worldX));
    j.set("worldY", JsonValue(m.worldY));
    j.set("worldValid", JsonValue(m.worldValid));
    j.set("radius", JsonValue(m.radius));
    j.set("parentPlaneStamp", JsonValue(m.parentPlaneStamp));
    JsonValue arr;
    arr.makeArray();
    for (size_t i = 0; i < m.samples.size(); ++i) {
        JsonValue s;
        s.makeObject();
        s.set("angleDeg", JsonValue(m.samples[i].angleDeg));
        s.set("imageX", JsonValue(m.samples[i].imageX));
        s.set("imageY", JsonValue(m.samples[i].imageY));
        arr.append(s);
    }
    j.set("samples", arr);
    return j;
}

// 从 JSON 对象填旋转中心；缺半径视为不是旋转中心
bool rotateFromJson(const JsonValue& j, RotateCenter* m)
{
    if (!m || !j.isObject() || !j.contains("radius")) {
        return false;
    }
    *m = RotateCenter();
    m->present = true;
    m->status = static_cast<CalibStatus>(j.integerOf("status", static_cast<int>(CalibValid)));
    m->source = static_cast<CalibSource>(j.integerOf("source", static_cast<int>(CalibImport)));
    m->rms = j.number("rms", 0.0);
    m->maxResidual = j.number("maxResidual", 0.0);
    m->updatedAt = j.stringOf("updatedAt");
    m->imageX = j.number("imageX", 0.0);
    m->imageY = j.number("imageY", 0.0);
    m->worldX = j.number("worldX", 0.0);
    m->worldY = j.number("worldY", 0.0);
    m->worldValid = j.flagOf("worldValid", false);
    m->radius = j.number("radius", 0.0);
    m->parentPlaneStamp = j.stringOf("parentPlaneStamp");
    const JsonValue arr = j.value("samples");
    if (arr.isArray()) {
        for (int i = 0; i < arr.size(); ++i) {
            const JsonValue s = arr.at(i);
            if (!s.isObject()) {
                continue;
            }
            RotateSample sample;
            sample.angleDeg = s.number("angleDeg", 0.0);
            sample.imageX = s.number("imageX", 0.0);
            sample.imageY = s.number("imageY", 0.0);
            m->samples.push_back(sample);
        }
    }
    return true;
}

}