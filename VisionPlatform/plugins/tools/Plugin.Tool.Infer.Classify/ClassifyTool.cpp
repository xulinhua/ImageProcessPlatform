/**
 * @file ClassifyTool.cpp
 * @brief 图像分类工具壳
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ClassifyTool.h"
#include "AlgToolBase.h"
#include "TextEncoding.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <windows.h>
#include <stdio.h>

namespace Vision {

namespace {

const char kToolVersion[] = "1.0.0.0";
const VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "图像分类" },
};

}

std::string ClassifyTool::toolName() const { return "infer.classify"; }
ToolType ClassifyTool::type() const { return ToolType::Infer; }
std::string ClassifyTool::displayName() const { return "图像分类"; }
std::string ClassifyTool::version() const { return std::string(kToolVersion); }

std::vector<VerNote> ClassifyTool::versionNotes() const
{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string ClassifyTool::description() const
{
    return "整图一个类别和分数";
}

std::vector<ParamField> ClassifyTool::learnFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramString("model", "模型", std::string("model.onnx"), std::vector<std::string>()));
    f.push_back(paramString("labels", "类别文件", std::string("labels.txt"), std::vector<std::string>()));
    return f;
}

std::vector<ParamField> ClassifyTool::runFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramInt("width", "宽", 224, true, 1, false, 0));
    f.push_back(paramInt("height", "高", 224, true, 1, false, 0));
    f.push_back(paramDouble("threshold", "分数阈值", 0.5, true, 0.0, true, 1.0));
    return f;
}

std::vector<ParamField> ClassifyTool::cfg_teach() const { return learnFields(); }

std::vector<ParamField> ClassifyTool::paramFields() const
{
    std::vector<ParamField> f = learnFields();
    const std::vector<ParamField> run = runFields();
    f.insert(f.end(), run.begin(), run.end());
    return f;
}

ParamMap ClassifyTool::defaults() const
{
    ParamMap m;
    const std::vector<ParamField> f = paramFields();
    for (size_t i = 0; i < f.size(); ++i) {
        m.insert(f[i].key, f[i].def);
    }
    return m;
}

ParamMap ClassifyTool::defaultParams() const { return defaults(); }

std::vector<PortDesc> ClassifyTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Image));
    PortDesc classIndex(PortDir::Out, PortData::Tuple);
    classIndex.slot.name = "classIndex";
    PortDesc score(PortDir::Out, PortData::Tuple);
    score.slot.name = "score";
    PortDesc className(PortDir::Out, PortData::Tuple);
    className.slot.name = "className";
    p.push_back(classIndex);
    p.push_back(score);
    p.push_back(className);
    p.push_back(PortDesc(PortDir::Out, PortData::Logic));
    return p;
}

std::string ClassifyTool::textOf(const ToolIn& in, const char* key) const
{
    if (in.param.learn.contains(key)) {
        return in.param.learn.value(key).toString();
    }
    return in.param.run.value(key).toString();
}

std::string ClassifyTool::joinToolFile(const ToolIn& in, const std::string& fileName) const
{
    std::string dir = in.toolDataDir;
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
        dir.push_back('\\');
    }
    return dir + fileName;
}

bool ClassifyTool::fileExists(const std::string& path) const
{
    const std::wstring wide = utf8ToWide(path);
    const DWORD attr = GetFileAttributesW(wide.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool ClassifyTool::filesReady(const ToolIn& in, std::string* err) const
{
    if (in.toolDataDir.empty()) {
        if (err) {
            *err = "请先保存方案";
        }
        return false;
    }
    const char* keys[2] = { "model", "labels" };
    for (int i = 0; i < 2; ++i) {
        const std::string name = textOf(in, keys[i]);
        if (name.empty() || !fileExists(joinToolFile(in, name))) {
            if (err) {
                *err = std::string("未加载模型 ") + name;
            }
            return false;
        }
    }
    return true;
}

bool ClassifyTool::copyIntoToolDir(const std::string& srcPath, const std::string& toolDataDir,
                                   const std::string& fileName, std::string* err) const
{
    if (toolDataDir.empty()) {
        if (err) {
            *err = "请先保存方案";
        }
        return false;
    }
    std::string dir = toolDataDir;
    if (dir.back() != '/' && dir.back() != '\\') {
        dir.push_back('\\');
    }
    const std::wstring src = utf8ToWide(srcPath);
    const std::wstring dst = utf8ToWide(dir + fileName);
    if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
        if (err) {
            *err = "拷贝模型失败";
        }
        return false;
    }
    return true;
}

void ClassifyTool::writeLabel(ToolOut* out) const
{
    if (!out || !out->labels.empty() || !out->values.contains("className")) {
        return;
    }
    ResultLabel lab;
    lab.x = 8;
    lab.y = 8;
    lab.text = out->values.value("className").toString();
    if (out->values.contains("score")) {
        char buf[64];
        sprintf_s(buf, " %.3f", out->values.value("score").toDouble(0));
        lab.text += buf;
    }
    out->labels.push_back(lab);
}

ToolOut ClassifyTool::execute(const ToolIn& in)
{
    std::string err;
    if (!filesReady(in, &err)) {
        ToolOut out;
        out.ok = false;
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = err;
        return out;
    }
    ToolIn shot = in;
    shot.algLib = AlgLibOnnx;
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) {
        out.message = "未登记推理算法";
    }
    writeLabel(&out);
    out.image = in.input;
    return out;
}

ToolBase* ClassifyTool::clone() const
{
    ClassifyTool* p = new ClassifyTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

}
