/**
 * @file HalCalibPlate.cpp
 * @brief 用 Halcon 标定板求相机内参
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalCalibPlate.h"
#include "HalImageConvert.h"
#include "HalOps.h"
#include "SolutionDataPaths.h"
#include "TextEncoding.h"
#include "ToolParaCalibPlate.hpp"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <windows.h>
#include <cstdio>
#include <cstdlib>

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "calib.plate";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "首版" },
};
}

HalCalibPlate::HalCalibPlate()
    : rows_(0)
    , cols_(0)
    , spacing_mm_(0.0)
    , min_images_(3)
    , shot_err_(0)
{
}

AlgToolBase* HalCalibPlate::clone() const
{
    return new HalCalibPlate();
}

std::string HalCalibPlate::tool_name() const
{
    return kName;
}

AlgLibKind HalCalibPlate::alg_lib() const
{
    return AlgLibHal;
}

std::string HalCalibPlate::version() const
{
    return kVer;
}

std::vector<VerNote> HalCalibPlate::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

void HalCalibPlate::init(const std::string&)
{
}

void HalCalibPlate::save(const std::string&)
{
}

int HalCalibPlate::tool_reset()
{
    image_files_.clear();
    image_dir_.clear();
    rows_ = 0;
    cols_ = 0;
    spacing_mm_ = 0.0;
    min_images_ = 3;
    tol_.reset();
    shot_err_ = 0;
    shot_msg_.clear();
    return HS_OK;
}

bool HalCalibPlate::is_image_name(const std::wstring& name) const
{
    const wchar_t* ext[] = { L".bmp", L".jpg", L".jpeg", L".png", L".tif", L".tiff", nullptr };
    for (int i = 0; ext[i]; ++i) {
        const size_t n = wcslen(ext[i]);
        if (name.size() >= n && _wcsicmp(name.c_str() + name.size() - n, ext[i]) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> HalCalibPlate::list_images(const std::string& dir) const
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
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && is_image_name(fd.cFileName)) {
            char name[512];
            WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, static_cast<int>(sizeof(name)), nullptr, nullptr);
            out.push_back(pathJoin(dir, name));
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

std::string HalCalibPlate::tuple_text(const HalconCpp::HTuple& t) const
{
    std::string s;
    for (int i = 0; i < t.Length(); ++i) {
        if (i) {
            s += " ";
        }
        const HalconCpp::HTupleElement e = t[i];
        if (e.Type() == HalconCpp::eElementTypeString) {
            s += e.S().Text();
        } else {
            char buf[64];
            sprintf_s(buf, "%.17g", e.D());
            s += buf;
        }
    }
    return s;
}

int HalCalibPlate::proc_input(const AlgToolIn& in)
{
    ToolParaCalibPlate local;
    const ToolParaCalibPlate* para = paraOf(in, &local);
    if (in.param != nullptr && toolParaAs<ToolParaCalibPlate>(in.param) == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    rows_ = para->run.rows;
    cols_ = para->run.cols;
    spacing_mm_ = para->run.spacingMm;
    min_images_ = para->run.minImages < 1 ? 3 : para->run.minImages;
    image_dir_ = para->run.imageDir;
    if (rows_ < 2 || cols_ < 2 || spacing_mm_ <= 0.0 || image_dir_.empty()) {
        shot_err_ = HS_ERR_PARAM;
        shot_msg_ = "请填写标定板行列、间距和图片目录";
        return shot_err_;
    }
    image_files_ = list_images(image_dir_);
    if (image_files_.empty()) {
        shot_err_ = HS_ERR_FILE_OPEN;
        shot_msg_ = "离线目录里没有图片";
        return shot_err_;
    }
    return HS_OK;
}

int HalCalibPlate::alg_run(const AlgToolIn& in)
{
    (void)in;
    try {
        initFileNames();
        const std::string descr = pathJoin(image_dir_, "caltab.descr");
        const std::string ps = pathJoin(image_dir_, "caltab.ps");
        HalconCpp::GenCaltab(cols_, rows_, spacing_mm_ / 1000.0, 0.5, descr.c_str(), ps.c_str());
        HalconCpp::HObject first;
        HalconCpp::ReadImage(&first, image_files_[0].c_str());
        HalconCpp::HTuple w;
        HalconCpp::HTuple h;
        HalconCpp::GetImageSize(first, &w, &h);
        const int iw = static_cast<int>(w[0].L());
        const int ih = static_cast<int>(h[0].L());
        HalconCpp::HTuple start;
        start.Append("area_scan_division");
        start.Append(0.008);
        start.Append(0.0);
        start.Append(8.0e-6);
        start.Append(8.0e-6);
        start.Append(iw * 0.5);
        start.Append(ih * 0.5);
        start.Append(iw);
        start.Append(ih);
        HalconCpp::HTuple data;
        HalconCpp::CreateCalibData("calibration_object", 1, 1, &data);
        HalconCpp::SetCalibDataCalibObject(data, 0, descr.c_str());
        HalconCpp::SetCalibDataCamParam(data, HalconCpp::HTuple(0), "area_scan_division", start);
        int used = 0;
        for (size_t i = 0; i < image_files_.size(); ++i) {
            try {
                HalconCpp::HObject img;
                HalconCpp::ReadImage(&img, image_files_[i].c_str());
                HalconCpp::FindCalibObject(img, data, 0, 0, static_cast<Hlong>(used), HalconCpp::HTuple(), HalconCpp::HTuple());
                ++used;
            } catch (const HalconCpp::HException&) {
            }
        }
        if (used < min_images_) {
            HalconCpp::ClearCalibData(data);
            shot_err_ = HS_ERR_PARAM;
            shot_msg_ = "找到标定板的图片不够";
            tol_.reset();
            tol_.put_int(CalibPlateKey::count, used);
            return shot_err_;
        }
        HalconCpp::HTuple err;
        HalconCpp::CalibrateCameras(data, &err);
        const double rms = err.TupleLength() > 0 ? err[0].D() : 0.0;
        HalconCpp::HTuple cam;
        HalconCpp::GetCalibData(data, "camera", HalconCpp::HTuple(0), "params", &cam);
        HalconCpp::ClearCalibData(data);
        const std::string text = tuple_text(cam);
        double focus = 0.008;
        double sx = 8.0e-6;
        double cx = iw * 0.5;
        double cy = ih * 0.5;
        double kappa = 0.0;
        if (cam.Length() >= 8) {
            focus = cam[1].D();
            kappa = cam[2].D();
            sx = cam[3].D();
            cx = cam[5].D();
            cy = cam[6].D();
        }
        const double fx = (sx != 0.0) ? focus / sx : 0.0;
        tol_.reset();
        tol_.put("fx", fx);
        tol_.put("fy", fx);
        tol_.put("cx", cx);
        tol_.put("cy", cy);
        tol_.put("k1", kappa);
        tol_.put("k2", 0.0);
        tol_.put("k3", 0.0);
        tol_.put("p1", 0.0);
        tol_.put("p2", 0.0);
        tol_.put("rms", rms);
        tol_.put("maxResidual", rms);
        tol_.put_int(CalibPlateKey::count, used);
        tol_.put_int("width", iw);
        tol_.put_int("height", ih);
        tol_.put_text(CalibPlateKey::halconCam, text);
        shot_err_ = HS_OK;
        shot_msg_ = "内参已算出";
        return HS_OK;
    } catch (const HalconCpp::HException& e) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = hExceptionText(e);
        return shot_err_;
    }
}

int HalCalibPlate::judge_tol(const AlgToolIn& in)
{
    ToolParaCalibPlate local;
    const ToolParaCalibPlate* para = paraOf(in, &local);
    tol_.judge(para->judge, nullptr);
    return HS_OK;
}

int HalCalibPlate::alg_rst_proc(const AlgToolIn& in, AlgToolOut* out)
{
    (void)in;
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    AlgToolOut result;
    result.errCode = shot_err_;
    result.runOk = shot_err_ == HS_OK;
    result.ok = shot_err_ == HS_OK && tol_.ok;
    result.message = shot_msg_;
    if (shot_err_ == HS_OK) {
        tol_.apply(&result);
        result.ok = tol_.ok;
    } else if (tol_.values.size() > 0) {
        tol_.apply(&result);
        result.ok = false;
    }
    *out = result;
    return shot_err_;
}

int HalCalibPlate::execute(const AlgToolIn& in, AlgToolOut* out)
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
        } else if (shot_err_ == HS_ERR_PARAM && tol_.values.size() > 0) {
            judge_tol(in);
        }
        alg_rst_proc(in, out);
        run_timer_.stop("alg_rst_proc");
        write_alg_steps(out, run_timer_.sections(), run_timer_.totalMs());
        return step != HS_OK ? step : shot_err_;
    } catch (const HalconCpp::HException& e) {
        return fail(HS_ERR_OPERATOR, hExceptionText(e), out);
    }
}

}
