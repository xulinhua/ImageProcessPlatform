/**
 * @file SmoothTool.cpp
 * @brief 图像平滑：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SmoothTool.h"
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
const char kToolName[] = "preprocess.smooth";
const char kDisplayName[] = "图像平滑";
const char kDescription[] = "可叠加均值、中值、高斯、双边滤波";
}

std::vector<Vision::ParamField> Vision::SmoothTool::param_fields()
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

Vision::ToolCaps Vision::SmoothTool::roi_caps()
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

Vision::SmoothTool::SmoothTool() {}
Vision::SmoothTool::~SmoothTool() {}

std::string Vision::SmoothTool::tool_name() const { return kToolName; }
Vision::ToolType Vision::SmoothTool::type() const { return Vision::ToolType::Preprocess; }
std::string Vision::SmoothTool::display_name() const { return kDisplayName; }
std::string Vision::SmoothTool::description() const { return kDescription; }
std::string Vision::SmoothTool::version() const { return kToolVersion; }
std::vector<Vision::VerNote> Vision::SmoothTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

Vision::ParamMap Vision::SmoothTool::default_params() const { return defaultsFrom(param_fields()); }
std::vector<Vision::ParamField> Vision::SmoothTool::cfg_param() const { return param_fields(); }
Vision::ToolCaps Vision::SmoothTool::cfg_roi() const { return roi_caps(); }

int Vision::SmoothTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

Vision::ToolBase* Vision::SmoothTool::clone() const
{
    SmoothTool* p = new SmoothTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::SmoothTool::tool_param() const { return &param_; }

int Vision::SmoothTool::check_param(std::string*) const
{
    return HS_OK;
}

void Vision::SmoothTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    build_alg_in(in, ain, "roi");
    ain->param = &param_;
}

int Vision::SmoothTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
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

Vision::ToolOut Vision::SmoothTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
