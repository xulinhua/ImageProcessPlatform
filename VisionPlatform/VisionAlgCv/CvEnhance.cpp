/**
 * @file CvEnhance.cpp
 * @brief 图像增强流水（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvEnhance.h"
#include "CvAlgoHelpers.h"
#include "CvImageConvert.h"
#include "CvObjectConvert.h"
#include <HsCommData/HsException.h>

namespace Vision {

namespace {
const char kVer_CvEnhance[] = "1.0.0.0";
const char kName_CvEnhance[] = "preprocess.enhance";
const Vision::VerNoteRaw kNotes_CvEnhance[] = {
	{ "1.0.0.0", "首版：增强流水" },
};
}

CvEnhance::CvEnhance()
	: shot_err_(HS_OK)
{
}

CvEnhance::~CvEnhance()
{
}

AlgToolBase* CvEnhance::clone() const
{
	return new CvEnhance();
}

std::string CvEnhance::tool_name() const
{
	return kName_CvEnhance;
}

AlgLibKind CvEnhance::alg_lib() const
{
	return AlgLibCv;
}

std::string CvEnhance::version() const
{
	return kVer_CvEnhance;
}

std::vector<VerNote> CvEnhance::version_notes() const
{
	return verNotesFrom(kNotes_CvEnhance, static_cast<int>(sizeof(kNotes_CvEnhance) / sizeof(kNotes_CvEnhance[0])));
}

std::string CvEnhance::step_key(int i, const char* suffix) const
{
	return std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_" + suffix;
}

CvEnhance::EnhanceOp CvEnhance::enhance_op_from_int(int v) const
{
	switch (v) {
	case 0: return EnhanceOp::EquHisto;
	case 1: return EnhanceOp::Emphasize;
	case 2: return EnhanceOp::Illuminate;
	case 3: return EnhanceOp::Scale;
	case 4: return EnhanceOp::Invert;
	default: return EnhanceOp::EquHisto;
	}
}

std::vector<CvEnhance::EnhanceStep> CvEnhance::enhance_steps_from_params(const ParamMap& p) const
{
	std::vector<EnhanceStep> out;
	const int n = p.value("pl_n").toInt(0);
	if (n <= 0) {
		return out;
	}
	out.reserve(static_cast<size_t>(n));
	for (int i = 0; i < n; ++i) {
		EnhanceStep st;
		st.op = enhance_op_from_int(p.value(step_key(i, "op")).toInt());
		st.maskW = p.value(step_key(i, "w")).toInt(st.maskW);
		st.maskH = p.value(step_key(i, "h")).toInt(st.maskH);
		st.coeff = p.value(step_key(i, "c0")).toDouble(st.coeff);
		st.coeff2 = p.value(step_key(i, "c1")).toDouble(st.coeff2);
		st.scaleMax = p.value(step_key(i, "f")).toInt(0) != 0;
		out.push_back(st);
	}
	return out;
}

bool CvEnhance::apply_enhance_op(cv::Mat* img, const EnhanceStep& st, std::string* err) const
{
	if (img == nullptr) {
		if (err) {
			*err = "空图像";
		}
		return false;
	}
	switch (st.op) {
	case EnhanceOp::EquHisto:
		cv::equalizeHist(*img, *img);
		return true;
	case EnhanceOp::Emphasize: {
		int mw = st.maskW < 1 ? 1 : st.maskW;
		int mh = st.maskH < 1 ? 1 : st.maskH;
		if ((mw % 2) == 0) {
			++mw;
		}
		if ((mh % 2) == 0) {
			++mh;
		}
		cv::Mat blur;
		cv::blur(*img, blur, cv::Size(mw, mh));
		cv::Mat diff;
		cv::subtract(*img, blur, diff, cv::noArray(), CV_16S);
		cv::Mat scaled;
		diff.convertTo(scaled, CV_16S, st.coeff);
		cv::Mat src16;
		img->convertTo(src16, CV_16S);
		cv::Mat sum;
		cv::add(src16, scaled, sum);
		sum.convertTo(*img, CV_8U);
		return true;
	}
	case EnhanceOp::Illuminate: {
		int mw = st.maskW < 1 ? 1 : st.maskW;
		int mh = st.maskH < 1 ? 1 : st.maskH;
		if ((mw % 2) == 0) {
			++mw;
		}
		if ((mh % 2) == 0) {
			++mh;
		}
		cv::Mat blur;
		cv::blur(*img, blur, cv::Size(mw, mh));
		cv::Mat fImg;
		cv::Mat fBlur;
		img->convertTo(fImg, CV_32F);
		blur.convertTo(fBlur, CV_32F);
		cv::Mat out;
		cv::divide(fImg, fBlur + 1e-3f, out);
		out *= static_cast<float>(128.0 * (st.coeff > 0.0 ? st.coeff : 1.0));
		out.convertTo(*img, CV_8U);
		return true;
	}
	case EnhanceOp::Scale:
		if (st.scaleMax) {
			double mn = 0;
			double mx = 0;
			cv::minMaxLoc(*img, &mn, &mx);
			if (mx > mn + 1e-9) {
				img->convertTo(*img, CV_8U, 255.0 / (mx - mn), -mn * 255.0 / (mx - mn));
			}
		} else {
			cv::Mat tmp;
			img->convertTo(tmp, CV_32F, st.coeff, st.coeff2);
			tmp.convertTo(*img, CV_8U);
		}
		return true;
	case EnhanceOp::Invert:
		cv::bitwise_not(*img, *img);
		return true;
	default:
		if (err) {
			*err = "未知增强算子";
		}
		return false;
	}
}

int CvEnhance::tool_reset()
{
	shot_err_ = HS_OK;
	shot_msg_.clear();
	work_.release();
	return HS_OK;
}

int CvEnhance::proc_input(const AlgToolIn& in)
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
	ToolParaEnhance local;
	paraOf(in, &local);
	image_ = gray;
	work_ = work;
	roi_mask_ = roiMask;
	return HS_OK;
}

int CvEnhance::alg_run(const AlgToolIn& in)
{
	std::vector<EnhanceStep> steps = enhance_steps_from_params(in.run);
	if (steps.empty()) {
		EnhanceStep one;
		steps.push_back(one);
	}
	std::string err;
	for (size_t i = 0; i < steps.size(); ++i) {
		if (!apply_enhance_op(&work_, steps[i], &err)) {
			shot_err_ = HS_ERR_OPERATOR;
			shot_msg_ = err;
			return shot_err_;
		}
	}
	shot_msg_ = "图像增强完成";
	return HS_OK;
}

int CvEnhance::judge_tol(const AlgToolIn&)
{
	return HS_OK;
}

int CvEnhance::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
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

int CvEnhance::execute(const AlgToolIn& in, AlgToolOut* out)
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
