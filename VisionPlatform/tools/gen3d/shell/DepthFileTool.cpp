#include "DepthFileTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "深度图文件" } };
}
std::string DepthFileTool::toolName() const { return "acquire3d.depth_file"; }
ToolType DepthFileTool::type() const { return ToolType::Acquire3D; }
std::string DepthFileTool::displayName() const { return "深度图文件"; }
std::string DepthFileTool::version() const { return "1.0.0.0"; }
std::vector<VerNote> DepthFileTool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }
std::string DepthFileTool::description() const { return "从文件读取深度图"; }
std::vector<ParamField> DepthFileTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramPath("path", "深度图路径", std::string()));
    return f;
}
ParamMap DepthFileTool::defaultParams() const { return defaultsFrom(paramFields()); }
std::vector<PortDesc> DepthFileTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::Out, PortData::Image));
    return p;
}
ToolOut DepthFileTool::execute(const ToolIn& in)
{
    ToolOut out;
    if (!in.inputIo.images.empty() && !in.inputIo.images[0].image.isNull()) {
        out.io.images.push_back(in.inputIo.images[0]);
        out.ok = true;
        out.errCode = HS_OK;
        out.message = "使用上游深度图";
        return out;
    }
    const std::string path = in.param.run.value("path").toString();
    if (path.empty()) {
        out.error = true;
        out.errCode = HS_ERR_OPERATOR;
        out.message = "请指定深度图路径";
        return out;
    }
    (void)path;
    out.error = true;
    out.errCode = HS_ERR_OPERATOR;
    out.message = "深度图加载未实现";
    return out;
}
ToolBase* DepthFileTool::clone() const
{
    DepthFileTool* p = new DepthFileTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
}
