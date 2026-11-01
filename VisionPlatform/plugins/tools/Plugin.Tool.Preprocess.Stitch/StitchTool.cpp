/**
 * @file stitch_tool.cpp
 * @brief 图像拼接：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StitchTool.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <vector>

namespace {
// 当前版本。必须等于变更表最后一行
const char kToolVersion[] = "1.0.0.0";
// 变更表。只追加，不删旧行
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "Initial version" },
};
// 注册名
const char kToolName[] = "preprocess.stitch";
// 界面显示名
const char kDisplayName[] = "Image stitch";
// 工具说明
const char kDescription[] = "Tile upstream images into one image. columns is the count per row";

// 参数说明
}

// 参数
std::vector<Vision::ParamField> Vision::StitchTool::param_fields()
{
    std::vector<Vision::ParamField> list;
    list.push_back(Vision::paramInt(StitchKey::columns, "每行张数", 2, true, 1, true, 32));
    return list;
}

// 骨架
Vision::StitchTool::StitchTool()
{
}

Vision::StitchTool::~StitchTool()
{
}

// 注册名
std::string Vision::StitchTool::tool_name() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::StitchTool::type() const
{
    return Vision::ToolType::Preprocess;
}

// 界面显示名
std::string Vision::StitchTool::display_name() const
{
    return kDisplayName;
}

// 工具版本
std::string Vision::StitchTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::StitchTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

// 工具说明
std::string Vision::StitchTool::description() const
{
    return kDescription;
}

// 缺省参数
Vision::ParamMap Vision::StitchTool::default_params() const
{
    return defaultsFrom(param_fields());
}

// 参数说明
std::vector<Vision::ParamField> Vision::StitchTool::cfg_param() const
{
    return param_fields();
}

// 画布能力
Vision::ToolCaps Vision::StitchTool::cfg_roi() const
{
    return Vision::ToolCaps();
}

int Vision::StitchTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

Vision::ToolBase* Vision::StitchTool::clone() const
{
    StitchTool* copy = new StitchTool();
    copy->copyInstanceDataFrom(*this);
    return copy;
}

const Vision::ToolParaBase* Vision::StitchTool::tool_param() const
{
    return &param_;
}

int Vision::StitchTool::check_param(std::string* detail) const
{
    if (param_.loadCode() != HS_OK) {
        if (detail != nullptr) {
            *detail = HsCommData::HsException(param_.loadCode()).errorMessage();
        }
        return param_.loadCode();
    }
    return HS_OK;
}

void Vision::StitchTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    build_alg_in(in, ain);
    ain->param = &param_;
}

int Vision::StitchTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
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
    return run_attached_shot(in, actionId, &ain, true, out);
}

int Vision::StitchTool::save()
{
    return ToolBase::save();
}

void Vision::StitchTool::clear(const std::string& toolDataDir)
{
    ToolBase::clear(toolDataDir);
}

Vision::ToolOut Vision::StitchTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
