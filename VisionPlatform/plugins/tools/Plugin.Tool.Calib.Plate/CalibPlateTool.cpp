/**
 * @file CalibPlateTool.cpp
 * @brief CalibPlateTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CalibPlateTool.h"
#include "AlgToolBase.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include "CalibMath.h"
#include "CalibMgr.h"
#include "CvImageConvert.h"
#include "SolutionDataPaths.h"
#include "TextEncoding.h"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
// 当前版本。必须等于变更表最后一行
const char kToolVersion[] = "1.0.0.0";
// 变更表。只追加，不删旧行
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};

const double kPi = 3.14159265358979323846;

// 宽字符转 UTF-8
std::string U(const wchar_t* w)
{
    std::string o;
    if (!w) {
        return o;
    }
    for (; *w; ++w) {
        unsigned int c = static_cast<unsigned int>(*w);
        if (c < 0x80u) {
            o.push_back(static_cast<char>(c));
        } else if (c < 0x800u) {
            o.push_back(static_cast<char>(0xC0u | (c >> 6)));
            o.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
        } else {
            o.push_back(static_cast<char>(0xE0u | (c >> 12)));
            o.push_back(static_cast<char>(0x80u | ((c >> 6) & 0x3Fu)));
            o.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
        }
    }
    return o;
}

// 标定工具不画搜索区
Vision::ToolCaps noRoi()
{
    Vision::ToolCaps c;
    c.usesRoi = false;
    return c;
}

// 用参数说明填默认值
Vision::ParamMap defaultsFrom(const std::vector<Vision::ParamField>& fields)
{
    Vision::ParamMap m;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].type == Vision::ParamFieldType::Device) {
            continue;
        }
        m.insert(fields[i].key, fields[i].def);
    }
    return m;
}

// 读文本行，跳过空行和井号
bool readLines(const std::string& path, std::vector<std::string>* lines, std::string* err)
{
    std::ifstream in(path.c_str());
    if (!in) {
        if (err) {
            *err = U(L"\u65e0\u6cd5\u8bfb\u53d6\u6587\u4ef6");
        }
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        lines->push_back(line);
    }
    return true;
}

// 按逗号、分号或空白拆成数字
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

// 工具上绑定的相机
Vision::ObjectId cameraOf(const Vision::ToolIn& in)
{
    return in.param.deviceId;
}

// 没绑相机则失败
Vision::ToolOut needCamera(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    if (cameraOf(in).isNull()) {
        out.ok = false;
        out.message = U(L"\u8bf7\u5728\u5de5\u5177\u4e0a\u7ed1\u5b9a\u76f8\u673a");
        return out;
    }
    out.ok = true;
    return out;
}

void fillBoardRun(const Vision::ObjectId& cam, Vision::ParamMap* run)
{
    const Vision::CalibParam p = Vision::CalibMgr::instance().param(cam);
    run->insert("rows", p.rows);
    run->insert("cols", p.cols);
    run->insert("spacingMm", p.spacingMm);
    run->insert("plateKind", static_cast<int>(p.plateKind));
    run->insert("minImages", p.minPlateImages);
    if (run->value("imageDir").toString().empty()) {
        run->insert("imageDir", Vision::CalibMgr::instance().offlineDir(cam));
    }
}

Vision::AlgToolOut runNamed(const char* toolName, Vision::AlgLibKind prefer, const Vision::AlgToolIn& in, std::string* err)
{
    Vision::AlgToolBase* proto = Vision::AlgToolBaseRegistry::instance().getPreferred(toolName, prefer);
    if (!proto) {
        if (err) {
            *err = U(L"\u6ca1\u6709\u767b\u8bb0\u8fd9\u4e2a\u7b97\u6cd5");
        }
        return Vision::AlgToolOut();
    }
    Vision::AlgToolOut out;
    proto->run(Vision::ExecMode::Auto, in, &out);
    return out;
}

Vision::CamModel modelFrom(const Vision::ParamMap& v)
{
    Vision::CamModel m;
    m.fx = v.value("fx").toDouble(0);
    m.fy = v.value("fy").toDouble(0);
    m.cx = v.value("cx").toDouble(0);
    m.cy = v.value("cy").toDouble(0);
    m.k1 = v.value("k1").toDouble(0);
    m.k2 = v.value("k2").toDouble(0);
    m.k3 = v.value("k3").toDouble(0);
    m.p1 = v.value("p1").toDouble(0);
    m.p2 = v.value("p2").toDouble(0);
    m.rms = v.value("rms").toDouble(0);
    m.maxResidual = v.value("maxResidual").toDouble(0);
    m.count = v.value("count").toInt(0);
    m.source = Vision::CalibOfflineImage;
    return m;
}

}

// 工具版本
std::string Vision::CalibPlateTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::CalibPlateTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

// 注册名
std::string Vision::CalibPlateTool::tool_name() const { return "calib.plate"; }
// 类型为标定
Vision::ToolType Vision::CalibPlateTool::type() const { return Vision::ToolType::Calib; }
// 界面显示名
std::string Vision::CalibPlateTool::display_name() const { return U(L"\u6807\u5b9a\u677f"); }
// 工具说明
std::string Vision::CalibPlateTool::description() const
{
    return U(L"\u5b66\u4e60\u65f6\u5bfc\u5165\u5df2\u7b97\u597d\u7684\u76f8\u673a\u6a21\u578b json");
}
// 默认参数
Vision::ParamMap Vision::CalibPlateTool::default_params() const { return defaultsFrom(cfg_param()); }
std::vector<Vision::ParamField> Vision::CalibPlateTool::cfg_param() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDevice("deviceId", "相机"));
    f.push_back(Vision::paramPath("resultPath", "结果文件", std::string()));
    f.push_back(Vision::paramPath("imageDir", "imageDir", std::string()));
    return f;
}
// 不使用搜索区
Vision::ToolCaps Vision::CalibPlateTool::cfg_roi() const { return noRoi(); }
// 学习工具无端口
std::vector<Vision::PortDesc> Vision::CalibPlateTool::cfg_port() const
{
    return std::vector<Vision::PortDesc>();
}
// 学习时导入相机模型
int Vision::CalibPlateTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = needCamera(in);
    if (!out->ok) {
        return out->errCode;
    }
    if (in.mode != Vision::RunMode::Learn) {
        out->ok = true;
        out->message = U(L"\u81ea\u52a8\u8fd0\u884c\u4e0d\u6539\u6807\u5b9a");
        out->syncIo();
        return out->errCode;
    }
    const std::string path = in.param.run.value("resultPath").toString();
    std::string err;
    // 给了结果文件：直接导入相机模型（不经算法 shot）
    if (!path.empty()) {
        if (!Vision::CalibMgr::instance().importResult(cameraOf(in), path, &err)
            || !Vision::CalibMgr::instance().save(&err)) {
            out->ok = false;
            out->message = err;
            return out->errCode;
        }
        out->ok = true;
        out->message = U(L"\u76f8\u673a\u6a21\u578b\u5df2\u5bfc\u5165");
        out->syncIo();
        return out->errCode;
    }
    return shot_learn(in, out);
}

void Vision::CalibPlateTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    ain->mode = in.mode;
    ain->run = in.param.run;
    ain->solutionDir = in.solutionDir;
    ain->param = &param_;
    fillBoardRun(cameraOf(in), &ain->run);
}

int Vision::CalibPlateTool::shot_learn(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    attachAlg(in.toolDataDir);
    param_.load(in.param);
    if (param_.loadCode() != HS_OK) {
        out->ok = false;
        out->errCode = param_.loadCode();
        out->message = HsCommData::HsException(param_.loadCode()).errorMessage();
        return out->errCode;
    }
    // Halcon 标定板需指定 Halcon 库；attachAlg 无 prefer，此处仍走 runNamed
    Vision::AlgToolIn ain;
    fill_alg_in(in, &ain);
    const Vision::CalibParam board = Vision::CalibMgr::instance().param(cameraOf(in));
    Vision::AlgLibKind prefer = in.algLib;
    if (board.plateKind == Vision::CalibPlateHalconCaltab) {
        prefer = Vision::AlgLibHal;
    }
    std::string err;
    const Vision::AlgToolOut aout = runNamed("calib.plate", prefer, ain, &err);
    if (!aout.ok) {
        out->ok = false;
        out->message = aout.message.empty() ? err : aout.message;
        return out->errCode;
    }
    Vision::CamModel model = modelFrom(aout.values);
    model.plateKind = board.plateKind;
    model.rows = board.rows;
    model.cols = board.cols;
    model.spacingMm = board.spacingMm;
    const int iw = aout.values.value("width").toInt(0);
    const int ih = aout.values.value("height").toInt(0);
    if (iw > 0 && ih > 0) {
        Vision::CalibMgr::instance().setImageSize(cameraOf(in), iw, ih, &err);
    }
    const std::string halconCam = aout.values.value("halconCam").toString();
    if (!halconCam.empty()) {
        Vision::CalibMgr::instance().writeHalconDat(cameraOf(in), halconCam, &err);
    }
    if (!Vision::CalibMgr::instance().commitCamera(cameraOf(in), model, &err)) {
        std::string saveErr;
        if (!Vision::CalibMgr::instance().save(&saveErr)) {
            err += U(L"\uff1b") + saveErr;
        }
        out->ok = false;
        out->message = err;
        return out->errCode;
    }
    if (!Vision::CalibMgr::instance().save(&err)) {
        out->ok = false;
        out->message = err;
        return out->errCode;
    }
    out->ok = true;
    out->message = aout.message;
    out->syncIo();
    return out->errCode;
}

int Vision::CalibPlateTool::save()
{
    return ToolBase::save();
}

void Vision::CalibPlateTool::clear(const std::string& toolDataDir)
{
    ToolBase::clear(toolDataDir);
}
// 克隆实例
Vision::ToolBase* Vision::CalibPlateTool::clone() const
{
    CalibPlateTool* p = new CalibPlateTool();
    p->copyInstanceDataFrom(*this);
    return p;
}



// 返回本工具参数对象
const Vision::ToolParaBase* Vision::CalibPlateTool::tool_param() const
{
    return &param_;
}

int Vision::CalibPlateTool::check_param(std::string* detail) const
{
    if (param_.loadCode() != HS_OK) {
        if (detail != nullptr) {
            *detail = HsCommData::HsException(param_.loadCode()).errorMessage();
        }
        return param_.loadCode();
    }
    return HS_OK;
}

Vision::ToolOut Vision::CalibPlateTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}

Vision::CalibPlateTool::CalibPlateTool() {}
Vision::CalibPlateTool::~CalibPlateTool() {}
