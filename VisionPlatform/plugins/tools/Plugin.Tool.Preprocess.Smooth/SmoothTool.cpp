/**
 * @file SmoothTool.cpp
 * @brief SmoothTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SmoothTool.h"
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
const char kToolName[] = "preprocess.smooth";
// 界面显示名
const char kDisplayName[] = "图像平滑";
// 工具说明
const char kDescription[] = "可叠加均值、中值、高斯、双边滤波";

// 参数说明
std::vector<Vision::ParamField> fields()
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramInt("pl_n", "步骤数", 1, true, 0, true, 8));
    f.push_back(Vision::paramInt("pl_0_op", "算子", 1, true, 0, true, 3));
    f.push_back(Vision::paramInt("pl_0_w", "掩膜宽", 3, true, 1, true, 255));
    f.push_back(Vision::paramInt("pl_0_h", "掩膜高", 3, true, 1, true, 255));
    f.push_back(Vision::paramDouble("pl_0_c0", "灰度域", 20.0, false, 0, false, 0));
    f.push_back(Vision::paramDouble("pl_0_c1", "空间域", 3.0, false, 0, false, 0));
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
std::string Vision::SmoothTool::toolName() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::SmoothTool::type() const
{
    return Vision::ToolType::Preprocess;
}

// 界面显示名
std::string Vision::SmoothTool::displayName() const
{
    return kDisplayName;
}

// 工具版本
std::string Vision::SmoothTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::SmoothTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::SmoothTool::description() const
{
    return kDescription;
}

// 缺省参数
Vision::ParamMap Vision::SmoothTool::defaultParams() const
{
    return defaultsFrom(fields());
}

// 参数说明
std::vector<Vision::ParamField> Vision::SmoothTool::paramFields() const
{
    return fields();
}

// 画布能力
Vision::ToolCaps Vision::SmoothTool::capabilities() const
{
    return caps();
}

// 调算法插件执行
Vision::ToolOut Vision::SmoothTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out = Vision::runAlgToolBase(toolName(), in, true);
    if (!Vision::toolTypeIsImageSource(type())) {
        out.io.images.clear();
    }
    if (in.input.isNull() && out.image.isNull()) {
        out.ok = false;
    }
    return out;
}

// 克隆本实例
Vision::ToolBase* Vision::SmoothTool::clone() const
{
    SmoothTool* p = new SmoothTool();
    p->copyInstanceDataFrom(*this);
    return p;
}// 返回本工具参数对象

const Vision::ToolParaBase* Vision::SmoothTool::toolParam() const
{
    return &m_param;
}
