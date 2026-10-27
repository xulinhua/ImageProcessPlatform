/**
 * @file CalibNPointTool.cpp
 * @brief CalibNPointTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CalibNPointTool.h"
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
    return proto->run(in);
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

// 九点标定工具名
std::string Vision::CalibNPointTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::CalibNPointTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Vision::CalibNPointTool::toolName() const { return "calib.npoint"; }
// 类型为标定
Vision::ToolType Vision::CalibNPointTool::type() const { return Vision::ToolType::Calib; }
// 界面显示名
std::string Vision::CalibNPointTool::displayName() const { return U(L"\u4e5d\u70b9\u6807\u5b9a"); }
// 工具说明
std::string Vision::CalibNPointTool::description() const
{
    return U(L"\u5b66\u4e60\u65f6\u6309\u70b9\u8868\u62df\u5408\u5e73\u9762\u6620\u5c04\u3002\u6bcf\u884c imageX,imageY,worldX,worldY");
}
// 默认参数
Vision::ParamMap Vision::CalibNPointTool::defaultParams() const { return defaultsFrom(paramFields()); }
// 参数说明
std::vector<Vision::ParamField> Vision::CalibNPointTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDevice("deviceId", "相机"));
    f.push_back(Vision::paramPath("tablePath", "点表", std::string()));
    f.push_back(Vision::paramPath("imageDir", "imageDir", std::string()));
    return f;
}
// 不使用搜索区
Vision::ToolCaps Vision::CalibNPointTool::capabilities() const { return noRoi(); }
// 学习工具无端口
std::vector<Vision::PortDesc> Vision::CalibNPointTool::ports() const
{
    return std::vector<Vision::PortDesc>();
}
// 学习时导入点表并拟合平面
Vision::ToolOut Vision::CalibNPointTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out = needCamera(in);
    if (!out.ok) {
        return out;
    }
    if (in.mode != Vision::RunMode::Learn) {
        out.ok = true;
        out.message = U(L"\u81ea\u52a8\u8fd0\u884c\u4e0d\u6539\u6807\u5b9a");
        out.syncIo();
        return out;
    }
    const std::string path = in.param.run.value("tablePath").toString();
    if (path.empty()) {
        Vision::AlgToolIn ain;
        ain.mode = in.mode;
        ain.run = in.param.run;
        ain.solutionDir = in.solutionDir;
        fillBoardRun(cameraOf(in), &ain.run);
        std::string aerr;
        const Vision::AlgToolOut aout = runNamed("calib.npoint", in.algLib, ain, &aerr);
        const std::string pts = aout.values.value("points").toString();
        if (!aout.ok || pts.empty()) {
            out.ok = false;
            out.message = aout.message.empty() ? aerr : aout.message;
            return out;
        }
        const std::string csv = Vision::pathJoin(ain.run.value("imageDir").toString(), "board.csv");
        std::ofstream csvFile(csv.c_str(), std::ios::binary);
        if (!csvFile) {
            out.ok = false;
            out.message = U(L"\u65e0\u6cd5\u8bfb\u53d6\u6587\u4ef6");
            return out;
        }
        csvFile << pts;
        csvFile.close();
        const int iw = aout.values.value("width").toInt(0);
        const int ih = aout.values.value("height").toInt(0);
        if (iw > 0 && ih > 0) {
            Vision::CalibMgr::instance().setImageSize(cameraOf(in), iw, ih, &aerr);
        }
        if (!Vision::CalibMgr::instance().importPointTable(cameraOf(in), csv, &aerr)) {
            Vision::CalibMgr::instance().save(&aerr);
            out.ok = false;
            out.message = aerr;
            return out;
        }
        if (!Vision::CalibMgr::instance().save(&aerr)) {
            out.ok = false;
            out.message = aerr;
            return out;
        }
        out.ok = true;
        out.message = U(L"\u5e73\u9762\u6620\u5c04\u5df2\u751f\u6548");
        out.syncIo();
        return out;
    }
    std::string err;
    if (!Vision::CalibMgr::instance().importPointTable(cameraOf(in), path, &err)
        || !Vision::CalibMgr::instance().save(&err)) {
        out.ok = false;
        out.message = err;
        return out;
    }
    out.ok = true;
    out.message = U(L"\u5e73\u9762\u6620\u5c04\u5df2\u751f\u6548");
    out.syncIo();
    return out;
}
// 克隆实例
Vision::ToolBase* Vision::CalibNPointTool::clone() const
{
    CalibNPointTool* p = new CalibNPointTool();
    p->copyInstanceDataFrom(*this);
    return p;
}



// 返回本工具参数对象
const Vision::ToolParaBase* Vision::CalibNPointTool::toolParam() const
{
    return &m_param;
}
