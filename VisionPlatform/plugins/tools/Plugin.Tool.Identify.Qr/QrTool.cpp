/**
 * @file QrTool.cpp
 * @brief 二维码识别工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "QrTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};

const char kToolName[] = "identify.qr";
const char kDisplayName[] = "二维码";
const char kDescription[] = "在搜索区里读二维码，输出文本、个数、码区和位姿";
}

std::string Vision::QrTool::toolName() const
{
    return kToolName;
}

Vision::ToolType Vision::QrTool::type() const
{
    return Vision::ToolType::Identify;
}

std::string Vision::QrTool::displayName() const
{
    return kDisplayName;
}

std::string Vision::QrTool::version() const
{
    return std::string(kToolVersion);
}

std::vector<Vision::VerNote> Vision::QrTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Vision::QrTool::description() const
{
    return kDescription;
}

Vision::ParamMap Vision::QrTool::defaultParams() const
{
    return defaultsFrom(paramFields());
}

std::vector<Vision::ParamField> Vision::QrTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString(QrKey::polarity, "极性", QrKey::any,
        Vision::paramChoices(QrKey::any, QrKey::darkOnLight, QrKey::lightOnDark)));
    f.push_back(Vision::paramInt(QrKey::timeout, "超时(ms)", 2000, true, 0, false, 0));
    f.push_back(Vision::paramInt(QrKey::num, "最多个数", 1, true, 1, true, 99));
    f.push_back(Vision::paramInt(QrKey::moduleMin, "最小模块", 0, true, 0, true, 100));
    return f;
}

Vision::ToolCaps Vision::QrTool::capabilities() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = QrKey::roi;
    slot.title = "搜索区";
    slot.maxCount = 1;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect2, 0));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRoi);
    c.slotRules.push_back(slot);
    return c;
}

Vision::ToolOut Vision::QrTool::execute(const Vision::ToolIn& in)
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

Vision::ToolBase* Vision::QrTool::clone() const
{
    QrTool* p = new QrTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::QrTool::toolParam() const
{
    return &m_param;
}
