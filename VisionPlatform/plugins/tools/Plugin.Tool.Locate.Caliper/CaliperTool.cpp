/**
 * @file CaliperTool.cpp
 * @brief CaliperTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CaliperTool.h"
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
const char kToolName[] = "locate.caliper";
// 界面显示名
const char kDisplayName[] = "卡尺";
// 工具说明
const char kDescription[] = "测宽或找边，支持多卡尺 ROI";

// 参数说明
std::vector<Vision::ParamField> fields()
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString("mode", "模式", "width", Vision::paramChoices("width", "edge")));
    f.push_back(Vision::paramDouble("sigma", "平滑", 1.0, true, 0.4, true, 20.0));
    f.push_back(Vision::paramDouble("threshold", "阈值", 20.0, true, 0.0, false, 0));
    f.push_back(Vision::paramString("transition", "极性", "all", Vision::paramChoices("all", "positive", "negative")));
    f.push_back(Vision::paramString("select", "选择", "all", Vision::paramChoices("all", "first", "last")));
    f.push_back(Vision::paramInt("searchCount", "查找条数", 1, true, 1, true, 50));
    return f;
}
Vision::ToolCaps caps()
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = "roi";
    slot.maxCount = 50;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjLine, 0));
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
std::string Vision::CaliperTool::toolName() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::CaliperTool::type() const
{
    return Vision::ToolType::Locate;
}

// 界面显示名
std::string Vision::CaliperTool::displayName() const
{
    return kDisplayName;
}

// 工具版本
std::string Vision::CaliperTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::CaliperTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::CaliperTool::description() const
{
    return kDescription;
}

// 缺省参数
Vision::ParamMap Vision::CaliperTool::defaultParams() const
{
    return defaultsFrom(fields());
}

// 参数说明
std::vector<Vision::ParamField> Vision::CaliperTool::paramFields() const
{
    return fields();
}

// 画布能力
Vision::ToolCaps Vision::CaliperTool::capabilities() const
{
    return caps();
}

// 调算法插件执行
Vision::ToolOut Vision::CaliperTool::execute(const Vision::ToolIn& in)
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
Vision::ToolBase* Vision::CaliperTool::clone() const
{
    CaliperTool* p = new CaliperTool();
    p->copyInstanceDataFrom(*this);
    return p;
}// 返回本工具参数对象

const Vision::ToolParaBase* Vision::CaliperTool::toolParam() const
{
    return &m_param;
}
