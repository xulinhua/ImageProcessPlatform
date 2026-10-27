/**
 * @file CvCalibPlate.cpp
 * @brief CvCalibPlate实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvCalibPlate.h"
#include "TextEncoding.h"
#include "SolutionDataPaths.h"
#include "opencvLib.h"
#include <windows.h>
#include <cmath>
#include <string>
#include <vector>

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
        if (name.size() >= n) {
            const std::wstring tail = name.substr(name.size() - n);
            if (_wcsicmp(tail.c_str(), ext[i]) == 0) {
                return true;
            }
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
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        if (!isImageName(fd.cFileName)) {
            continue;
        }
        char name[512];
        WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, (int)sizeof(name), 0, 0);
        out.push_back(Vision::pathJoin(dir, name));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

void objectGrid(int rows, int cols, double spacing, std::vector<cv::Point3f>* obj)
{
    obj->clear();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            obj->push_back(cv::Point3f(static_cast<float>(c * spacing), static_cast<float>(r * spacing), 0.f));
        }
    }
}

bool findBoard(const cv::Mat& gray, int rows, int cols, int plateKind, std::vector<cv::Point2f>* corners)
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

void putModel(Vision::AlgToolOut* out, const cv::Mat& K, const cv::Mat& dist, double rms, double maxRes, int count, int w, int h)
{
    out->values.insert("fx", K.at<double>(0, 0));
    out->values.insert("fy", K.at<double>(1, 1));
    out->values.insert("cx", K.at<double>(0, 2));
    out->values.insert("cy", K.at<double>(1, 2));
    out->values.insert("k1", dist.cols > 0 ? dist.at<double>(0) : 0.0);
    out->values.insert("k2", dist.cols > 1 ? dist.at<double>(1) : 0.0);
    out->values.insert("p1", dist.cols > 2 ? dist.at<double>(2) : 0.0);
    out->values.insert("p2", dist.cols > 3 ? dist.at<double>(3) : 0.0);
    out->values.insert("k3", dist.cols > 4 ? dist.at<double>(4) : 0.0);
    out->values.insert("rms", rms);
    out->values.insert("maxResidual", maxRes);
    out->values.insert("count", count);
    out->values.insert("width", w);
    out->values.insert("height", h);
    out->ok = true;
}

std::string packPoints(const std::vector<cv::Point2f>& img, const std::vector<cv::Point3f>& obj)
{
    std::string s;
    char buf[128];
    for (size_t i = 0; i < img.size() && i < obj.size(); ++i) {
        sprintf_s(buf, "%.6f,%.6f,%.6f,%.6f\n", img[i].x, img[i].y, obj[i].x, obj[i].y);
        s += buf;
    }
    return s;
}

}

// 找标定板并求内参，或只导出一张板的点
namespace {
const char kVer_CvCalibPlate[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_CvCalibPlate[] = {
    { "1.0.0.0", "首版" },
};
}

std::string Vision::CvCalibPlate::version() const
{
    return kVer_CvCalibPlate;
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::CvCalibPlate::versionNotes() const
{
    return Vision::verNotesFrom(kNotes_CvCalibPlate, static_cast<int>(sizeof(kNotes_CvCalibPlate) / sizeof(kNotes_CvCalibPlate[0])));
}
Vision::AlgToolOut Vision::CvCalibPlate::run(const Vision::AlgToolIn& in)
{
    AlgToolOut out;
    const int rows = in.run.value("rows").toInt(0);
    const int cols = in.run.value("cols").toInt(0);
    const double spacing = in.run.value("spacingMm").toDouble(0);
    const int plateKind = in.run.value("plateKind").toInt(0);
    const int minImages = in.run.value("minImages").toInt(3);
    const bool pointsOnly = in.run.value("pointsOnly").toInt(0) != 0;
    const std::string dir = in.run.value("imageDir").toString();
    if (rows < 2 || cols < 2 || spacing <= 0) {
        out.message = U(L"\u8bf7\u5148\u5728\u6807\u5b9a\u7ba1\u7406\u91cc\u586b\u597d\u884c\u5217\u548c\u95f4\u8ddd");
        return out;
    }
    std::vector<std::string> files = listImages(dir);
    if (files.empty()) {
        out.message = U(L"\u79bb\u7ebf\u76ee\u5f55\u91cc\u6ca1\u6709\u56fe\u7247");
        return out;
    }
    std::vector<std::vector<cv::Point3f> > objAll;
    std::vector<std::vector<cv::Point2f> > imgAll;
    cv::Size imageSize;
    std::vector<cv::Point3f> grid;
    objectGrid(rows, cols, spacing, &grid);
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
        if (!findBoard(gray, rows, cols, plateKind, &corners)) {
            continue;
        }
        if (pointsOnly) {
            out.values.insert("points", packPoints(corners, grid));
            out.values.insert("width", gray.cols);
            out.values.insert("height", gray.rows);
            out.ok = true;
            out.message = U(L"\u5df2\u627e\u5230\u6807\u5b9a\u677f");
            return out;
        }
        imageSize = gray.size();
        objAll.push_back(grid);
        imgAll.push_back(corners);
    }
    if (static_cast<int>(imgAll.size()) < minImages) {
        out.message = U(L"\u627e\u5230\u6807\u5b9a\u677f\u7684\u56fe\u7247\u4e0d\u591f");
        out.values.insert("count", static_cast<int>(imgAll.size()));
        return out;
    }
    cv::Mat K, dist;
    std::vector<cv::Mat> rvecs, tvecs;
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
    putModel(&out, K, dist, rms, maxRes, static_cast<int>(imgAll.size()), imageSize.width, imageSize.height);
    out.message = U(L"\u5185\u53c2\u5df2\u7b97\u51fa");
    return out;
}

// 只用第一张能找到板的图，给出像素点和毫米点
namespace {
const char kVer_CvCalibPlane[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_CvCalibPlane[] = {
    { "1.0.0.0", "首版" },
};
}

std::string Vision::CvCalibPlane::version() const
{
    return kVer_CvCalibPlane;
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::CvCalibPlane::versionNotes() const
{
    return Vision::verNotesFrom(kNotes_CvCalibPlane, static_cast<int>(sizeof(kNotes_CvCalibPlane) / sizeof(kNotes_CvCalibPlane[0])));
}
Vision::AlgToolOut Vision::CvCalibPlane::run(const Vision::AlgToolIn& in)
{
    AlgToolIn copy = in;
    copy.run.insert("pointsOnly", 1);
    copy.run.insert("minImages", 1);
    return CvCalibPlate().run(copy);
}
