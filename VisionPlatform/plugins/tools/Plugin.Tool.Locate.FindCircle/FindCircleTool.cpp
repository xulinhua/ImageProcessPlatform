/**
 * @file FindCircleTool.cpp
 * @brief FindCircleTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FindCircleTool.h"
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
const char kToolName[] = "locate.find_circle";
// 界面显示名
const char kDisplayName[] = "找圆";
// 工具说明
const char kDescription[] = "在 ROI 内找圆，输出圆心、半径与叠加图形";

// 参数说明
std::vector<Vision::ParamField> fields()
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramInt("threshold", "阈值", 80, true, 0, true, 255));
    f.push_back(Vision::paramInt("minR", "最小半径", 8, true, 1, false, 0));
    f.push_back(Vision::paramInt("maxR", "最大半径", 400, true, 1, false, 0));
    f.push_back(Vision::paramBool("dark", "暗目标", true));
    f.push_back(Vision::paramDouble("sigma", "平滑", 1.0, true, 0.4, true, 20.0));
    f.push_back(Vision::paramInt("low", "低阈值", 20, true, 0, true, 255));
    f.push_back(Vision::paramInt("high", "高阈值", 40, true, 0, true, 255));
    return f;
}
Vision::ToolCaps caps()
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = "roi";
    slot.maxCount = 1;
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 1));
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
std::string Vision::FindCircleTool::toolName() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::FindCircleTool::type() const
{
    return Vision::ToolType::Locate;
}

// 界面显示名
std::string Vision::FindCircleTool::displayName() const
{
    return Vision::acpToUtf8(kDisplayName);
}

// 工具版本
std::string Vision::FindCircleTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::FindCircleTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::FindCircleTool::description() const
{
    return Vision::acpToUtf8(kDescription);
}

// 缺省参数
Vision::ParamMap Vision::FindCircleTool::defaultParams() const
{
    return defaultsFrom(fields());
}

// 参数说明
std::vector<Vision::ParamField> Vision::FindCircleTool::paramFields() const
{
    return fields();
}

// 画布能力
Vision::ToolCaps Vision::FindCircleTool::capabilities() const
{
    return caps();
}

// 调算法插件执行
Vision::ToolOut Vision::FindCircleTool::execute(const Vision::ToolIn& in)
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
Vision::ToolBase* Vision::FindCircleTool::clone() const
{
    FindCircleTool* p = new FindCircleTool();
    p->copyInstanceDataFrom(*this);
    return p;
}// 返回本工具参数对象

const Vision::ToolParaBase* Vision::FindCircleTool::toolParam() const
{
    return &m_param;
}
