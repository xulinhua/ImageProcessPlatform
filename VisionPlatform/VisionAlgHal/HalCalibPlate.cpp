/**
 * @file HalCalibPlate.cpp
 * @brief HalCalibPlate实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalCalibPlate.h"
#include "TextEncoding.h"
#include "SolutionDataPaths.h"
#include "HalconCpp.h"
#include <windows.h>
#include <string>
#include <vector>
#include <cstdlib>

namespace {

std::string U(const wchar_t* w)
{
    std::string o;
    if (!w) {
        return o;
    }
    for (; *w; ++w) {
        unsigned int c = static_cast<unsigned int>(*w);
        if (c < 0x80u) {
            o.push_back(static_cast<char>(c));
        } else if (c < 0x800u) {
            o.push_back(static_cast<char>(0xC0u | (c >> 6)));
            o.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
        } else {
            o.push_back(static_cast<char>(0xE0u | (c >> 12)));
            o.push_back(static_cast<char>(0x80u | ((c >> 6) & 0x3Fu)));
            o.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
        }
    }
    return o;
}

bool isImageName(const std::wstring& name)
{
    const wchar_t* ext[] = { L".bmp", L".jpg", L".jpeg", L".png", L".tif", L".tiff", 0 };
    for (int i = 0; ext[i]; ++i) {
        const size_t n = wcslen(ext[i]);
        if (name.size() >= n && _wcsicmp(name.c_str() + name.size() - n, ext[i]) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> listImages(const std::string& dir)
{
    std::vector<std::string> out;
    if (dir.empty()) {
        return out;
    }
    const std::wstring pattern = Vision::utf8ToWide(Vision::pathJoin(dir, "*"));
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return out;
    }
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && isImageName(fd.cFileName)) {
            char name[512];
            WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, (int)sizeof(name), 0, 0);
            out.push_back(Vision::pathJoin(dir, name));
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

std::string tupleText(const HalconCpp::HTuple& t)
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

}

// 用 Halcon 标定板描述文件找标记并求内参
namespace {
const char kVer_HalCalibPlate[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_HalCalibPlate[] = {
    { "1.0.0.0", "首版" },
};
}

std::string Vision::HalCalibPlate::version() const
{
    return kVer_HalCalibPlate;
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::HalCalibPlate::versionNotes() const
{
    return Vision::verNotesFrom(kNotes_HalCalibPlate, static_cast<int>(sizeof(kNotes_HalCalibPlate) / sizeof(kNotes_HalCalibPlate[0])));
}
Vision::AlgToolOut Vision::HalCalibPlate::run(const Vision::AlgToolIn& in)
{
    AlgToolOut out;
    const int rows = in.run.value("rows").toInt(0);
    const int cols = in.run.value("cols").toInt(0);
    const double spacingMm = in.run.value("spacingMm").toDouble(0);
    const int minImages = in.run.value("minImages").toInt(3);
    const std::string dir = in.run.value("imageDir").toString();
    if (rows < 2 || cols < 2 || spacingMm <= 0 || dir.empty()) {
        out.message = U(L"\u8bf7\u5148\u5728\u6807\u5b9a\u7ba1\u7406\u91cc\u586b\u597d\u884c\u5217\u548c\u95f4\u8ddd");
        return out;
    }
    const std::vector<std::string> files = listImages(dir);
    if (files.empty()) {
        out.message = U(L"\u79bb\u7ebf\u76ee\u5f55\u91cc\u6ca1\u6709\u56fe\u7247");
        return out;
    }
    try {
        // 按行列和间距在图片目录里生成 Halcon 标定板描述文件，初值用第一张图的尺寸
        const std::string descr = pathJoin(dir, "caltab.descr");
        const std::string ps = pathJoin(dir, "caltab.ps");
        HalconCpp::HMisc::GenCaltab(cols, rows, spacingMm / 1000.0, 0.5, descr.c_str(), ps.c_str());
        HalconCpp::HImage first(files[0].c_str());
        HalconCpp::HTuple w, h;
        first.GetImageSize(&w, &h);
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
        HalconCpp::HCalibData data("calibration_object", 1, 1);
        data.SetCalibDataCalibObject(0, descr.c_str());
        data.SetCalibDataCamParam(HalconCpp::HTuple(0), "area_scan_division", HalconCpp::HCamPar(start));
        // 逐张找板，找不到的图忽略，姿态序号只给找到的图递增
        int used = 0;
        for (size_t i = 0; i < files.size(); ++i) {
            try {
                HalconCpp::HImage img(files[i].c_str());
                data.FindCalibObject(img, 0, 0, static_cast<Hlong>(used), HalconCpp::HTuple(), HalconCpp::HTuple());
                ++used;
            } catch (const HalconCpp::HException&) {
            }
        }
        if (used < minImages) {
            out.message = U(L"\u627e\u5230\u6807\u5b9a\u677f\u7684\u56fe\u7247\u4e0d\u591f");
            out.values.insert("count", used);
            return out;
        }
        // 把 area_scan_division 参数折成 fx/fy/cx/cy；kappa 放进 k1，其余畸变项置 0，fy 取 fx
        const double rms = data.CalibrateCameras();
        const HalconCpp::HTuple cam = data.GetCalibData("camera", HalconCpp::HTuple(0), HalconCpp::HTuple("params"));
        const std::string text = tupleText(cam);
        double focus = 0.008;
        double sx = 8.0e-6;
        double cx = iw * 0.5;
        double cy = ih * 0.5;
        double kappa = 0;
        if (cam.Length() >= 8) {
            focus = cam[1].D();
            kappa = cam[2].D();
            sx = cam[3].D();
            cx = cam[5].D();
            cy = cam[6].D();
        }
        const double fx = (sx != 0.0) ? focus / sx : 0.0;
        out.values.insert("fx", fx);
        out.values.insert("fy", fx);
        out.values.insert("cx", cx);
        out.values.insert("cy", cy);
        out.values.insert("k1", kappa);
        out.values.insert("k2", 0.0);
        out.values.insert("k3", 0.0);
        out.values.insert("p1", 0.0);
        out.values.insert("p2", 0.0);
        out.values.insert("rms", rms);
        out.values.insert("maxResidual", rms);
        out.values.insert("count", used);
        out.values.insert("width", iw);
        out.values.insert("height", ih);
        out.values.insert("halconCam", text);
        out.ok = true;
        out.message = U(L"\u5185\u53c2\u5df2\u7b97\u51fa");
    } catch (const HalconCpp::HException& e) {
        out.message = e.ErrorMessage().Text();
    }
    return out;
}
