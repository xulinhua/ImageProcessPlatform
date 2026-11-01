/**
 * @file filter_tool.cpp
 * @brief 图像滤波：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FilterTool.h"
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
const char kToolName[] = "preprocess.filter";
// 界面显示名
const char kDisplayName[] = "Image filter";
// 工具说明
const char kDescription[] = "Ordered preprocessing steps with enhancement, smoothing and gray morphology";

// 参数说明
}

// 参数
std::vector<Vision::ParamField> Vision::FilterTool::param_fields(int stepCount)
{
    std::vector<Vision::ParamField> f;

    int steps = stepCount;
    if (steps < 1) {
        steps = 1;
    }
    if (steps > 16) {
        steps = 16;
    }

    f.push_back(Vision::paramInt(FilterKey::stepCount, "Step count", steps, true, 1, true, 16));
    f.back().liveAction = FilterKey::previewFilter;

    const char* operatorNames[] = {
        FilterKey::opEquHisto,
        FilterKey::opEmphasize,
        FilterKey::opIlluminate,
        FilterKey::opScale,
        FilterKey::opInvert,
        FilterKey::opMean,
        FilterKey::opMedian,
        FilterKey::opGauss,
        FilterKey::opBilateral,
        FilterKey::opGrayOpen,
        FilterKey::opGrayClose,
        FilterKey::opGrayErode,
        FilterKey::opGrayDilate,
        FilterKey::opTopHat,
        FilterKey::opBottomHat,
    };
    const int operatorCount = static_cast<int>(sizeof(operatorNames) / sizeof(operatorNames[0]));

    const char* widthOps[] = {
        FilterKey::opEmphasize, FilterKey::opIlluminate, FilterKey::opMean, FilterKey::opMedian,
        FilterKey::opGauss, FilterKey::opGrayOpen, FilterKey::opGrayClose,
        FilterKey::opGrayErode, FilterKey::opGrayDilate, FilterKey::opTopHat, FilterKey::opBottomHat,
    };
    const char* heightOps[] = {
        FilterKey::opEmphasize, FilterKey::opIlluminate, FilterKey::opMean,
    };
    const char* c0Ops[] = {
        FilterKey::opEmphasize, FilterKey::opIlluminate, FilterKey::opScale,
        FilterKey::opBilateral, FilterKey::opGrayOpen, FilterKey::opGrayClose,
        FilterKey::opGrayErode, FilterKey::opGrayDilate, FilterKey::opTopHat, FilterKey::opBottomHat,
    };
    const char* c1Ops[] = {
        FilterKey::opScale, FilterKey::opBilateral,
    };
    const char* scaleOps[] = {
        FilterKey::opScale,
    };
    const char* shapeOps[] = {
        FilterKey::opGrayOpen, FilterKey::opGrayClose, FilterKey::opGrayErode,
        FilterKey::opGrayDilate, FilterKey::opTopHat, FilterKey::opBottomHat,
    };

    for (int step = 0; step < steps; ++step) {
        const std::string prefix = std::string("step_") + std::to_string(static_cast<long long>(step)) + "_";        const std::string opKey = prefix + FilterKey::op;

        f.push_back(Vision::paramString(opKey.c_str(), "Operator", FilterKey::opEquHisto, {}));
        {
            Vision::ParamField& op = f.back();
            for (int i = 0; i < operatorCount; ++i) {
                op.choices.push_back(operatorNames[i]);
            }
            op.liveAction = FilterKey::previewFilter;
        }

        f.push_back(Vision::paramInt((prefix + FilterKey::w).c_str(), "Mask width", 7, true, 1, true, 101));
        f.push_back(Vision::paramInt((prefix + FilterKey::h).c_str(), "Mask height", 7, true, 1, true, 101));
        f.push_back(Vision::paramDouble((prefix + FilterKey::c0).c_str(), "Param 1", 1.0, false, 0, false, 0));
        f.push_back(Vision::paramDouble((prefix + FilterKey::c1).c_str(), "Param 2", 3.0, false, 0, false, 0));
        f.push_back(Vision::paramBool((prefix + FilterKey::scaleMax).c_str(), "Scale to max", false));
        f.push_back(Vision::paramString((prefix + FilterKey::shape).c_str(), "Mask shape", FilterKey::shapeRect, {}));
        f.push_back(Vision::paramInt((prefix + FilterKey::roiIndex).c_str(), "ROI index", -1,
                                     true, -1, true, 49));

        for (size_t i = f.size() - 7; i < f.size(); ++i) {
            Vision::ParamField& item = f[i];
            item.showKey = opKey;
            item.liveAction = FilterKey::previewFilter;

            const char* const* ops = nullptr;
            size_t count = 0;

            if (item.key == prefix + FilterKey::w) {
                ops = widthOps;
                count = sizeof(widthOps) / sizeof(widthOps[0]);
            }
            else if (item.key == prefix + FilterKey::h) {
                ops = heightOps;
                count = sizeof(heightOps) / sizeof(heightOps[0]);
            }
            else if (item.key == prefix + FilterKey::c0) {
                ops = c0Ops;
                count = sizeof(c0Ops) / sizeof(c0Ops[0]);
            }
            else if (item.key == prefix + FilterKey::c1) {
                ops = c1Ops;
                count = sizeof(c1Ops) / sizeof(c1Ops[0]);
            }
            else if (item.key == prefix + FilterKey::scaleMax) {
                ops = scaleOps;
                count = sizeof(scaleOps) / sizeof(scaleOps[0]);
            }
            else if (item.key == prefix + FilterKey::shape) {
                ops = shapeOps;
                count = sizeof(shapeOps) / sizeof(shapeOps[0]);
            }
            else if (item.key == prefix + FilterKey::roiIndex) {
                // roiIndex 对所有算子都显示，不需要 showValues 过滤
                continue;
            }

            for (size_t j = 0; j < count; ++j) {
                item.showValues.push_back(ops[j]);
            }
        }

        Vision::ParamField& shape = f.back();
        shape.choices = {
            FilterKey::shapeRect,
            FilterKey::shapeOctagon,
            FilterKey::shapeRhombus,
        };
    }

    return f;
}
// 搜索区
Vision::ToolCaps Vision::FilterTool::roi_caps()
{
    Vision::ToolCaps c;
    Vision::RoiSlotRule slot;
    slot.slot = FilterKey::roi;
    slot.title = "Filter ROI";
    slot.maxCount = 50; // 支持逐步按索引绑定 ROI

    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
    slot.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjPoly, 0));

    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::DrawRegion);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRoi);
    slot.itemKinds.push_back(Vision::RoiItemKind::RefRegion);

    c.slotRules.push_back(slot);
    return c;
}


// 注册名
// 骨架
Vision::FilterTool::FilterTool()
{
}

Vision::FilterTool::~FilterTool()
{
}

// 注册名
std::string Vision::FilterTool::tool_name() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::FilterTool::type() const
{
    return Vision::ToolType::Preprocess;
}

// 界面显示名
std::string Vision::FilterTool::display_name() const
{
    return kDisplayName;
}

// 工具版本
std::string Vision::FilterTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::FilterTool::version_notes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::FilterTool::description() const
{
    return kDescription;
}

// 缺省参数
Vision::ParamMap Vision::FilterTool::default_params() const
{
    return defaultsFrom(param_fields(1));
}

// 参数说明
std::vector<Vision::ParamField> Vision::FilterTool::cfg_param() const
{
    return param_fields(1);
}

// 按当前方案生成参数说明
std::vector<Vision::ParamField> Vision::FilterTool::cfg_param(const Vision::ToolParam& current) const
{
    int steps = current.run.value(FilterKey::stepCount).toInt(1);
    if (steps < 1) {
        steps = 1;
    }
    if (steps > 16) {
        steps = 16;
    }
    return param_fields(steps);
}

Vision::OutputSpec Vision::FilterTool::cfg_output() const
{
    Vision::OutputSpec spec;

    Vision::OutputField width;
    width.key = FilterKey::outWidth;
    width.title = "Width";
    width.unit = "px";
    width.kind = Vision::OutputKind::Number;
    spec.fields.push_back(width);

    Vision::OutputField height;
    height.key = FilterKey::outHeight;
    height.title = "Height";
    height.unit = "px";
    height.kind = Vision::OutputKind::Number;
    spec.fields.push_back(height);

    Vision::OutputField cost;
    cost.key = FilterKey::outCostMs;
    cost.title = "Cost";
    cost.unit = "ms";
    cost.kind = Vision::OutputKind::Number;
    spec.fields.push_back(cost);

    return spec;
}

// 端口表。在 Preprocess 默认的 Out Image 后追加 Out Region
std::vector<Vision::PortDesc> Vision::FilterTool::cfg_port() const
{
    std::vector<Vision::PortDesc> p = Vision::ToolBase::cfg_port();
    Vision::PortDesc region(Vision::PortDir::Out, Vision::PortData::Region);
    region.slot.name = FilterKey::roi;
    p.push_back(region);
    return p;
}

// 画布能力
Vision::ToolCaps Vision::FilterTool::cfg_roi() const
{
    return roi_caps();
}


int Vision::FilterTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out);
}

std::vector<Vision::ToolAction> Vision::FilterTool::cfg_action() const
{
    std::vector<Vision::ToolAction> actions;

    Vision::ToolAction preview;
    preview.id = FilterKey::previewFilter;
    preview.title = "Filter";
    preview.place = Vision::ActionPlace::Run;
    preview.live = true;
    actions.push_back(preview);

    return actions;
}

int Vision::FilterTool::action(
    const std::string& id,
    const Vision::ToolIn& in,
    Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }

    if (id != FilterKey::previewFilter) {
        return Vision::ToolBase::action(id, in, out);
    }
    return shot(in, id, out);
}

Vision::ToolBase* Vision::FilterTool::clone() const
{
    FilterTool* p = new FilterTool();
    p->copyInstanceDataFrom(*this);
    return p;
}

const Vision::ToolParaBase* Vision::FilterTool::tool_param() const
{
    return &param_;
}

int Vision::FilterTool::check_param(std::string* detail) const
{
    if (param_.loadCode() != HS_OK) {
        if (detail != nullptr) {
            *detail = HsCommData::HsException(param_.loadCode()).errorMessage();
        }
        return param_.loadCode();
    }
    return HS_OK;
}

void Vision::FilterTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    build_alg_in(in, ain, FilterKey::roi);
    ain->param = &param_;
}

int Vision::FilterTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
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

Vision::ToolOut Vision::FilterTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
