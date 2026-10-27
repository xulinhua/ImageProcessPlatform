/**
 * @file TmplTool.cpp
 * @brief TmplTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TmplTool.h"
#include "AlgToolBase.h"
#include "TextEncoding.h"
#include "VisionVersion.h"
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
// 当前版本。必须等于变更表最后一行
const char kToolVersion[] = "1.0.0.0";
// 变更表。只追加，不删旧行
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};

// 注册名
const char kToolName[] = "locate.template";
// 界面显示名
const char kDisplayName[] = "模板定位";
// 工具说明
const char kDescription[] = "NCC/形状模板定位；模型存方案旁 .vpdata，输出位姿";

// 参数说明
std::vector<Vision::ParamField> fields()
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString("method", "匹配方式", "ncc", Vision::paramChoices("ncc", "shape")));
    f.push_back(Vision::paramDouble("minScore", "最低分", 0.5, true, 0.0, true, 1.0));
    f.push_back(Vision::paramDouble("angleRange", "角度范围", 15.0, true, 0.0, true, 180.0));
    f.push_back(Vision::paramInt("numMatches", "匹配个数", 1, true, 1, true, 50));
    f.push_back(Vision::paramPath("modelFile", "模型文件", "model.ncm"));
    f.push_back(Vision::paramBool("rebuildModel", "强制重建", false));
    return f;
}
Vision::ToolCaps caps()
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = "roi";
    slot.maxCount = 1;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    c.slotRules.push_back(slot);
    return c;
}

// 由参数说明生成缺省 ParamMap
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

// 注册名
std::string Vision::TmplTool::toolName() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::TmplTool::type() const
{
    return Vision::ToolType::Locate;
}

// 界面显示名
std::string Vision::TmplTool::displayName() const
{
    return Vision::acpToUtf8(kDisplayName);
}

// 工具版本
std::string Vision::TmplTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::TmplTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::TmplTool::description() const
{
    return Vision::acpToUtf8(kDescription);
}

// 缺省参数
Vision::ParamMap Vision::TmplTool::defaultParams() const
{
    return defaultsFrom(fields());
}

// 参数说明
std::vector<Vision::ParamField> Vision::TmplTool::paramFields() const
{
    return fields();
}

// 画布能力
Vision::ToolCaps Vision::TmplTool::capabilities() const
{
    return caps();
}

// 调算法插件执行
Vision::ToolOut Vision::TmplTool::execute(const Vision::ToolIn& in)
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

// 克隆本实例
Vision::ToolBase* Vision::TmplTool::clone() const
{
    TmplTool* p = new TmplTool();
    p->copyInstanceDataFrom(*this);
    return p;
}// 返回本工具参数对象

const Vision::ToolParaBase* Vision::TmplTool::toolParam() const
{
    return &m_param;
}
