#include "MergeTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <stdio.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "点云合并" } };
}
std::string MergeTool::toolName() const { return "prepare3d.merge"; }
ToolType MergeTool::type() const { return ToolType::Prepare3D; }
std::string MergeTool::displayName() const { return "点云合并"; }
std::string MergeTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> MergeTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string MergeTool::description() const { return "合并多路输入点云"; }
std::vector<ParamField> MergeTool::paramFields() const { return std::vector<ParamField>(); }
ParamMap MergeTool::defaultParams() const { return ParamMap(); }
std::vector<PortDesc> MergeTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud));
    PortDesc extra(PortDir::In, PortData::Cloud);
    extra.slot.name = "cloud2";
    p.push_back(extra);
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return p;
}
ToolOut MergeTool::execute(const ToolIn& in)
{
    ToolOut out;
    HsCommData::Cloud3 merged;
    for (size_t i = 0; i < in.inputIo.clouds.size(); ++i) {
        const HsCommData::Cloud3& c = in.inputIo.clouds[i].cloud;
        if (c.isEmpty()) {
            continue;
        }
        merged.xyz.insert(merged.xyz.end(), c.xyz.begin(), c.xyz.end());
        if (!c.normals.empty()) {
            if (merged.normals.empty()) {
                merged.normals.assign(merged.xyz.size() - c.xyz.size(), HsCommData::Point3d());
            }
            merged.normals.insert(merged.normals.end(), c.normals.begin(), c.normals.end());
        }
    }
    if (merged.xyz.empty()) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "没有点云";
        return out;
    }
    if (!merged.normals.empty() && merged.normals.size() != merged.xyz.size()) {
        merged.normals.clear();
    }
    PortCloud pc;
    pc.cloud = merged;
    out.io.clouds.push_back(pc);
    out.ok = true;
    out.errCode = HS_OK;
    char msg[64];
    sprintf_s(msg, "点数 %d", static_cast<int>(merged.xyz.size()));
    out.message = msg;
    return out;
}
ToolBase* MergeTool::clone() const
{
    MergeTool* p = new MergeTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
