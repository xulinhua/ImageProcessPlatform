/**
 * @file CvBlob.cpp
 * @brief 绝对、相对、动态阈值 Blob（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvBlob.h"
#include "CvAlgoHelpers.h"
#include "CvObjectConvert.h"
#include "CvOps.h"
#include <HsCommData/HsException.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Vision {

namespace {
const char kVer_CvBlob[] = "1.0.0.0"; ///< 当前版本
const char kName_CvBlob[] = "detect.blob"; ///< 注册名
const Vision::VerNoteRaw kNotes_CvBlob[] = {
	{ "1.0.0.0", "首版：阈值、形态学、连通域筛选" },
};
}

// 构造
CvBlob::CvBlob()
	: img_w_(0)
	, img_h_(0)
	, blob_count_(0)
	, area_sum_(0)
	, shot_err_(HS_OK)
{
}

// 析构
CvBlob::~CvBlob()
{
}

// 克隆本实例
AlgToolBase* CvBlob::clone() const
{
	return new CvBlob();
}

// 注册名
std::string CvBlob::tool_name() const
{
	return kName_CvBlob;
}

// 算法库
AlgLibKind CvBlob::alg_lib() const
{
	return AlgLibCv;
}

// 工具版本
std::string CvBlob::version() const
{
	return kVer_CvBlob;
}

// 版本变更，只追加
std::vector<VerNote> CvBlob::version_notes() const
{
	return verNotesFrom(kNotes_CvBlob, static_cast<int>(sizeof(kNotes_CvBlob) / sizeof(kNotes_CvBlob[0])));
}

// 斑点没有模型文件
void CvBlob::init(const std::string&)
{
}

// 斑点没有模型文件
void CvBlob::save(const std::string&)
{
}

// 清空本拍测值、区域与错误码
int CvBlob::tool_reset()
{
	blob_count_ = 0;
	area_sum_ = 0;
	tol_.reset();
	shot_err_ = HS_OK;
	shot_msg_.clear();
	result_mask_.release();
	return HS_OK;
}

// 取灰度图、搜索掩膜与参数对象
int CvBlob::proc_input(const AlgToolIn& in)
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
	if (w < 1 || h < 1) {
		shot_err_ = HS_ERR_IMAGE_EMPTY;
		shot_msg_ = HsCommData::HsException(HS_ERR_IMAGE_EMPTY).errorMessage();
		return shot_err_;
	}
	image_ = gray;
	img_w_ = w;
	img_h_ = h;
	ToolParaBlob local;
	paraOf(in, &local);
	search_mask_ = searchDomainFromAlgIn(plainRois(in.rois), plainRegions(in.regions), w, h);
	if (maskIsEmpty(search_mask_)) {
		shot_err_ = HS_ERR_OPERATOR;
		shot_msg_ = "搜索区域为空";
		return shot_err_;
	}
	return HS_OK;
}

// 按一步阈值参数二值化，并与搜索掩膜求交
int CvBlob::threshold_bin(const ToolParaBlob::Step& th, cv::Mat* bin) const
{
	if (bin == nullptr) {
		return HS_ERR_NULL;
	}
	cv::Mat local;
	image_.copyTo(local, search_mask_);
	const std::string mode = th.threshMode;
	const bool dark = th.dark;
	if (mode == "relative") {
		const cv::Scalar mean = cv::mean(local, search_mask_);
		const double m = mean[0];
		const double off = th.relOffset;
		if (dark) {
			cv::threshold(local, *bin, m - off, 255.0, cv::THRESH_BINARY_INV);
		} else {
			cv::threshold(local, *bin, m + off, 255.0, cv::THRESH_BINARY);
		}
	} else if (mode == "dynamic") {
		int k = th.dynKernel;
		if (k < 3) {
			k = 3;
		}
		if ((k % 2) == 0) {
			++k;
		}
		cv::Mat bg;
		cv::blur(local, bg, cv::Size(k, k));
		cv::Mat diff;
		if (dark) {
			cv::subtract(bg, local, diff);
		} else {
			cv::subtract(local, bg, diff);
		}
		cv::threshold(diff, *bin, th.dynOffset, 255.0, cv::THRESH_BINARY);
	} else {
		if (dark) {
			cv::threshold(local, *bin, th.absMin, 255.0, cv::THRESH_BINARY_INV);
		} else {
			cv::inRange(local, cv::Scalar(th.absMin), cv::Scalar(th.absMax), *bin);
		}
	}
	cv::bitwise_and(*bin, search_mask_, *bin);
	return HS_OK;
}

// 开闭运算
void CvBlob::morph_open_close(const cv::Mat& bin, double openR, double closeR, cv::Mat* out) const
{
	if (out == nullptr) {
		return;
	}
	cv::Mat* pOut = out;
	morphOpenClose(bin, openR, closeR, pOut);
}

// 连通、按面积/圆度筛选
int CvBlob::pack_blobs(const cv::Mat& bin, double minArea, double maxArea, double minCirc, double maxCirc)
{
	cv::Mat labels;
	cv::Mat stats;
	cv::Mat centroids;
	const int n = cv::connectedComponentsWithStats(bin, labels, stats, centroids, 8, CV_32S);
	cv::Mat sel = cv::Mat::zeros(bin.size(), CV_8UC1);
	int count = 0;
	double sumA = 0;
	const double areaHi = maxArea > 0.0 ? maxArea : 1.0e12;
	for (int i = 1; i < n; ++i) {
		const double area = static_cast<double>(stats.at<int>(i, cv::CC_STAT_AREA));
		if (area < minArea || area > areaHi) {
			continue;
		}
		if (minCirc > 0.0 || maxCirc < 0.999) {
			cv::Mat comp = (labels == i);
			std::vector<std::vector<cv::Point> > contours;
			cv::findContours(comp, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
			double peri = 0;
			if (!contours.empty()) {
				peri = cv::arcLength(contours[0], true);
			}
			double circ = 0;
			if (peri > 1e-6) {
				circ = 4.0 * M_PI * area / (peri * peri);
			}
			if (circ < minCirc || circ > maxCirc) {
				continue;
			}
			sel.setTo(255, comp);
		} else {
			sel.setTo(255, labels == i);
		}
		++count;
		sumA += area;
	}
	blob_count_ = count;
	area_sum_ = sumA;
	result_mask_ = sel;
	if (count < 1) {
		shot_err_ = HS_OK;
		shot_msg_ = "未找到连通域";
		return HS_OK;
	}
	shot_msg_ = "Blob 完成";
	return HS_OK;
}

// 阈值、形态学、连通域筛选
int CvBlob::alg_run(const AlgToolIn& in)
{
	ToolParaBlob local;
	const ToolParaBlob* para = paraOf(in, &local);
	const ToolParaBlob::Step* th = nullptr;
	double openR = 0.0;
	double closeR = 0.0;
	double minArea = 50.0;
	double maxArea = 0.0;
	double minCirc = 0.0;
	double maxCirc = 1.0;
	bool haveArea = false;
	for (size_t si = 0; si < para->run.steps.size(); ++si) {
		const ToolParaBlob::Step& st = para->run.steps[si];
		if (st.type == BlobKey::stepThresh && th == nullptr) {
			th = &para->run.steps[si];
		} else if (st.type == BlobKey::stepMorph && st.morphOp == BlobKey::morphOpen) {
			openR = st.radius;
		} else if (st.type == BlobKey::stepMorph && st.morphOp == BlobKey::morphClose) {
			closeR = st.radius;
		} else if (st.type == BlobKey::stepSelect && st.feature == BlobKey::featArea && !haveArea) {
			minArea = st.minVal;
			maxArea = st.maxVal;
			haveArea = true;
		} else if (st.type == BlobKey::stepSelect && st.feature == BlobKey::featCirc) {
			minCirc = st.minVal;
			maxCirc = st.maxVal;
		}
	}
	ToolParaBlob::Step fallback;
	if (th == nullptr) {
		th = &fallback;
	}
	cv::Mat bin;
	const int thr = threshold_bin(*th, &bin);
	if (thr != HS_OK) {
		shot_err_ = thr;
		return shot_err_;
	}
	if (maskIsEmpty(bin)) {
		shot_msg_ = "未找到连通域";
		blob_count_ = 0;
		area_sum_ = 0;
		result_mask_ = cv::Mat::zeros(image_.size(), CV_8UC1);
		return HS_OK;
	}
	cv::Mat morphed;
	morph_open_close(bin, openR, closeR, &morphed);
	return pack_blobs(morphed, minArea, maxArea, minCirc, maxCirc);
}

// 按数量与总面积做公差
int CvBlob::judge_tol(const AlgToolIn& in)
{
	ToolParaBlob local;
	const ToolParaBlob* para = paraOf(in, &local);
	tol_.reset();
	tol_.put_int(BlobKey::count, blob_count_);
	tol_.put(BlobKey::areaSum, area_sum_);
	const int okMinCount = para->run.okMinCount;
	const int okMaxCount = para->run.okMaxCount;
	const double okMinArea = para->run.okMinArea;
	const double okMaxArea = para->run.okMaxArea;
	if (okMinCount > 0 && blob_count_ < okMinCount) {
		tol_.ok = false;
		tol_.add_miss(BlobKey::count, static_cast<double>(blob_count_), static_cast<double>(okMinCount), static_cast<double>(okMaxCount > 0 ? okMaxCount : blob_count_));
		shot_msg_ = "Blob 数量低于判定下限";
	} else if (okMaxCount > 0 && blob_count_ > okMaxCount) {
		tol_.ok = false;
		tol_.add_miss(BlobKey::count, static_cast<double>(blob_count_), static_cast<double>(okMinCount), static_cast<double>(okMaxCount));
		shot_msg_ = "Blob 数量超过判定上限";
	} else if (okMinArea > 0.0 && area_sum_ + 1e-9 < okMinArea) {
		tol_.ok = false;
		tol_.add_miss(BlobKey::areaSum, area_sum_, okMinArea, okMaxArea > 0.0 ? okMaxArea : area_sum_);
		shot_msg_ = "Blob 总面积低于判定下限";
	} else if (okMaxArea > 0.0 && area_sum_ > okMaxArea + 1e-9) {
		tol_.ok = false;
		tol_.add_miss(BlobKey::areaSum, area_sum_, okMinArea, okMaxArea);
		shot_msg_ = "Blob 总面积超过判定上限";
	}
	tol_.judge(in.judge, BlobKey::count);
	return HS_OK;
}

// 把连通域、测值和公差写进 out
int CvBlob::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
{
	if (out == nullptr) {
		return HS_ERR_NULL;
	}
	*out = AlgToolOut();
	out->image.image = inputImage(in);
	out->errCode = shot_err_;
	out->message = shot_msg_;
	tol_.apply(out);
	if (shot_err_ != HS_OK) {
		out->ok = false;
		return shot_err_;
	}
	if (blob_count_ < 1) {
		out->ok = false;
		out->runOk = true;
		out->judgeOk = false;
		return HS_OK;
	}
	out->objects.push_back(maskToRegionObject(result_mask_));
	out->ok = tol_.ok;
	out->runOk = true;
	out->judgeOk = tol_.ok;
	return HS_OK;
}

// 编排：复位 → 输入 → 算法 → 公差 → 写出
int CvBlob::execute(const AlgToolIn& in, AlgToolOut* out)
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
