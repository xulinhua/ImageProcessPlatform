/**
 * @file CvSmooth.cpp
 * @brief 图像平滑流水（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvSmooth.h"
#include "CvAlgoHelpers.h"
#include "CvImageConvert.h"
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer_CvSmooth[] = "1.0.0.0";
const char kName_CvSmooth[] = "preprocess.smooth";
const Vision::VerNoteRaw kNotes_CvSmooth[] = {
	{ "1.0.0.0", "首版：平滑流水" },
};
}

CvSmooth::CvSmooth()
	: shot_err_(HS_OK)
{
}

CvSmooth::~CvSmooth()
{
}

AlgToolBase* CvSmooth::clone() const
{
	return new CvSmooth();
}

std::string CvSmooth::tool_name() const
{
	return kName_CvSmooth;
}

AlgLibKind CvSmooth::alg_lib() const
{
	return AlgLibCv;
}

std::string CvSmooth::version() const
{
	return kVer_CvSmooth;
}

std::vector<VerNote> CvSmooth::version_notes() const
{
	return verNotesFrom(kNotes_CvSmooth, static_cast<int>(sizeof(kNotes_CvSmooth) / sizeof(kNotes_CvSmooth[0])));
}

std::string CvSmooth::step_key(int i, const char* suffix) const
{
	return std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_" + suffix;
}

CvSmooth::SmoothOp CvSmooth::smooth_op_from_int(int v) const
{
	switch (v) {
	case 0: return SmoothOp::Mean;
	case 1: return SmoothOp::Median;
	case 2: return SmoothOp::Gauss;
	case 3: return SmoothOp::Bilateral;
	default: return SmoothOp::Median;
	}
}

std::vector<CvSmooth::SmoothStep> CvSmooth::smooth_steps_from_params(const ParamMap& p) const
{
	std::vector<SmoothStep> out;
	const int n = p.value("pl_n").toInt(0);
	if (n <= 0) {
		return out;
	}
	out.reserve(static_cast<size_t>(n));
	for (int i = 0; i < n; ++i) {
		SmoothStep st;
		st.op = smooth_op_from_int(p.value(step_key(i, "op")).toInt());
		st.maskW = p.value(step_key(i, "w")).toInt(st.maskW);
		st.maskH = p.value(step_key(i, "h")).toInt(st.maskH);
		st.sigmaRange = p.value(step_key(i, "c0")).toDouble(st.sigmaRange);
		st.sigmaSpace = p.value(step_key(i, "c1")).toDouble(st.sigmaSpace);
		out.push_back(st);
	}
	return out;
}

bool CvSmooth::apply_smooth_op(cv::Mat* img, const SmoothStep& st, std::string* err) const
{
	if (img == nullptr) {
		if (err) {
			*err = "空图像";
		}
		return false;
	}
	switch (st.op) {
	case SmoothOp::Mean: {
		int w = st.maskW < 1 ? 1 : st.maskW;
		int h = st.maskH < 1 ? 1 : st.maskH;
		if ((w % 2) == 0) {
			++w;
		}
		if ((h % 2) == 0) {
			++h;
		}
		cv::blur(*img, *img, cv::Size(w, h));
		return true;
	}
	case SmoothOp::Median: {
		int radius = st.maskW < 1 ? 1 : st.maskW;
		int k = radius * 2 + 1;
		if (k < 3) {
			k = 3;
		}
		if ((k % 2) == 0) {
			++k;
		}
		cv::medianBlur(*img, *img, k);
		return true;
	}
	case SmoothOp::Gauss: {
		int size = st.maskW < 3 ? 3 : st.maskW;
		if ((size % 2) == 0) {
			++size;
		}
		cv::GaussianBlur(*img, *img, cv::Size(size, size), 0);
		return true;
	}
	case SmoothOp::Bilateral: {
		cv::Mat tmp;
		cv::bilateralFilter(*img, tmp, 0, st.sigmaRange, st.sigmaSpace);
		*img = tmp;
		return true;
	}
	default:
		if (err) {
			*err = "未知平滑算子";
		}
		return false;
	}
}

int CvSmooth::tool_reset()
{
	shot_err_ = HS_OK;
	shot_msg_.clear();
	work_.release();
	return HS_OK;
}

int CvSmooth::proc_input(const AlgToolIn& in)
{
	cv::Mat gray;
	cv::Mat work;
	cv::Mat roiMask;
	std::string err;
	cv::Mat* pGray = &gray;
	cv::Mat* pWork = &work;
	cv::Mat* pRoi = &roiMask;
	if (!preparePreprocessWork(in, pGray, pWork, pRoi, &err)) {
		shot_err_ = HS_ERR_IMAGE_EMPTY;
		shot_msg_ = err.empty() ? HsCommData::HsException(HS_ERR_IMAGE_EMPTY).errorMessage() : err;
		return shot_err_;
	}
	ToolParaSmooth local;
	paraOf(in, &local);
	image_ = gray;
	work_ = work;
	roi_mask_ = roiMask;
	return HS_OK;
}

int CvSmooth::alg_run(const AlgToolIn& in)
{
	std::vector<SmoothStep> steps = smooth_steps_from_params(in.run);
	if (steps.empty()) {
		SmoothStep one;
		steps.push_back(one);
	}
	std::string err;
	for (size_t i = 0; i < steps.size(); ++i) {
		if (!apply_smooth_op(&work_, steps[i], &err)) {
			shot_err_ = HS_ERR_OPERATOR;
			shot_msg_ = err;
			return shot_err_;
		}
	}
	shot_msg_ = "图像平滑完成";
	return HS_OK;
}

int CvSmooth::judge_tol(const AlgToolIn&)
{
	return HS_OK;
}

int CvSmooth::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
{
	if (out == nullptr) {
		return HS_ERR_NULL;
	}
	*out = AlgToolOut();
	out->errCode = shot_err_;
	out->message = shot_msg_;
	if (shot_err_ != HS_OK) {
		out->ok = false;
		return shot_err_;
	}
	cv::Mat merged = image_.clone();
	work_.copyTo(merged, roi_mask_);
	std::string err;
	HsCommData::HsImage* pImage = &out->image.image;
	if (!matToHsImage(merged, inputImage(in).frameId, pImage, &err)) {
		out->ok = false;
		out->errCode = HS_ERR_OPERATOR;
		out->message = err;
		return out->errCode;
	}
	out->ok = true;
	out->runOk = true;
	out->judgeOk = true;
	return HS_OK;
}

int CvSmooth::execute(const AlgToolIn& in, AlgToolOut* out)
{
	if (out == nullptr) {
		return HS_ERR_NULL;
	}
	try {
		run_timer_.start();
		tool_reset();
		run_timer_.mark("tool_reset");
		int step = proc_input(in);
		run_timer_.mark("proc_input");
		if (step == HS_OK) {
			step = alg_run(in);
			run_timer_.mark("alg_run");
		}
		if (step == HS_OK) {
			step = judge_tol(in);
			run_timer_.mark("judge_tol");
		}
		alg_rst_proc(in, out);
		run_timer_.stop("alg_rst_proc");
		write_alg_steps(out, run_timer_.sections(), run_timer_.totalMs());
		return step != HS_OK ? step : shot_err_;
	} catch (const cv::Exception& e) {
		out->ok = false;
		out->errCode = HS_ERR_OPERATOR;
		out->message = e.what() ? std::string(e.what()) : std::string("OpenCV error");
		return HS_ERR_OPERATOR;
	}
}

}
