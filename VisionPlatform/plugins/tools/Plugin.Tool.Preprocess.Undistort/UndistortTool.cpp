/**
 * @file UndistortTool.cpp
 * @brief UndistortTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "UndistortTool.h"
#include "AlgToolBase.h"
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
std::string Vision::UndistortTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::UndistortTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

// 注册名
std::string Vision::UndistortTool::toolName() const { return "preprocess.undistort"; }
// 类型为标定
Vision::ToolType Vision::UndistortTool::type() const { return Vision::ToolType::Calib; }
// 界面显示名
std::string Vision::UndistortTool::displayName() const { return U(L"\u7578\u53d8\u6821\u6b63"); }
// 工具说明
std::string Vision::UndistortTool::description() const
{
    return U(L"\u6309\u8fd9\u8def\u76f8\u673a\u751f\u6548\u7684\u5185\u53c2\u53bb\u7578\u53d8\u3002\u6ca1\u6709\u6a21\u578b\u5219\u539f\u56fe\u4f20\u4e0b");
}
// 默认参数
Vision::ParamMap Vision::UndistortTool::defaultParams() const { return defaultsFrom(paramFields()); }
// 参数说明
std::vector<Vision::ParamField> Vision::UndistortTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDevice("deviceId", "相机"));
    return f;
}
// 不使用搜索区
Vision::ToolCaps Vision::UndistortTool::capabilities() const { return noRoi(); }
// 图像输入输出
std::vector<Vision::PortDesc> Vision::UndistortTool::ports() const
{
    std::vector<Vision::PortDesc> p;
    p.push_back(Vision::PortDesc(Vision::PortDir::In, Vision::PortData::Image));
    p.push_back(Vision::PortDesc(Vision::PortDir::Out, Vision::PortData::Image));
    return p;
}
// 按生效内参去畸变
Vision::ToolOut Vision::UndistortTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    out.frameId = in.input.frameId;
    out.source = in.input;
    if (in.input.isNull()) {
        out.ok = false;
        out.message = U(L"\u6ca1\u6709\u8f93\u5165\u56fe");
        return out;
    }
    if (cameraOf(in).isNull()) {
        out.ok = false;
        out.message = U(L"\u8bf7\u5728\u5de5\u5177\u4e0a\u7ed1\u5b9a\u76f8\u673a");
        return out;
    }
    // 记下当前图尺寸，再查这路相机生效的模型：失效报错，未标定原图透传
    if (in.input.width > 0 && in.input.height > 0) {
        std::string sizeErr;
        Vision::CalibMgr::instance().setImageSize(cameraOf(in), in.input.width, in.input.height, &sizeErr);
    }
    const Vision::CalibRst active = Vision::CalibMgr::instance().active(cameraOf(in));
    if (active.camera.present && active.camera.status == Vision::CalibExpired) {
        out.ok = false;
        out.message = U(L"\u76f8\u673a\u6a21\u578b\u5df2\u5931\u6548");
        return out;
    }
    if (!active.camera.present || active.camera.status != Vision::CalibValid) {
        out.image = in.input;
        out.ok = true;
        out.message = U(L"\u672a\u6821\u6b63");
        out.syncIo();
        return out;
    }
    // 内参传给算法；有 Halcon 相机参数文件时强制走 Halcon 算法库
    Vision::AlgToolIn ain;
    ain.mode = in.mode;
    ain.imagePtr = &in.input;
    ain.imageNative = in.inputNative;
    ain.solutionDir = in.solutionDir;
    const Vision::CamModel& m = active.camera;
    ain.run.insert("fx", m.fx);
    ain.run.insert("fy", m.fy);
    ain.run.insert("cx", m.cx);
    ain.run.insert("cy", m.cy);
    ain.run.insert("k1", m.k1);
    ain.run.insert("k2", m.k2);
    ain.run.insert("k3", m.k3);
    ain.run.insert("p1", m.p1);
    ain.run.insert("p2", m.p2);
    ain.run.insert("stamp", m.updatedAt);
    ain.run.insert("halconCam", Vision::CalibMgr::instance().readHalconDat(cameraOf(in)));
    Vision::AlgLibKind prefer = in.algLib;
    if (!ain.run.value("halconCam").toString().empty()) {
        prefer = Vision::AlgLibHal;
    }
    std::string err;
    const Vision::AlgToolOut aout = runNamed("preprocess.undistort", prefer, ain, &err);
    if (!aout.ok || aout.image.image.isNull()) {
        out.ok = false;
        out.message = aout.message.empty() ? err : aout.message;
        return out;
    }
    out.image = aout.image.image;
    out.ok = true;
    out.message = aout.message;
    out.syncIo();
    return out;
}
// 克隆实例
Vision::ToolBase* Vision::UndistortTool::clone() const
{
    UndistortTool* p = new UndistortTool();
    p->copyInstanceDataFrom(*this);
    return p;
}



// 返回本工具参数对象
const Vision::ToolParaBase* Vision::UndistortTool::toolParam() const
{
    return &m_param;
}
