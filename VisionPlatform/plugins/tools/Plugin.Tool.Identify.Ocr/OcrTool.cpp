/**
 * @file OcrTool.cpp
 * @brief 字符识别工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "OcrTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};

const char kToolName[] = "identify.ocr";
const char kDisplayName[] = "字符识别";
const char kDescription[] = "在搜索区里读字符，输出文本、字符数、字符区和位姿；填了期望文本则比对";
}

std::string Vision::OcrTool::toolName() const
{
    return kToolName;
}

Vision::ToolType Vision::OcrTool::type() const
{
    return Vision::ToolType::Identify;
}

std::string Vision::OcrTool::displayName() const
{
    return kDisplayName;
}

std::string Vision::OcrTool::version() const
{
    return std::string(kToolVersion);
}

std::vector<Vision::VerNote> Vision::OcrTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Vision::OcrTool::description() const
{
    return kDescription;
}

std::vector<std::string> Vision::OcrTool::fonts() const
{
    std::vector<std::string> names;
    names.push_back(OcrKey::defaultFont);
    names.push_back("Universal_0-9_NoRej.occ");
    names.push_back("Industrial_0-9A-Z_NoRej.omc");
    names.push_back("Document_0-9A-Z_NoRej.omc");
    names.push_back("DotPrint_0-9A-Z_NoRej.omc");
    names.push_back("SEMI_NoRej.omc");
    return names;
}

Vision::ParamMap Vision::OcrTool::defaultParams() const
{
    return defaultsFrom(paramFields());
}

std::vector<Vision::ParamField> Vision::OcrTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString(OcrKey::font, "字体", OcrKey::defaultFont, fonts()));
    f.push_back(Vision::paramString(OcrKey::polarity, "极性", OcrKey::darkOnLight,
        Vision::paramChoices(OcrKey::darkOnLight, OcrKey::lightOnDark, OcrKey::both)));
    f.push_back(Vision::paramInt(OcrKey::minCharHeight, "最小字高(0自动)", 0, true, 0, false, 0));
    f.push_back(Vision::paramInt(OcrKey::maxCharHeight, "最大字高(0自动)", 0, true, 0, false, 0));
    f.push_back(Vision::paramString(OcrKey::expected, "期望文本", std::string(), std::vector<std::string>()));
    return f;
}

Vision::ToolCaps Vision::OcrTool::capabilities() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = OcrKey::roi;
    slot.title = "搜索区";
    slot.maxCount = 1;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect2, 0));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRoi);
    c.slotRules.push_back(slot);
    return c;
}

Vision::ToolOut Vision::OcrTool::execute(const Vision::ToolIn& in)
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

Vision::ToolBase* Vision::OcrTool::clone() const
{
    OcrTool* p = new OcrTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::OcrTool::toolParam() const
{
    return &m_param;
}
