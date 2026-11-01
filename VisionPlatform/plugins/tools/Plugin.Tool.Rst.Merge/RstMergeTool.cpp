/**
 * @file rst_merge_tool.cpp
 * @brief 区域合并：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RstMergeTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
// 当前版本。必须等于变更表最后一行
const char kToolVersion[] = "1.0.0.0";
// 变更表。只追加，不删旧行
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};

// 注册名
const char kToolName[] = "result.merge";
// 界面显示名
const char kDisplayName[] = "Merge regions";
// 工具说明
const char kDescription[] = "Merge defect regions from upstream tools and total the area and count";

// 参数说明
}

// 参数
std::vector<Vision::ParamField> Vision::RstMergeTool::param_fields()
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDouble(RstMergeKey::minArea, "Min area", 0.0, true, 0.0, false, 0));
    f.push_back(Vision::paramInt(RstMergeKey::maxCount, "Max count", 0, true, 0, false, 0));
    return f;
}
// 搜索区
Vision::ToolCaps Vision::RstMergeTool::roi_caps()
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


// 注册名
// 骨架
Vision::RstMergeTool::RstMergeTool()
{
}

Vision::RstMergeTool::~RstMergeTool()
{
}

// 注册名
std::string Vision::RstMergeTool::tool_name() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::RstMergeTool::type() const
{
    return Vision::ToolType::RstProcess;
}

// 界面显示名
std::string Vision::RstMergeTool::display_name() const
{
    return kDisplayName;
}

// 工具版本
std::string Vision::RstMergeTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::RstMergeTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::RstMergeTool::description() const
{
    return kDescription;
}

// 缺省参数
Vision::ParamMap Vision::RstMergeTool::default_params() const
{
    return defaultsFrom(param_fields());
}

// 参数说明
std::vector<Vision::ParamField> Vision::RstMergeTool::cfg_param() const
{
    return param_fields();
}

// 画布能力
Vision::ToolCaps Vision::RstMergeTool::cfg_roi() const
{
    return roi_caps();
}


int Vision::RstMergeTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

Vision::ToolBase* Vision::RstMergeTool::clone() const
{
    RstMergeTool* p = new RstMergeTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::RstMergeTool::tool_param() const
{
    return &param_;
}

int Vision::RstMergeTool::check_param(std::string* detail) const
{
    if (param_.loadCode() != HS_OK) {
        if (detail != nullptr) {
            *detail = HsCommData::HsException(param_.loadCode()).errorMessage();
        }
        return param_.loadCode();
    }
    return HS_OK;
}

void Vision::RstMergeTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    build_alg_in(in, ain, "roi");
    ain->param = &param_;
}

int Vision::RstMergeTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = Vision::ToolOut();
    attachAlg(in.toolDataDir);
    param_.load(in.param);
    if (param_.loadCode() != HS_OK) {
        out->ok = false;
        out->errCode = param_.loadCode();
        out->message = HsCommData::HsException(param_.loadCode()).errorMessage();
        return out->errCode;
    }
    Vision::AlgToolIn ain;
    fill_alg_in(in, &ain);
    const int code = run_attached_shot(in, actionId, &ain, false, out);
    if (!Vision::toolTypeIsImageSource(type())) {
        out->io.images.clear();
    }
    return code;
}

Vision::ToolOut Vision::RstMergeTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
