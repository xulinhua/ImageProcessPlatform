/**
 * @file texture_tool.cpp
 * @brief 纹理缺陷：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TextureTool.h"
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
const char kToolName[] = "detect.texture";
// 界面显示名
const char kDisplayName[] = "纹理缺陷";
// 工具说明
const char kDescription[] = "Defects on a regular texture: large-kernel background difference plus morphology";

// 参数说明
}

// 参数
std::vector<Vision::ParamField> Vision::TextureTool::param_fields()
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramString(TextureKey::bgMethod, "Background", TextureKey::mean, Vision::paramChoices(TextureKey::mean, "median", "gauss")));
    f.push_back(Vision::paramInt(TextureKey::kernelW, "Kernel width", 31, true, 3, true, 255));
    f.push_back(Vision::paramInt(TextureKey::kernelH, "Kernel height", 31, true, 3, true, 255));
    f.push_back(Vision::paramDouble(TextureKey::residThresh, "Residual threshold", 18.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble(TextureKey::openR, "Opening", 1.5, true, 0.0, false, 0));
    f.push_back(Vision::paramInt(TextureKey::minArea, "Min area", 30, true, 0, false, 0));
    f.push_back(Vision::paramInt(TextureKey::maxArea, "Max area", 0, true, 0, false, 0));
    return f;
}
// 搜索区
Vision::ToolCaps Vision::TextureTool::roi_caps()
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
Vision::TextureTool::TextureTool()
{
}

Vision::TextureTool::~TextureTool()
{
}

// 注册名
std::string Vision::TextureTool::tool_name() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::TextureTool::type() const
{
    return Vision::ToolType::Detect;
}

// 界面显示名
std::string Vision::TextureTool::display_name() const
{
    return kDisplayName;
}

// 工具版本
std::string Vision::TextureTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::TextureTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::TextureTool::description() const
{
    return kDescription;
}

// 缺省参数
Vision::ParamMap Vision::TextureTool::default_params() const
{
    return defaultsFrom(param_fields());
}

// 参数说明
std::vector<Vision::ParamField> Vision::TextureTool::cfg_param() const
{
    return param_fields();
}

// 画布能力
Vision::ToolCaps Vision::TextureTool::cfg_roi() const
{
    return roi_caps();
}


int Vision::TextureTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

Vision::ToolBase* Vision::TextureTool::clone() const
{
    TextureTool* p = new TextureTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::TextureTool::tool_param() const
{
    return &param_;
}

int Vision::TextureTool::check_param(std::string* detail) const
{
    if (param_.loadCode() != HS_OK) {
        if (detail != nullptr) {
            *detail = HsCommData::HsException(param_.loadCode()).errorMessage();
        }
        return param_.loadCode();
    }
    return HS_OK;
}

void Vision::TextureTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    build_alg_in(in, ain, "roi");
    ain->param = &param_;
}

int Vision::TextureTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
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

Vision::ToolOut Vision::TextureTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
