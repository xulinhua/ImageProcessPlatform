/**
 * @file BarcodeTool.cpp
 * @brief 一维码识别工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "BarcodeTool.h"
#include "AlgToolBase.h"
#include "VisionVersion.h"
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};

const char kToolName[] = "identify.barcode";
const char kDisplayName[] = "一维码";
const char kDescription[] = "在搜索区里读一维码，输出文本、个数、码区和位姿";
}

std::string Vision::BarcodeTool::toolName() const
{
    return kToolName;
}

Vision::ToolType Vision::BarcodeTool::type() const
{
    return Vision::ToolType::Identify;
}

std::string Vision::BarcodeTool::displayName() const
{
    return kDisplayName;
}

std::string Vision::BarcodeTool::version() const
{
    return std::string(kToolVersion);
}

std::vector<Vision::VerNote> Vision::BarcodeTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Vision::BarcodeTool::description() const
{
    return kDescription;
}

std::vector<std::string> Vision::BarcodeTool::codeTypes() const
{
    std::vector<std::string> names;
    names.push_back(BarcodeKey::autoType);
    names.push_back("Code 128");
    names.push_back("Code 39");
    names.push_back("Code 93");
    names.push_back("EAN-13");
    names.push_back("EAN-8");
    names.push_back("UPC-A");
    names.push_back("2/5 Interleaved");
    return names;
}

Vision::ParamMap Vision::BarcodeTool::defaultParams() const
{
    return defaultsFrom(paramFields());
}

std::vector<Vision::ParamField> Vision::BarcodeTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString(BarcodeKey::codeType, "码制", BarcodeKey::autoType, codeTypes()));
    f.push_back(Vision::paramString(BarcodeKey::polarity, "极性", BarcodeKey::any,
        Vision::paramChoices(BarcodeKey::any, BarcodeKey::darkOnLight, BarcodeKey::lightOnDark)));
    f.push_back(Vision::paramInt(BarcodeKey::timeout, "超时(ms)", 2000, true, 0, false, 0));
    f.push_back(Vision::paramInt(BarcodeKey::num, "最多个数", 1, true, 1, true, 99));
    return f;
}

Vision::ToolCaps Vision::BarcodeTool::capabilities() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = BarcodeKey::roi;
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

Vision::ToolOut Vision::BarcodeTool::execute(const Vision::ToolIn& in)
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

Vision::ToolBase* Vision::BarcodeTool::clone() const
{
    BarcodeTool* p = new BarcodeTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::BarcodeTool::toolParam() const
{
    return &m_param;
}
