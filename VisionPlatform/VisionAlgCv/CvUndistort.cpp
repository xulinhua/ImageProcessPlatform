/**
 * @file CvUndistort.cpp
 * @brief 按内参和畸变系数去畸变（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvUndistort.h"
#include "CvImageConvert.h"
#include <HsCommData/HsException.h>
#include <string>

namespace Vision {

namespace {
const char kVer_CvUndistort[] = "1.0.0.0";
const char kName_CvUndistort[] = "preprocess.undistort";
const Vision::VerNoteRaw kNotes_CvUndistort[] = {
	{ "1.0.0.0", "首版：内参去畸变" },
};
}

CvUndistort::CvUndistort()
	: shot_err_(HS_OK)
{
}

CvUndistort::~CvUndistort()
{
}

AlgToolBase* CvUndistort::clone() const
{
	return new CvUndistort();
}

std::string CvUndistort::tool_name() const
{
	return kName_CvUndistort;
}

AlgLibKind CvUndistort::alg_lib() const
{
	return AlgLibCv;
}

std::string CvUndistort::version() const
{
	return kVer_CvUndistort;
}

std::vector<VerNote> CvUndistort::version_notes() const
{
	return verNotesFrom(kNotes_CvUndistort, static_cast<int>(sizeof(kNotes_CvUndistort) / sizeof(kNotes_CvUndistort[0])));
}

void CvUndistort::init(const std::string&)
{
}

void CvUndistort::save(const std::string&)
{
}

int CvUndistort::tool_reset()
{
	shot_err_ = HS_OK;
	shot_msg_.clear();
	dst_.release();
	return HS_OK;
}

int CvUndistort::proc_input(const AlgToolIn& in)
{
	if (inputImage(in).isNull()) {
		shot_err_ = HS_ERR_IMAGE_EMPTY;
		shot_msg_ = HsCommData::HsException(HS_ERR_IMAGE_EMPTY).errorMessage();
		return shot_err_;
	}
	ToolParaUndistort local;
	paraOf(in, &local);
	cv::Mat src;
	cv::Mat* srcPtr = &src;
	std::string err;
	if (!hsImageToMat(inputImage(in), srcPtr, &err)) {
		shot_err_ = HS_ERR_OPERATOR;
		shot_msg_ = err;
		return shot_err_;
	}
	src_ = src;
	return HS_OK;
}

void CvUndistort::ensure_maps(const cv::Size& size, const cv::Mat& K, const cv::Mat& dist, const std::string& stamp)
{
	const std::string key = stamp + ":" + std::to_string(size.width) + "x" + std::to_string(size.height);
	if (map_key_ == key && !map1_.empty() && !map2_.empty()) {
		return;
	}
	cv::initUndistortRectifyMap(K, dist, cv::Mat(), K, size, CV_16SC2, map1_, map2_);
	map_key_ = key;
}

int CvUndistort::alg_run(const AlgToolIn& in)
{
	const double fx = in.run.value("fx").toDouble(0);
	const double fy = in.run.value("fy").toDouble(0);
	const double cx = in.run.value("cx").toDouble(0);
	const double cy = in.run.value("cy").toDouble(0);
	cv::Mat K = (cv::Mat_<double>(3, 3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
	cv::Mat dist = (cv::Mat_<double>(1, 5)
		<< in.run.value("k1").toDouble(0), in.run.value("k2").toDouble(0),
		   in.run.value("p1").toDouble(0), in.run.value("p2").toDouble(0),
		   in.run.value("k3").toDouble(0));
	ensure_maps(src_.size(), K, dist, in.run.value("stamp").toString());
	cv::remap(src_, dst_, map1_, map2_, cv::INTER_LINEAR);
	shot_msg_ = "已校正";
	return HS_OK;
}

int CvUndistort::judge_tol(const AlgToolIn&)
{
	return HS_OK;
}

int CvUndistort::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
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
	std::string err;
	HsCommData::HsImage image;
	HsCommData::HsImage* imagePtr = &image;
	if (!matToHsImage(dst_, inputImage(in).frameId, imagePtr, &err)) {
		out->ok = false;
		out->errCode = HS_ERR_OPERATOR;
		out->message = err;
		return out->errCode;
	}
	out->image.image = image;
	out->ok = true;
	out->runOk = true;
	out->judgeOk = true;
	return HS_OK;
}

int CvUndistort::execute(const AlgToolIn& in, AlgToolOut* out)
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
