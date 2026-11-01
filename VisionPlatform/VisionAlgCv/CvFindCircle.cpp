/**
 * @file CvFindCircle.cpp
 * @brief 用 Canny 拟合圆（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvFindCircle.h"
#include "CvAlgoHelpers.h"
#include "CvObjectConvert.h"
#include "ParamKeys.h"
#include <HsCommData/HsException.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Vision {

namespace {
const char kVer_CvFindCircle[] = "1.0.0.0";
const char kName_CvFindCircle[] = "locate.find_circle";
const Vision::VerNoteRaw kNotes_CvFindCircle[] = {
	{ "1.0.0.0", "首版：Canny 轮廓圆度筛选" },
};
}

// 构造
CvFindCircle::CvFindCircle()
	: img_w_(0)
	, img_h_(0)
	, found_(false)
	, cx_(0)
	, cy_(0)
	, radius_(0)
	, shot_err_(HS_OK)
{
}

// 析构
CvFindCircle::~CvFindCircle()
{
}

// 克隆
AlgToolBase* CvFindCircle::clone() const
{
	return new CvFindCircle();
}

std::string CvFindCircle::tool_name() const
{
	return kName_CvFindCircle;
}

AlgLibKind CvFindCircle::alg_lib() const
{
	return AlgLibCv;
}

std::string CvFindCircle::version() const
{
	return kVer_CvFindCircle;
}

std::vector<VerNote> CvFindCircle::version_notes() const
{
	return verNotesFrom(kNotes_CvFindCircle, static_cast<int>(sizeof(kNotes_CvFindCircle) / sizeof(kNotes_CvFindCircle[0])));
}

int CvFindCircle::tool_reset()
{
	found_ = false;
	cx_ = 0;
	cy_ = 0;
	radius_ = 0;
	tol_.reset();
	shot_err_ = HS_OK;
	shot_msg_.clear();
	return HS_OK;
}

int CvFindCircle::proc_input(const AlgToolIn& in)
{
	cv::Mat gray;
	int w = 0;
	int h = 0;
	std::string err;
	cv::Mat* pGray = &gray;
	if (!loadGrayFromAlgIn(in, pGray, &w, &h, &err)) {
		shot_err_ = HS_ERR_IMAGE_EMPTY;
		shot_msg_ = err.empty() ? HsCommData::HsException(HS_ERR_IMAGE_EMPTY).errorMessage() : err;
		return shot_err_;
	}
	image_ = gray;
	img_w_ = w;
	img_h_ = h;
	ToolParaFindCircle local;
	paraOf(in, &local);
	search_mask_ = toolRoisToMask(plainRois(in.rois), w, h);
	return HS_OK;
}

int CvFindCircle::alg_run(const AlgToolIn& in)
{
	ToolParaFindCircle local;
	const ToolParaFindCircle* para = paraOf(in, &local);
	const double sigma = para->run.sigma;
	const double low = static_cast<double>(para->run.low);
	const double high = static_cast<double>(para->run.high);
	const double minR = static_cast<double>(para->run.minR);
	const double maxR = static_cast<double>(para->run.maxR);
	cv::Mat localImg;
	image_.copyTo(localImg, search_mask_);
	cv::Mat blur;
	const double s = sigma < 0.4 ? 0.4 : sigma;
	int ksize = static_cast<int>(s * 6.0 + 1.0);
	if (ksize < 3) {
		ksize = 3;
	}
	if ((ksize % 2) == 0) {
		++ksize;
	}
	cv::GaussianBlur(localImg, blur, cv::Size(ksize, ksize), s);
	cv::Mat edges;
	cv::Canny(blur, edges, low, high);
	cv::bitwise_and(edges, search_mask_, edges);
	std::vector<std::vector<cv::Point> > contours;
	cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);
	double bestR = 0;
	double bestCx = 0;
	double bestCy = 0;
	bool found = false;
	for (size_t i = 0; i < contours.size(); ++i) {
		if (contours[i].size() < 8) {
			continue;
		}
		cv::Point2f cen;
		float radius = 0;
		cv::minEnclosingCircle(contours[i], cen, radius);
		const double rVal = static_cast<double>(radius);
		if (rVal < minR || rVal > maxR) {
			continue;
		}
		const double area = cv::contourArea(contours[i]);
		const double peri = cv::arcLength(contours[i], false);
		if (peri < 1e-6) {
			continue;
		}
		const double circ = 4.0 * M_PI * area / (peri * peri);
		if (circ < 0.25) {
			continue;
		}
		if (!found || rVal > bestR) {
			found = true;
			bestR = rVal;
			bestCx = cen.x;
			bestCy = cen.y;
		}
	}
	found_ = found;
	cx_ = bestCx;
	cy_ = bestCy;
	radius_ = bestR;
	if (!found_) {
		shot_msg_ = "未找到圆";
		return HS_OK;
	}
	shot_msg_ = "找圆完成";
	return HS_OK;
}

int CvFindCircle::judge_tol(const AlgToolIn& in)
{
	tol_.reset();
	if (!found_) {
		return HS_OK;
	}
	tol_.put("cx", cx_);
	tol_.put("cy", cy_);
	tol_.put("r", radius_);
	tol_.judge(in.judge, nullptr);
	return HS_OK;
}

int CvFindCircle::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
{
	if (out == nullptr) {
		return HS_ERR_NULL;
	}
	*out = AlgToolOut();
	out->image.image = inputImage(in);
	out->errCode = shot_err_;
	out->message = shot_msg_;
	if (shot_err_ != HS_OK) {
		out->ok = false;
		return shot_err_;
	}
	if (!found_) {
		out->ok = false;
		out->runOk = true;
		out->judgeOk = false;
		return HS_OK;
	}
	tol_.apply(out);
	paramSet(&out->values, ValueKey::X, cx_);
	paramSet(&out->values, ValueKey::Y, cy_);
	paramSet(&out->values, ValueKey::Angle, 0.0);
	paramSet(&out->values, ValueKey::Score, 1.0);
	out->objects.push_back(circleObject(cx_, cy_, radius_));
	out->ok = tol_.ok;
	out->runOk = true;
	out->judgeOk = tol_.ok;
	return HS_OK;
}

int CvFindCircle::execute(const AlgToolIn& in, AlgToolOut* out)
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
