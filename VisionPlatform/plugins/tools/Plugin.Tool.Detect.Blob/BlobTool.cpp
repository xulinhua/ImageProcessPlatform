/**
 * @file BlobTool.cpp
 * @brief 斑点检测：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "BlobTool.h"
#include "VisionVersion.h"
#include <HsCommData/Graphic.h>
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <vector>

namespace {
const char kToolVersion[] = "1.0.0.0";
const Vision::VerNoteRaw kVerNotes[] = {
	{ "1.0.0.0", "首版" },
};
const char kToolName[] = "detect.blob";
const char kDisplayName[] = "Blob";
const char kDescription[] = "按顺序做阈值、形态学和筛选，再按数量与面积判定";
}

std::vector<Vision::ParamField> Vision::BlobTool::param_fields(int stepCount)
{
	std::vector<Vision::ParamField> f;
	int steps = stepCount;
	if (steps < 1) {
		steps = 1;
	}
	if (steps > 16) {
		steps = 16;
	}
	f.push_back(Vision::paramInt(BlobKey::stepCount, "步骤数", steps, true, 1, true, 16));
	f.back().liveAction = BlobKey::previewThresh;
	for (int step = 0; step < steps; ++step) {
		const std::string prefix = std::string("step_") + std::to_string(static_cast<long long>(step)) + "_";
		const std::string groupName = std::string("步骤 ") + std::to_string(static_cast<long long>(step + 1));
		const std::string typeKey = prefix + BlobKey::type;
		f.push_back(Vision::paramString(typeKey.c_str(), "步骤类型", BlobKey::stepThresh,
			Vision::paramChoices(BlobKey::stepThresh, BlobKey::stepMorph, BlobKey::stepSelect)));
		f.push_back(Vision::paramString((prefix + BlobKey::threshMode).c_str(), "阈值模式", BlobKey::absolute,
			Vision::paramChoices(BlobKey::absolute, BlobKey::relative, BlobKey::dynamic)));
		f.push_back(Vision::paramInt((prefix + BlobKey::absMin).c_str(), "绝对下限", 80, true, 0, true, 255));
		f.push_back(Vision::paramInt((prefix + BlobKey::absMax).c_str(), "绝对上限", 255, true, 0, true, 255));
		f.push_back(Vision::paramDouble((prefix + BlobKey::relOffset).c_str(), "相对偏移", 20.0, false, 0, false, 0));
		f.push_back(Vision::paramDouble((prefix + BlobKey::dynOffset).c_str(), "动态偏移", 15.0, false, 0, false, 0));
		f.push_back(Vision::paramInt((prefix + BlobKey::dynKernel).c_str(), "动态核", 31, true, 3, true, 255));
		f.push_back(Vision::paramBool((prefix + BlobKey::dark).c_str(), "暗目标", false));
		f.push_back(Vision::paramString((prefix + BlobKey::morphOp).c_str(), "形态学", BlobKey::morphOpen,
			Vision::paramChoices(BlobKey::morphOpen, BlobKey::morphClose, BlobKey::morphErode, BlobKey::morphDilate)));
		f.push_back(Vision::paramDouble((prefix + BlobKey::radius).c_str(), "半径", 1.0, true, 0.5, false, 0));
		f.push_back(Vision::paramString((prefix + BlobKey::feature).c_str(), "特征", BlobKey::featArea,
			Vision::paramChoices(BlobKey::featArea, BlobKey::featCirc)));
		f.push_back(Vision::paramDouble((prefix + BlobKey::selMin).c_str(), "下限", 50.0, false, 0, false, 0));
		f.push_back(Vision::paramDouble((prefix + BlobKey::selMax).c_str(), "上限", 0.0, false, 0, false, 0));
		f.push_back(Vision::paramInt((prefix + BlobKey::roiIndex).c_str(), "ROI 索引", -1, true, -1, true, 49));
		const size_t begin = f.size() - 14;
		(void)groupName;
		for (size_t i = begin; i < f.size(); ++i) {
			Vision::ParamField& item = f[i];
			item.liveAction = BlobKey::previewThresh;
			if (item.key == typeKey) {
				continue;
			}
			item.showKey = typeKey;
			if (item.key == prefix + BlobKey::roiIndex
				|| item.key == prefix + BlobKey::threshMode
				|| item.key == prefix + BlobKey::absMin
				|| item.key == prefix + BlobKey::absMax
				|| item.key == prefix + BlobKey::relOffset
				|| item.key == prefix + BlobKey::dynOffset
				|| item.key == prefix + BlobKey::dynKernel
				|| item.key == prefix + BlobKey::dark) {
				item.showValues.push_back(BlobKey::stepThresh);
			} else if (item.key == prefix + BlobKey::morphOp || item.key == prefix + BlobKey::radius) {
				item.showValues.push_back(BlobKey::stepMorph);
			} else {
				item.showValues.push_back(BlobKey::stepSelect);
			}
		}
	}
	return f;
}

Vision::ToolCaps Vision::BlobTool::roi_caps()
{
	Vision::ToolCaps c;
	Vision::RoiSlotRule search;
	search.slot = BlobKey::search;
	search.title = "搜索区";
	search.maxCount = 1;
	search.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
	search.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
	c.slotRules.push_back(search);
	Vision::RoiSlotRule detect;
	detect.slot = BlobKey::roi;
	detect.title = "检测区";
	detect.maxCount = 50;
	detect.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjCircle, 0));
	detect.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjRect1, 0));
	detect.shapes.push_back(Vision::RoiShapeRule(HsCommData::ObjPoly, 0));
	detect.itemKinds.push_back(Vision::RoiItemKind::DrawRoi);
	detect.itemKinds.push_back(Vision::RoiItemKind::DrawRegion);
	detect.itemKinds.push_back(Vision::RoiItemKind::RefRoi);
	detect.itemKinds.push_back(Vision::RoiItemKind::RefRegion);
	c.slotRules.push_back(detect);
	return c;
}

Vision::BlobTool::BlobTool()
{
}

Vision::BlobTool::~BlobTool()
{
}

Vision::ToolBase* Vision::BlobTool::clone() const
{
	BlobTool* p = new BlobTool();
	p->copyInstanceDataFrom(*this);
	return p;
}

std::string Vision::BlobTool::tool_name() const
{
	return kToolName;
}

Vision::ToolType Vision::BlobTool::type() const
{
	return Vision::ToolType::Detect;
}

std::string Vision::BlobTool::display_name() const
{
	return kDisplayName;
}

std::string Vision::BlobTool::description() const
{
	return kDescription;
}

std::string Vision::BlobTool::version() const
{
	return std::string(kToolVersion);
}

std::vector<Vision::VerNote> Vision::BlobTool::version_notes() const
{
	return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}

Vision::ParamMap Vision::BlobTool::default_params() const
{
	return defaultsFrom(param_fields(1));
}

std::vector<Vision::ParamField> Vision::BlobTool::cfg_param() const
{
	return param_fields(1);
}

std::vector<Vision::ParamField> Vision::BlobTool::cfg_param(const Vision::ToolParam& current) const
{
	int steps = current.run.value(BlobKey::stepCount).toInt(1);
	if (steps < 1) {
		steps = 1;
	}
	if (steps > 16) {
		steps = 16;
	}
	return param_fields(steps);
}

const Vision::ToolParaBase* Vision::BlobTool::tool_param() const
{
	return &param_;
}

int Vision::BlobTool::check_param(std::string*) const
{
	return HS_OK;
}

Vision::OutputSpec Vision::BlobTool::cfg_output() const
{
	Vision::OutputSpec spec;
	Vision::OutputField count;
	count.key = BlobKey::count;
	count.title = BlobKey::count;
	spec.fields.push_back(count);
	Vision::OutputField areaSum;
	areaSum.key = BlobKey::areaSum;
	areaSum.title = BlobKey::areaSum;
	spec.fields.push_back(areaSum);
	return spec;
}

Vision::TolSpec Vision::BlobTool::cfg_tol() const
{
	Vision::TolSpec spec;
	Vision::TolField count;
	count.key = BlobKey::count;
	count.title = BlobKey::count;
	count.defMin = 1;
	count.defMax = 999;
	count.integer = true;
	spec.items.push_back(count);
	Vision::TolField areaSum;
	areaSum.key = BlobKey::areaSum;
	areaSum.title = BlobKey::areaSum;
	areaSum.defMin = 0;
	areaSum.defMax = 1.0e9;
	spec.items.push_back(areaSum);
	return spec;
}

Vision::DisplaySpec Vision::BlobTool::cfg_display() const
{
	Vision::DisplaySpec spec;
	Vision::GraphicSlot result;
	result.key = BlobKey::resultRoi;
	result.title = BlobKey::resultRoi;
	result.defEnable = true;
	result.region = true;
	spec.graphics.push_back(result);
	Vision::TextSlot count;
	count.key = BlobKey::count;
	count.title = BlobKey::count;
	count.defAnchor = Vision::TextAnchor::ImageCorner;
	count.defEnable = true;
	spec.texts.push_back(count);
	Vision::TextSlot verdict;
	verdict.key = BlobKey::verdict;
	verdict.title = BlobKey::verdict;
	verdict.defAnchor = Vision::TextAnchor::ImageCorner;
	verdict.defEnable = true;
	spec.texts.push_back(verdict);
	return spec;
}

Vision::ToolCaps Vision::BlobTool::cfg_roi() const
{
	return roi_caps();
}

void Vision::BlobTool::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
	build_alg_in(in, ain, BlobKey::roi);
	ain->param = &param_;
}

int Vision::BlobTool::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out)
{
	if (out == nullptr) {
		return HS_ERR_NULL;
	}
	*out = Vision::ToolOut();
	attachAlg(in.toolDataDir);
	param_.load(in.param);
	Vision::AlgToolIn ain;
	fill_alg_in(in, &ain);
	return run_attached_shot(in, actionId, &ain, false, out);
}

int Vision::BlobTool::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
	return shot(in, std::string(), out);
}

std::vector<Vision::ToolAction> Vision::BlobTool::cfg_action() const
{
	std::vector<Vision::ToolAction> actions;
	Vision::ToolAction preview;
	preview.id = BlobKey::previewThresh;
	preview.title = "预览";
	preview.place = Vision::ActionPlace::Run;
	preview.live = true;
	actions.push_back(preview);
	return actions;
}

int Vision::BlobTool::action(const std::string& id, const Vision::ToolIn& in, Vision::ToolOut* out)
{
	if (out == nullptr) {
		return HS_ERR_NULL;
	}
	if (id != BlobKey::previewThresh) {
		return Vision::ToolBase::action(id, in, out);
	}
	return shot(in, id, out);
}

Vision::ToolOut Vision::BlobTool::execute(const Vision::ToolIn& in)
{
	Vision::ToolOut out;
	execute(in, &out);
	return out;
}
