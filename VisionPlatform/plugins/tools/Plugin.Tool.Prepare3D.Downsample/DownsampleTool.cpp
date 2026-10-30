/**
 * @file DownsampleTool.cpp
 * @brief 点云降采样工具壳
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DownsampleTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {

namespace {
const char kToolVersion[] = "1.0.0.0";
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "体素降采样" } };
}

std::string DownsampleTool::toolName() const { return "prepare3d.downsample"; }
ToolType DownsampleTool::type() const { return ToolType::Prepare3D; }
std::string DownsampleTool::displayName() const { return "降采样"; }
std::string DownsampleTool::version() const { return std::string(kToolVersion); }
std::vector<VerNote> DownsampleTool::versionNotes() const
{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}
std::string DownsampleTool::description() const { return "体素网格降采样点云"; }

std::vector<ParamField> DownsampleTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramDouble("leaf", "体素边长", 1.0, true, 0.0, false, 0.0));
    return f;
}
ParamMap DownsampleTool::defaultParams() const { return defaultsFrom(paramFields()); }

std::vector<PortDesc> DownsampleTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return p;
}

ToolOut DownsampleTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) {
        shot.algLib = AlgLibPcl;
    }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) {
        out.message = "未登记点云算法";
    }
    return out;
}

ToolBase* DownsampleTool::clone() const
{
    DownsampleTool* p = new DownsampleTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

}
