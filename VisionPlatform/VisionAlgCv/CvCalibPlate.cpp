/**
 * @file CvCalibPlate.cpp
 * @brief 离线目录标定板求内参，或导出点对（OpenCV）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvCalibPlate.h"
#include "SolutionDataPaths.h"
#include "TextEncoding.h"
#include <HsCommData/HsException.h>
#include <windows.h>
#include <cmath>
#include <string>
#include <vector>

namespace Vision {

namespace {
const char kVer_CvCalibPlate[] = "1.0.0.0";
const char kName_CvCalibPlate[] = "calib.plate";
const Vision::VerNoteRaw kNotes_CvCalibPlate[] = {
	{ "1.0.0.0", "首版：离线标定板求内参" },
};
const char kVer_CvCalibPlane[] = "1.0.0.0";
const char kName_CvCalibPlane[] = "calib.npoint";
const Vision::VerNoteRaw kNotes_CvCalibPlane[] = {
	{ "1.0.0.0", "首版：导出板点对" },
};
}

CvCalibPlate::CvCalibPlate()
	: rows_(0)
	, cols_(0)
	, spacing_(0)
	, plate_kind_(0)
	, min_images_(3)
	, points_only_(false)
	, found_w_(0)
	, found_h_(0)
	, found_count_(0)
	, shot_err_(HS_OK)
{
}

CvCalibPlate::~CvCalibPlate()
{
}

AlgToolBase* CvCalibPlate::clone() const
{
	return new CvCalibPlate();
}

std::string CvCalibPlate::tool_name() const
{
	return kName_CvCalibPlate;
}

AlgLibKind CvCalibPlate::alg_lib() const
{
	return AlgLibCv;
}

std::string CvCalibPlate::version() const
{
	return kVer_CvCalibPlate;
}

std::vector<VerNote> CvCalibPlate::version_notes() const
{
	return verNotesFrom(kNotes_CvCalibPlate, static_cast<int>(sizeof(kNotes_CvCalibPlate) / sizeof(kNotes_CvCalibPlate[0])));
}

void CvCalibPlate::init(const std::string&)
{
}

void CvCalibPlate::save(const std::string&)
{
}

bool CvCalibPlate::is_image_name(const std::wstring& name) const
{
	const wchar_t* ext[] = { L".bmp", L".jpg", L".jpeg", L".png", L".tif", L".tiff", 0 };
	for (int i = 0; ext[i]; ++i) {
		const size_t n = wcslen(ext[i]);
		if (name.size() >= n) {
			const std::wstring tail = name.substr(name.size() - n);
			if (_wcsicmp(tail.c_str(), ext[i]) == 0) {
				return true;
			}
		}
	}
	return false;
}

std::vector<std::string> CvCalibPlate::list_images(const std::string& dir) const
{
	std::vector<std::string> out;
	if (dir.empty()) {
		return out;
	}
	const std::wstring pattern = utf8ToWide(pathJoin(dir, "*"));
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) {
		return out;
	}
	do {
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			continue;
		}
		if (!is_image_name(fd.cFileName)) {
			continue;
		}
		char name[512];
		WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, static_cast<int>(sizeof(name)), 0, 0);
		out.push_back(pathJoin(dir, name));
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	return out;
}

void CvCalibPlate::object_grid(int rows, int cols, double spacing, std::vector<cv::Point3f>* obj) const
{
	obj->clear();
	for (int r = 0; r < rows; ++r) {
		for (int c = 0; c < cols; ++c) {
			obj->push_back(cv::Point3f(static_cast<float>(c * spacing), static_cast<float>(r * spacing), 0.f));
		}
	}
}

bool CvCalibPlate::find_board(const cv::Mat& gray, int rows, int cols, int plateKind, std::vector<cv::Point2f>* corners) const
{
	cv::Size pattern(cols, rows);
	bool found = false;
	if (plateKind == 1 || plateKind == 2) {
		found = cv::findCirclesGrid(gray, pattern, *corners, cv::CALIB_CB_SYMMETRIC_GRID);
	} else {
		found = cv::findChessboardCorners(gray, pattern, *corners,
			cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);
		if (found) {
			cv::cornerSubPix(gray, *corners, cv::Size(5, 5), cv::Size(-1, -1),
				cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01));
		}
	}
	return found && static_cast<int>(corners->size()) == rows * cols;
}

std::string CvCalibPlate::pack_points(const std::vector<cv::Point2f>& img, const std::vector<cv::Point3f>& obj) const
{
	std::string s;
	char buf[128];
	for (size_t i = 0; i < img.size() && i < obj.size(); ++i) {
		sprintf_s(buf, "%.6f,%.6f,%.6f,%.6f\n", img[i].x, img[i].y, obj[i].x, obj[i].y);
		s += buf;
	}
	return s;
}

void CvCalibPlate::put_model(const cv::Mat& K, const cv::Mat& dist, double rms, double maxRes, int count, int w, int h)
{
	tol_.put("fx", K.at<double>(0, 0));
	tol_.put("fy", K.at<double>(1, 1));
	tol_.put("cx", K.at<double>(0, 2));
	tol_.put("cy", K.at<double>(1, 2));
	tol_.put("k1", dist.cols > 0 ? dist.at<double>(0) : 0.0);
	tol_.put("k2", dist.cols > 1 ? dist.at<double>(1) : 0.0);
	tol_.put("p1", dist.cols > 2 ? dist.at<double>(2) : 0.0);
	tol_.put("p2", dist.cols > 3 ? dist.at<double>(3) : 0.0);
	tol_.put("k3", dist.cols > 4 ? dist.at<double>(4) : 0.0);
	tol_.put("rms", rms);
	tol_.put("maxResidual", maxRes);
	tol_.put_int("count", count);
	tol_.put_int("width", w);
	tol_.put_int("height", h);
}

int CvCalibPlate::tool_reset()
{
	tol_.reset();
	points_text_.clear();
	found_w_ = 0;
	found_h_ = 0;
	found_count_ = 0;
	shot_err_ = HS_OK;
	shot_msg_.clear();
	return HS_OK;
}

int CvCalibPlate::proc_input(const AlgToolIn& in)
{
	ToolParaCalibPlate local;
	const ToolParaCalibPlate* para = paraOf(in, &local);
	rows_ = in.run.value("rows").toInt(0);
	cols_ = in.run.value("cols").toInt(0);
	spacing_ = in.run.value("spacingMm").toDouble(0);
	plate_kind_ = in.run.value("plateKind").toInt(0);
	min_images_ = in.run.value("minImages").toInt(3);
	points_only_ = in.run.value("pointsOnly").toInt(0) != 0;
	image_dir_ = para->run.imageDir.empty() ? in.run.value("imageDir").toString() : para->run.imageDir;
	if (rows_ < 2 || cols_ < 2 || spacing_ <= 0) {
		shot_err_ = HS_ERR_PARAM;
		shot_msg_ = "请先在标定管理里填好行列和间距";
		return shot_err_;
	}
	return HS_OK;
}

int CvCalibPlate::alg_run(const AlgToolIn&)
{
	std::vector<std::string> files = list_images(image_dir_);
	if (files.empty()) {
		shot_err_ = HS_ERR_FILE_OPEN;
		shot_msg_ = "离线目录里没有图片";
		return shot_err_;
	}
	std::vector<std::vector<cv::Point3f> > objAll;
	std::vector<std::vector<cv::Point2f> > imgAll;
	cv::Size imageSize;
	std::vector<cv::Point3f> grid;
	object_grid(rows_, cols_, spacing_, &grid);
	for (size_t i = 0; i < files.size(); ++i) {
		cv::Mat bgr = cv::imread(files[i], cv::IMREAD_COLOR);
		if (bgr.empty()) {
			continue;
		}
		cv::Mat gray;
		if (bgr.channels() == 1) {
			gray = bgr;
		} else {
			cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
		}
		std::vector<cv::Point2f> corners;
		if (!find_board(gray, rows_, cols_, plate_kind_, &corners)) {
			continue;
		}
		if (points_only_) {
			points_text_ = pack_points(corners, grid);
			found_w_ = gray.cols;
			found_h_ = gray.rows;
			found_count_ = 1;
			tol_.put_text("points", points_text_);
			tol_.put_int("width", found_w_);
			tol_.put_int("height", found_h_);
			shot_msg_ = "已找到标定板";
			return HS_OK;
		}
		imageSize = gray.size();
		objAll.push_back(grid);
		imgAll.push_back(corners);
	}
	found_count_ = static_cast<int>(imgAll.size());
	if (found_count_ < min_images_) {
		shot_err_ = HS_ERR_PARAM;
		shot_msg_ = "找到标定板的图片不够";
		tol_.put_int("count", found_count_);
		return shot_err_;
	}
	cv::Mat K;
	cv::Mat dist;
	std::vector<cv::Mat> rvecs;
	std::vector<cv::Mat> tvecs;
	const double rms = cv::calibrateCamera(objAll, imgAll, imageSize, K, dist, rvecs, tvecs);
	double maxRes = 0;
	for (size_t i = 0; i < objAll.size(); ++i) {
		std::vector<cv::Point2f> proj;
		cv::projectPoints(objAll[i], rvecs[i], tvecs[i], K, dist, proj);
		for (size_t k = 0; k < proj.size(); ++k) {
			const double dx = proj[k].x - imgAll[i][k].x;
			const double dy = proj[k].y - imgAll[i][k].y;
			const double e = std::sqrt(dx * dx + dy * dy);
			if (e > maxRes) {
				maxRes = e;
			}
		}
	}
	found_w_ = imageSize.width;
	found_h_ = imageSize.height;
	put_model(K, dist, rms, maxRes, found_count_, found_w_, found_h_);
	shot_msg_ = "内参已算出";
	return HS_OK;
}

int CvCalibPlate::judge_tol(const AlgToolIn& in)
{
	if (shot_err_ != HS_OK || points_only_) {
		return HS_OK;
	}
	tol_.judge(in.judge, nullptr);
	return HS_OK;
}

int CvCalibPlate::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
	if (out == nullptr) {
		return HS_ERR_NULL;
	}
	*out = AlgToolOut();
	out->errCode = shot_err_;
	out->message = shot_msg_;
	tol_.apply(out);
	if (shot_err_ != HS_OK) {
		out->ok = false;
		return shot_err_;
	}
	out->ok = tol_.ok;
	out->runOk = true;
	out->judgeOk = tol_.ok;
	return HS_OK;
}

int CvCalibPlate::execute(const AlgToolIn& in, AlgToolOut* out)
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

// -------------------- CvCalibPlane --------------------

CvCalibPlane::CvCalibPlane()
	: shot_err_(HS_OK)
{
}

CvCalibPlane::~CvCalibPlane()
{
}

AlgToolBase* CvCalibPlane::clone() const
{
	return new CvCalibPlane();
}

std::string CvCalibPlane::tool_name() const
{
	return kName_CvCalibPlane;
}

AlgLibKind CvCalibPlane::alg_lib() const
{
	return AlgLibCv;
}

std::string CvCalibPlane::version() const
{
	return kVer_CvCalibPlane;
}

std::vector<VerNote> CvCalibPlane::version_notes() const
{
	return verNotesFrom(kNotes_CvCalibPlane, static_cast<int>(sizeof(kNotes_CvCalibPlane) / sizeof(kNotes_CvCalibPlane[0])));
}

int CvCalibPlane::tool_reset()
{
	shot_err_ = HS_OK;
	shot_msg_.clear();
	plate_out_ = AlgToolOut();
	return HS_OK;
}

int CvCalibPlane::proc_input(const AlgToolIn& in)
{
	ToolParaCalibPlate local;
	paraOf(in, &local);
	return HS_OK;
}

int CvCalibPlane::alg_run(const AlgToolIn& in)
{
	AlgToolIn copy = in;
	copy.run.insert("pointsOnly", 1);
	copy.run.insert("minImages", 1);
	const int code = plate_.execute(copy, &plate_out_);
	shot_err_ = code;
	shot_msg_ = plate_out_.message;
	return code;
}

int CvCalibPlane::judge_tol(const AlgToolIn&)
{
	return HS_OK;
}

int CvCalibPlane::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
	if (out == nullptr) {
		return HS_ERR_NULL;
	}
	*out = plate_out_;
	if (shot_err_ != HS_OK && out->errCode == HS_OK) {
		out->errCode = shot_err_;
		out->ok = false;
	}
	if (!shot_msg_.empty()) {
		out->message = shot_msg_;
	}
	return shot_err_;
}

int CvCalibPlane::execute(const AlgToolIn& in, AlgToolOut* out)
{
	if (out == nullptr) {
		return HS_ERR_NULL;
	}
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
}

}
