/**
 * @file FitPlaneTool.cpp
 * @brief 拟合平面工具壳
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FitPlaneTool.h"
#include "AlgToolBase.h"
#include "CloudFileGrabber.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

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
    return CloudFileGrabber::load(path, cloud, err);
}

ToolOut FitPlaneTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    // 三维测量只认 PCL / Open3D；旧方案若仍是 Hal 则回落到 PCL
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) {
        shot.algLib = AlgLibPcl;
    }
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
    // 输入点云也放进输出缺省口，供点云窗做底云（内点仍在 inlier）
    if (!shot.inputIo.clouds.empty() && !shot.inputIo.clouds[0].cloud.isEmpty()) {
        PortCloud base = shot.inputIo.clouds[0];
        base.port.clear();
        base.index = 0;
        out.io.clouds.insert(out.io.clouds.begin(), base);
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
