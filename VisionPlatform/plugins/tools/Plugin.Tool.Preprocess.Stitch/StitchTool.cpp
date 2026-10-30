/**
 * @file StitchTool.cpp
 * @brief 图像拼接工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StitchTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};

const char kToolName[] = "preprocess.stitch";
const char kDisplayName[] = "图像拼接";
const char kDescription[] = "把上游多张图按行列拼成一张，columns 为每行张数";
}

std::string Vision::StitchTool::toolName() const
{
    return kToolName;
}

Vision::ToolType Vision::StitchTool::type() const
{
    return Vision::ToolType::Preprocess;
}

std::string Vision::StitchTool::displayName() const
{
    return kDisplayName;
}

std::string Vision::StitchTool::version() const
{
    return std::string(kToolVersion);
}

std::vector<Vision::VerNote> Vision::StitchTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Vision::StitchTool::description() const
{
    return kDescription;
}

Vision::ParamMap Vision::StitchTool::defaultParams() const
{
    return defaultsFrom(paramFields());
}

std::vector<Vision::ParamField> Vision::StitchTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramInt(StitchKey::columns, "每行张数", 2, true, 1, true, 32));
    return f;
}

Vision::ToolCaps Vision::StitchTool::capabilities() const
{
    Vision::ToolCaps c;
    c.usesRoi = false;
    return c;
}

Vision::ToolOut Vision::StitchTool::execute(const Vision::ToolIn& in)
{
    m_param.load(in.param);
    Vision::ToolIn shot = in;
    shot.toolParam = &m_param;
    return Vision::runAlgToolBase(toolName(), shot, true);
}

Vision::ToolBase* Vision::StitchTool::clone() const
{
    StitchTool* p = new StitchTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::StitchTool::toolParam() const
{
    return &m_param;
}
