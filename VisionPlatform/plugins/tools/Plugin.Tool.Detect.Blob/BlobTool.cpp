/**
 * @file BlobTool.cpp
 * @brief Blob 工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "BlobTool.h"
#include "AlgToolBase.h"
#include "TextEncoding.h"
#include "VisionVersion.h"
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "initial release" },
};

const char kToolName[] = "detect.blob";
const char kDisplayName[] = "Blob";
const char kDescription[] = "Threshold, morphology and select steps, then judge by count and area";

std::string stepKey(int index, const char* suffix)
{
    return std::string("step_") + std::to_string(static_cast<long long>(index)) + "_" + suffix;
}

std::vector<Vision::ParamField> fields()
{
    std::vector<Vision::ParamField> f;
    const std::string p = stepKey(0, "");
    f.push_back(Vision::paramInt(Vision::BlobKey::stepCount, "Step count", 1, true, 1, true, 16));
    f.push_back(Vision::paramString((p + Vision::BlobKey::type).c_str(), "Step type", Vision::BlobKey::stepThresh,
        Vision::paramChoices(Vision::BlobKey::stepThresh, Vision::BlobKey::stepMorph, Vision::BlobKey::stepSelect)));
    f.push_back(Vision::paramString((p + Vision::BlobKey::threshMode).c_str(), "Threshold mode", Vision::BlobKey::absolute,
        Vision::paramChoices(Vision::BlobKey::absolute, Vision::BlobKey::relative, Vision::BlobKey::dynamic)));
    f.push_back(Vision::paramInt((p + Vision::BlobKey::absMin).c_str(), "Absolute min", 80, true, 0, true, 255));
    f.push_back(Vision::paramInt((p + Vision::BlobKey::absMax).c_str(), "Absolute max", 255, true, 0, true, 255));
    f.push_back(Vision::paramDouble((p + Vision::BlobKey::relOffset).c_str(), "Relative offset", 20.0, false, 0, false, 0));
    f.push_back(Vision::paramDouble((p + Vision::BlobKey::dynOffset).c_str(), "Dynamic offset", 15.0, false, 0, false, 0));
    f.push_back(Vision::paramInt((p + Vision::BlobKey::dynKernel).c_str(), "Kernel size", 31, true, 3, true, 255));
    f.push_back(Vision::paramBool((p + Vision::BlobKey::dark).c_str(), "Dark blob", false));
    f.push_back(Vision::paramString((p + Vision::BlobKey::morphOp).c_str(), "Morphology", Vision::BlobKey::morphOpen,
        Vision::paramChoices(Vision::BlobKey::morphOpen, Vision::BlobKey::morphClose, Vision::BlobKey::morphErode, Vision::BlobKey::morphDilate)));
    f.push_back(Vision::paramDouble((p + Vision::BlobKey::radius).c_str(), "Radius", 1.0, true, 0.0, false, 0));
    f.push_back(Vision::paramString((p + Vision::BlobKey::feature).c_str(), "Feature", Vision::BlobKey::featArea,
        Vision::paramChoices(Vision::BlobKey::featArea, Vision::BlobKey::featCirc)));
    f.push_back(Vision::paramDouble((p + Vision::BlobKey::selMin).c_str(), "Select min", 50.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble((p + Vision::BlobKey::selMax).c_str(), "Select max", 0.0, true, 0.0, false, 0));
    f.push_back(Vision::paramInt((p + Vision::BlobKey::roiIndex).c_str(), "ROI index", -1, true, -1, true, 49));
    f.push_back(Vision::paramInt(Vision::BlobKey::okMinCount, "Min count", 0, true, 0, false, 0));
    f.push_back(Vision::paramInt(Vision::BlobKey::okMaxCount, "Max count", 0, true, 0, false, 0));
    f.push_back(Vision::paramDouble(Vision::BlobKey::okMinArea, "Min area", 0.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble(Vision::BlobKey::okMaxArea, "Max area", 0.0, true, 0.0, false, 0));
    return f;
}

Vision::ToolCaps caps()
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = Vision::BlobKey::roi;
    slot.title = "Search region";
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

std::string Vision::BlobTool::toolName() const
{
    return kToolName;
}

Vision::ToolType Vision::BlobTool::type() const
{
    return Vision::ToolType::Detect;
}

std::string Vision::BlobTool::displayName() const
{
    return kDisplayName;
}

std::string Vision::BlobTool::version() const
{
    return std::string(kToolVersion);
}

std::vector<Vision::VerNote> Vision::BlobTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

std::string Vision::BlobTool::description() const
{
    return kDescription;
}

Vision::ParamMap Vision::BlobTool::defaultParams() const
{
    return defaultsFrom(fields());
}

std::vector<Vision::ParamField> Vision::BlobTool::paramFields() const
{
    return fields();
}

Vision::ToolCaps Vision::BlobTool::capabilities() const
{
    return caps();
}

Vision::ToolOut Vision::BlobTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out = Vision::runAlgToolBase(toolName(), in, false);
    if (!Vision::toolTypeIsImageSource(type())) {
        out.io.images.clear();
    }
    if (in.input.isNull() && out.image.isNull()) {
        out.ok = false;
    }
    return out;
}

Vision::ToolBase* Vision::BlobTool::clone() const
{
    BlobTool* p = new BlobTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::BlobTool::toolParam() const
{
    return &m_param;
}
