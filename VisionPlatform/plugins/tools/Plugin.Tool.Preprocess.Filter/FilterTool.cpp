/**
 * @file FilterTool.cpp
 * @brief 图像滤波：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FilterTool.h"
#include "AlgToolBase.h"
#include "TextEncoding.h"
#include "VisionVersion.h"
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版：多步骤滤波，支持形态学和步骤绑定区域" },
};
const char kToolName[] = "preprocess.filter";
const char kDisplayName[] = "图像滤波";
const char kDescription[] = "按顺序做增强、平滑和灰度形态学，每步可绑定一块区域";

std::string stepKey(int index, const char* suffix)
{
    return std::string("step_") + std::to_string(static_cast<long long>(index)) + "_" + suffix;
}

Vision::ParamMap defaultsFrom(const std::vector<Vision::ParamField>& fields)
{
    Vision::ParamMap m;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].type == Vision::ParamFieldType::Device) {
            continue;
        }
        m.insert(fields[i].key, fields[i].def);
    }
    return m;
}
}

std::string Vision::FilterTool::toolName() const
{
    return kToolName;
}

Vision::ToolType Vision::FilterTool::type() const
{
    return Vision::ToolType::Preprocess;
}

std::string Vision::FilterTool::displayName() const
{
    return kDisplayName;
}

std::string Vision::FilterTool::version() const
{
    return std::string(kToolVersion);
}

std::vector<Vision::VerNote> Vision::FilterTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Vision::FilterTool::description() const
{
    return kDescription;
}

std::vector<Vision::ParamField> Vision::FilterTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramInt(FilterKey::stepCount, "步骤数", 1, true, 1, true, 16));
    const std::string p = stepKey(0, "");
    f.push_back(Vision::paramString((p + FilterKey::op).c_str(), "算子", FilterKey::opEquHisto,
        Vision::paramChoices(FilterKey::opEquHisto, FilterKey::opEmphasize, FilterKey::opIlluminate, FilterKey::opScale)));
    f.push_back(Vision::paramInt((p + FilterKey::w).c_str(), "掩膜宽", 7, true, 1, true, 101));
    f.push_back(Vision::paramInt((p + FilterKey::h).c_str(), "掩膜高", 7, true, 1, true, 101));
    f.push_back(Vision::paramDouble((p + FilterKey::c0).c_str(), "参数1", 1.0, false, 0, false, 0));
    f.push_back(Vision::paramDouble((p + FilterKey::c1).c_str(), "参数2", 3.0, false, 0, false, 0));
    f.push_back(Vision::paramBool((p + FilterKey::scaleMax).c_str(), "拉伸到最大", false));
    f.push_back(Vision::paramString((p + FilterKey::shape).c_str(), "掩膜形状", FilterKey::shapeRect,
        Vision::paramChoices(FilterKey::shapeRect, FilterKey::shapeOctagon, FilterKey::shapeRhombus)));
    f.push_back(Vision::paramInt((p + FilterKey::roiIndex).c_str(), "绑定区域", -1, true, -1, true, 49));
    return f;
}

Vision::ParamMap Vision::FilterTool::defaultParams() const
{
    return defaultsFrom(paramFields());
}

Vision::ToolCaps Vision::FilterTool::capabilities() const
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = FilterKey::roi;
    slot.title = "滤波区域";
    slot.maxCount = 50;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjPoly, 0));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRegion);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRegion);
    c.slotRules.push_back(slot);
    return c;
}

Vision::ToolOut Vision::FilterTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out = Vision::runAlgToolBase(toolName(), in, true);
    if (in.input.isNull() && out.image.isNull()) {
        out.ok = false;
    }
    return out;
}

Vision::ToolBase* Vision::FilterTool::clone() const
{
    FilterTool* p = new FilterTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::FilterTool::toolParam() const
{
    return &m_param;
}
