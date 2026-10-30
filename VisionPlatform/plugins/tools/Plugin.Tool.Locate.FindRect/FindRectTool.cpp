/**
 * @file FindRectTool.cpp
 * @brief 找矩形工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FindRectTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};

const char kToolName[] = "locate.find_rect";
const char kDisplayName[] = "找矩形";
const char kDescription[] = "在搜索区里拟合旋转矩形，输出中心、角度、宽高与叠加图形";
}

std::string Vision::FindRectTool::toolName() const
{
    return kToolName;
}

Vision::ToolType Vision::FindRectTool::type() const
{
    return Vision::ToolType::Locate;
}

std::string Vision::FindRectTool::displayName() const
{
    return kDisplayName;
}

std::string Vision::FindRectTool::version() const
{
    return std::string(kToolVersion);
}

std::vector<Vision::VerNote> Vision::FindRectTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Vision::FindRectTool::description() const
{
    return kDescription;
}

Vision::ParamMap Vision::FindRectTool::defaultParams() const
{
    return defaultsFrom(paramFields());
}

std::vector<Vision::ParamField> Vision::FindRectTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDouble(FindRectKey::sigma, "平滑", 1.0, true, 0.4, true, 20.0));
    f.push_back(Vision::paramInt(FindRectKey::low, "低阈值", 20, true, 0, true, 255));
    f.push_back(Vision::paramInt(FindRectKey::high, "高阈值", 40, true, 0, true, 255));
    f.push_back(Vision::paramDouble(FindRectKey::minSide, "最短边", 10.0, true, 1.0, false, 0));
    f.push_back(Vision::paramDouble(FindRectKey::maxSide, "最长边(0不限)", 0.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble(FindRectKey::maxDist, "拟合误差", 2.0, true, 0.1, true, 50.0));
    return f;
}

Vision::ToolCaps Vision::FindRectTool::capabilities() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = FindRectKey::roi;
    slot.title = "搜索区";
    slot.maxCount = 1;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect2, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 0));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRoi);
    c.slotRules.push_back(slot);
    return c;
}

Vision::ToolOut Vision::FindRectTool::execute(const Vision::ToolIn& in)
{
    m_param.load(in.param);
    Vision::ToolIn shot = in;
    shot.toolParam = &m_param;
    Vision::ToolOut out = Vision::runAlgToolBase(toolName(), shot, false);
    out.io.images.clear();
    if (in.input.isNull() && out.image.isNull()) {
        out.ok = false;
    }
    return out;
}

Vision::ToolBase* Vision::FindRectTool::clone() const
{
    FindRectTool* p = new FindRectTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::FindRectTool::toolParam() const
{
    return &m_param;
}
