/**
 * @file FitPlaneTool.cpp
 * @brief 拟合平面工具壳
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FitPlaneTool.h"
#include "AlgToolBase.h"
#include "TextEncoding.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <sstream>
#include <string.h>

namespace Vision {

namespace {

const char kToolVersion[] = "1.0.0.0";
const VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "拟合平面" },
};

}

std::string FitPlaneTool::toolName() const
{
    return "measure3d.fit_plane";
}

ToolType FitPlaneTool::type() const
{
    return ToolType::Measure3D;
}

std::string FitPlaneTool::displayName() const
{
    return "拟合平面";
}

std::string FitPlaneTool::version() const
{
    return std::string(kToolVersion);
}

std::vector<VerNote> FitPlaneTool::versionNotes() const
{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string FitPlaneTool::description() const
{
    return "用点云拟合平面。没有上游点云时读取 ASCII PLY 或 PCD";
}

std::vector<ParamField> FitPlaneTool::fields() const
{
    std::vector<ParamField> f;
    f.push_back(paramPath("file", "点云文件", std::string()));
    f.push_back(paramDouble("distance", "距离阈值", 0.5, true, 0.0, false, 0.0));
    return f;
}

ParamMap FitPlaneTool::defaults() const
{
    ParamMap m;
    const std::vector<ParamField> f = fields();
    for (size_t i = 0; i < f.size(); ++i) {
        m.insert(f[i].key, f[i].def);
    }
    return m;
}

ParamMap FitPlaneTool::defaultParams() const
{
    return defaults();
}

std::vector<ParamField> FitPlaneTool::paramFields() const
{
    return fields();
}

std::vector<PortDesc> FitPlaneTool::ports() const
{
    std::vector<PortDesc> p = ToolBase::ports();
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i].dir() == PortDir::Out && p[i].data() == PortData::Cloud) {
            p[i].slot.name = "inlier";
        }
    }
    return p;
}

std::string FitPlaneTool::trimLine(const std::string& line) const
{
    size_t a = 0;
    while (a < line.size() && (line[a] == ' ' || line[a] == '\t' || line[a] == '\r' || line[a] == '\n')) {
        ++a;
    }
    size_t b = line.size();
    while (b > a && (line[b - 1] == ' ' || line[b - 1] == '\t' || line[b - 1] == '\r' || line[b - 1] == '\n')) {
        --b;
    }
    return line.substr(a, b - a);
}

bool FitPlaneTool::splitDoubles(const std::string& line, std::vector<double>* nums) const
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

std::string FitPlaneTool::resolvePath(const ToolIn& in, const std::string& file, std::string* err) const
{
    const bool absWin = file.size() >= 2 && file[1] == ':';
    const bool absUnix = !file.empty() && (file[0] == '/' || file[0] == '\\');
    if (absWin || absUnix) {
        return file;
    }
    if (in.solutionDir.empty()) {
        if (err) {
            *err = "请先保存方案";
        }
        return std::string();
    }
    std::string dir = in.solutionDir;
    if (dir.back() != '/' && dir.back() != '\\') {
        dir.push_back('\\');
    }
    return dir + file;
}

bool FitPlaneTool::readPly(FILE* fp, HsCommData::Cloud3* cloud, std::string* err) const
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

bool FitPlaneTool::readPcd(FILE* fp, HsCommData::Cloud3* cloud, std::string* err) const
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

bool FitPlaneTool::loadFileCloud(const ToolIn& in, HsCommData::Cloud3* cloud, std::string* err) const
{
    if (!cloud) {
        return false;
    }
    const std::string file = in.param.run.value("file").toString();
    if (file.empty()) {
        return true;
    }
    const std::string path = resolvePath(in, file, err);
    if (path.empty()) {
        return false;
    }
    const std::wstring wide = utf8ToWide(path);
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
        ok = readPly(fp, cloud, err);
    } else {
        fseek(fp, 0, SEEK_SET);
        ok = readPcd(fp, cloud, err);
    }
    fclose(fp);
    return ok;
}

ToolOut FitPlaneTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    shot.algLib = AlgLibPcl;
    if (shot.inputIo.clouds.empty()) {
        HsCommData::Cloud3 cloud;
        std::string err;
        if (!loadFileCloud(shot, &cloud, &err)) {
            ToolOut out;
            out.ok = false;
            out.error = true;
            out.errCode = HS_ERR_OPERATOR;
            out.message = err;
            return out;
        }
        if (!cloud.xyz.empty()) {
            PortCloud one;
            one.cloud = cloud;
            shot.inputIo.clouds.push_back(one);
        }
    }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) {
        out.message = "未登记测量算法";
    }
    return out;
}

ToolBase* FitPlaneTool::clone() const
{
    FitPlaneTool* p = new FitPlaneTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

}
