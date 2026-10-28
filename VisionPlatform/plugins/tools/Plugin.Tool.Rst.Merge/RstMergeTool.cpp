/**
 * @file RstMergeTool.cpp
 * @brief RstMergeTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RstMergeTool.h"
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
const char kToolName[] = "result.merge";
// 界面显示名
const char kDisplayName[] = "区域合并";
// 工具说明
const char kDescription[] = "合并上游多个工具的缺陷区域，统计面积与数量";

// 参数说明
std::vector<Vision::ParamField> fields()
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDouble("minArea", "最小面积", 0.0, true, 0.0, false, 0));
    f.push_back(Vision::paramInt("maxCount", "最大数量", 0, true, 0, false, 0));
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
std::string Vision::RstMergeTool::toolName() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::RstMergeTool::type() const
{
    return Vision::ToolType::RstProcess;
}

// 界面显示名
std::string Vision::RstMergeTool::displayName() const
{
    return kDisplayName;
}

// 工具版本
std::string Vision::RstMergeTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::RstMergeTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::RstMergeTool::description() const
{
    return kDescription;
}

// 缺省参数
Vision::ParamMap Vision::RstMergeTool::defaultParams() const
{
    return defaultsFrom(fields());
}

// 参数说明
std::vector<Vision::ParamField> Vision::RstMergeTool::paramFields() const
{
    return fields();
}

// 画布能力
Vision::ToolCaps Vision::RstMergeTool::capabilities() const
{
    return caps();
}

// 调算法插件执行
Vision::ToolOut Vision::RstMergeTool::execute(const Vision::ToolIn& in)
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
Vision::ToolBase* Vision::RstMergeTool::clone() const
{
    RstMergeTool* p = new RstMergeTool();
    p->copyInstanceDataFrom(*this);
    return p;
}// 返回本工具参数对象

const Vision::ToolParaBase* Vision::RstMergeTool::toolParam() const
{
    return &m_param;
}
