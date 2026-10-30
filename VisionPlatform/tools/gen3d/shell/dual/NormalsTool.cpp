#include "NormalsTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "娉曞悜浼拌" } };
}
std::string NormalsTool::toolName() const { return "prepare3d.normals"; }
ToolType NormalsTool::type() const { return ToolType::Prepare3D; }
std::string NormalsTool::displayName() const { return "娉曞悜浼拌"; }
std::string NormalsTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> NormalsTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string NormalsTool::description() const { return "浼拌鐐逛簯娉曞悜"; }
std::vector<ParamField> NormalsTool::paramFields() const
{
    std::vector<ParamField> f;
    p.push_back(PortDesc(PortDir::In, PortData::Cloud)); p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return f;
}
ParamMap NormalsTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> NormalsTool::ports() const
{
    std::vector<PortDesc> p;
    
    return p;
}
ToolOut NormalsTool::execute(const ToolIn& in)
{
    ToolIn shot = in;
    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }
    ToolOut out = runAlgToolBase(toolName(), shot, false);
    if (out.errCode == HS_ERR_NO_ALG) { out.message = "鏈櫥璁扮偣浜戠畻娉?@
f.push_back(paramDouble("radius", "鎼滅储鍗婂緞", 5.0, true, 0.01, false, 0.0));
@"; }
    return out;
}
ToolBase* NormalsTool::clone() const
{
    NormalsTool* p = new NormalsTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}