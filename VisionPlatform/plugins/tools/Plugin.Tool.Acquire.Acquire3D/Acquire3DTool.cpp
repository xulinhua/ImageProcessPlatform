/**
 * @file Acquire3DTool.cpp
 * @brief Acquire3DTool 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Acquire3DTool.h"
#include "CloudFileGrabber.h"
#include "VisionVersion.h"

namespace Vision {

namespace {

const char kToolVersion[] = "1.0.0.0";
const VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版：单文件或文件夹加载 ASCII PLY/PCD" },
};

const char kToolName[] = "acquire3d.load_cloud";
const char kDisplayName[] = "加载点云";
const char kDescription[] = "从单个点云文件或文件夹循环加载 ASCII PLY / PCD";

}

std::string Acquire3DTool::toolName() const
{
    return kToolName;
}

ToolType Acquire3DTool::type() const
{
    return ToolType::Acquire3D;
}

std::string Acquire3DTool::displayName() const
{
    return kDisplayName;
}

std::string Acquire3DTool::version() const
{
    return std::string(kToolVersion);
}

std::vector<VerNote> Acquire3DTool::versionNotes() const
{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Acquire3DTool::description() const
{
    return kDescription;
}

std::vector<ParamField> Acquire3DTool::paramFields() const
{
    std::vector<ParamField> f;
    f.push_back(paramString("sourceMode", "来源", std::string("folder"),
                             paramChoices("folder", "file")));
    f.push_back(paramPath("path", "路径", std::string()));
    return f;
}

ParamMap Acquire3DTool::defaultParams() const
{
    return defaultsFrom(paramFields());
}

std::vector<PortDesc> Acquire3DTool::ports() const
{
    std::vector<PortDesc> p;
    p.push_back(PortDesc(PortDir::Out, PortData::Cloud));
    return p;
}

std::string Acquire3DTool::resolvePath(const ToolIn& in, const std::string& file) const
{
    if (file.empty()) {
        return file;
    }
    const bool absWin = file.size() >= 2 && file[1] == ':';
    const bool absUnix = file[0] == '/' || file[0] == '\\';
    if (absWin || absUnix) {
        return file;
    }
    if (in.solutionDir.empty()) {
        return file;
    }
    std::string dir = in.solutionDir;
    if (dir.back() != '/' && dir.back() != '\\') {
        dir.push_back('\\');
    }
    return dir + file;
}

ToolOut Acquire3DTool::execute(const ToolIn& in)
{
    ToolOut out;
    ParamMap runtime = in.runtime;
    const std::string mode = in.param.run.value("sourceMode").toString();
    const std::string rawPath = in.param.run.value("path").toString();
    const std::string path = resolvePath(in, rawPath);
    const std::string sourceMode = (mode == "file") ? "file" : "folder";

    if (in.param.run.value("pathChanged").toInt()) {
        runtime.insert("index", 0);
        runtime.raw().erase("grabPath");
    }

    HsCommData::Cloud3 cloud;
    std::string err;
    if (!CloudFileGrabber::grab(path, sourceMode, &runtime, &cloud, &err)) {
        out.runtime = runtime;
        out.message = err;
        out.error = true;
        return out;
    }

    PortCloud one;
    one.cloud = cloud;
    out.io.clouds.push_back(one);
    out.ok = true;
    out.runtime = runtime;
    out.message = runtime.value("lastFile").toString();
    return out;
}

ToolBase* Acquire3DTool::clone() const
{
    Acquire3DTool* p = new Acquire3DTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const ToolParaBase* Acquire3DTool::toolParam() const
{
    return &m_param;
}

}
