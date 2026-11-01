/**
 * @file EnhanceTool.cpp
 * @brief 图像增强：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "EnhanceTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/Graphic.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};
const char kToolName[] = "preprocess.enhance";
const char kDisplayName[] = "图像增强";
const char kDescription[] = "可叠加直方图均衡、强调、照度、缩放、反色";
}

std::vector<Vision::ParamField> Vision::EnhanceTool::param_fields()
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramInt("pl_n", "步骤数", 1, true, 0, true, 8));
    f.push_back(Vision::paramInt("pl_0_op", "算子", 0, true, 0, true, 4));
    f.push_back(Vision::paramInt("pl_0_w", "掩膜宽", 7, true, 1, true, 255));
    f.push_back(Vision::paramInt("pl_0_h", "掩膜高", 7, true, 1, true, 255));
    f.push_back(Vision::paramDouble("pl_0_c0", "系数", 1.0, false, 0, false, 0));
    f.push_back(Vision::paramDouble("pl_0_c1", "系数2", 0.0, false, 0, false, 0));
    f.push_back(Vision::paramInt("pl_0_f", "缩放到最大", 0, true, 0, true, 1));
    return f;
}

Vision::ToolCaps Vision::EnhanceTool::roi_caps()
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

Vision::EnhanceTool::EnhanceTool() {}
Vision::EnhanceTool::~EnhanceTool() {}

std::string Vision::EnhanceTool::tool_name() const { return kToolName; }
Vision::ToolType Vision::EnhanceTool::type() const { return Vision::ToolType::Preprocess; }
std::string Vision::EnhanceTool::display_name() const { return kDisplayName; }
std::string Vision::EnhanceTool::description() const { return kDescription; }
std::string Vision::EnhanceTool::version() const { return kToolVersion; }
std::vector<Vision::VerNote> Vision::EnhanceTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

Vision::ParamMap Vision::EnhanceTool::default_params() const { return defaultsFrom(param_fields()); }
std::vector<Vision::ParamField> Vision::EnhanceTool::cfg_param() const { return param_fields(); }
Vision::ToolCaps Vision::EnhanceTool::cfg_roi() const { return roi_caps(); }

int Vision::EnhanceTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

Vision::ToolBase* Vision::EnhanceTool::clone() const
{
    EnhanceTool* p = new EnhanceTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::EnhanceTool::tool_param() const { return &param_; }

int Vision::EnhanceTool::check_param(std::string*) const
{
    return HS_OK;
}

void Vision::EnhanceTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    build_alg_in(in, ain, "roi");
    ain->param = &param_;
}

int Vision::EnhanceTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = Vision::ToolOut();
    attachAlg(in.toolDataDir);
    param_.load(in.param);
    Vision::AlgToolIn ain;
    fill_alg_in(in, &ain);
    return run_attached_shot(in, actionId, &ain, true, out);
}

Vision::ToolOut Vision::EnhanceTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
