/**
 * @file TextureTool.cpp
 * @brief TextureTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TextureTool.h"
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
const char kToolName[] = "detect.texture";
// 界面显示名
const char kDisplayName[] = "纹理缺陷";
// 工具说明
const char kDescription[] = "规则纹理下破损脏污：大核背景差加形态学";

// 参数说明
std::vector<Vision::ParamField> fields()
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString("bgMethod", "背景", "mean", Vision::paramChoices("mean", "median", "gauss")));
    f.push_back(Vision::paramInt("kernelW", "核宽", 31, true, 3, true, 255));
    f.push_back(Vision::paramInt("kernelH", "核高", 31, true, 3, true, 255));
    f.push_back(Vision::paramDouble("residThresh", "残差阈值", 18.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble("openR", "开运算", 1.5, true, 0.0, false, 0));
    f.push_back(Vision::paramInt("minArea", "最小面积", 30, true, 0, false, 0));
    f.push_back(Vision::paramInt("maxArea", "最大面积", 0, true, 0, false, 0));
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
std::string Vision::TextureTool::toolName() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::TextureTool::type() const
{
    return Vision::ToolType::Detect;
}

// 界面显示名
std::string Vision::TextureTool::displayName() const
{
    return Vision::acpToUtf8(kDisplayName);
}

// 工具版本
std::string Vision::TextureTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::TextureTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::TextureTool::description() const
{
    return Vision::acpToUtf8(kDescription);
}

// 缺省参数
Vision::ParamMap Vision::TextureTool::defaultParams() const
{
    return defaultsFrom(fields());
}

// 参数说明
std::vector<Vision::ParamField> Vision::TextureTool::paramFields() const
{
    return fields();
}

// 画布能力
Vision::ToolCaps Vision::TextureTool::capabilities() const
{
    return caps();
}

// 调算法插件执行
Vision::ToolOut Vision::TextureTool::execute(const Vision::ToolIn& in)
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
Vision::ToolBase* Vision::TextureTool::clone() const
{
    TextureTool* p = new TextureTool();
    p->copyInstanceDataFrom(*this);
    return p;
}// 返回本工具参数对象

const Vision::ToolParaBase* Vision::TextureTool::toolParam() const
{
    return &m_param;
}
