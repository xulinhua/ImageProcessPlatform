/**
 * @file CalibMapPoseTool.cpp
 * @brief CalibMapPoseTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CalibMapPoseTool.h"
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

// 位姿换算工具名
std::string Vision::CalibMapPoseTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::CalibMapPoseTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Vision::CalibMapPoseTool::toolName() const { return "calib.map_pose"; }
// 类型为标定
Vision::ToolType Vision::CalibMapPoseTool::type() const { return Vision::ToolType::Calib; }
// 界面显示名
std::string Vision::CalibMapPoseTool::displayName() const { return U(L"\u4f4d\u59ff\u6362\u7b97"); }
// 工具说明
std::string Vision::CalibMapPoseTool::description() const
{
    return U(L"\u628a\u4e0a\u6e38\u50cf\u7d20\u4f4d\u59ff\u6362\u6210\u6beB\u7c73\uff0c\u5e76\u6309\u65cb\u8f6c\u4e2d\u5fc3\u8865\u507f");
}
// 默认参数
Vision::ParamMap Vision::CalibMapPoseTool::defaultParams() const { return defaultsFrom(paramFields()); }
// 参数说明
std::vector<Vision::ParamField> Vision::CalibMapPoseTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDevice("deviceId", "相机"));
    return f;
}
// 不使用搜索区
Vision::ToolCaps Vision::CalibMapPoseTool::capabilities() const { return noRoi(); }
// 位姿输入输出
std::vector<Vision::PortDesc> Vision::CalibMapPoseTool::ports() const
{
    std::vector<Vision::PortDesc> p;
    p.push_back(Vision::PortDesc(Vision::PortDir::In, Vision::PortData::Pose));
    p.push_back(Vision::PortDesc(Vision::PortDir::Out, Vision::PortData::Pose));
    return p;
}
// 像素位姿换到毫米
Vision::ToolOut Vision::CalibMapPoseTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out = needCamera(in);
    if (!out.ok) {
        return out;
    }
    if (in.inputIo.poses.empty()) {
        out.ok = false;
        out.message = U(L"\u6ca1\u6709\u8f93\u5165\u4f4d\u59ff");
        return out;
    }
    std::string err;
    bool any = false;
    for (size_t i = 0; i < in.inputIo.poses.size(); ++i) {
        const HsCommData::Pose2& src = in.inputIo.poses[i].pose;
        Vision::CalibPose pin;
        pin.x = src.tx;
        pin.y = src.ty;
        pin.angle = src.angDeg();
        Vision::CalibPose pout;
        if (!Vision::CalibMgr::instance().mapPose(cameraOf(in), pin, &pout, &err)) {
            out.ok = false;
            out.message = err;
            return out;
        }
        Vision::PortPose dst;
        dst.port = in.inputIo.poses[i].port;
        dst.index = in.inputIo.poses[i].index;
        dst.pose.tx = pout.x;
        dst.pose.ty = pout.y;
        dst.pose.setAngDeg(pout.angle);
        out.io.poses.push_back(dst);
        any = true;
    }
    if (!any) {
        out.ok = false;
        out.message = U(L"\u6ca1\u6709\u6709\u6548\u4f4d\u59ff");
        return out;
    }
    out.ok = true;
    out.message = U(L"\u5df2\u6362\u7b97\u5230\u6beB\u7c73");
    out.syncIo();
    return out;
}
// 克隆实例
Vision::ToolBase* Vision::CalibMapPoseTool::clone() const
{
    CalibMapPoseTool* p = new CalibMapPoseTool();
    p->copyInstanceDataFrom(*this);
    return p;
}


// 返回本工具参数对象
const Vision::ToolParaBase* Vision::CalibMapPoseTool::toolParam() const
{
    return &m_param;
}
