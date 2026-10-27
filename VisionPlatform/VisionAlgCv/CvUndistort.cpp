/**
 * @file CvUndistort.cpp
 * @brief CvUndistort实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvUndistort.h"
#include "CvImageConvert.h"
#include "opencvLib.h"
#include <map>
#include <mutex>
#include <string>

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

struct MapCache {
    std::string key;
    cv::Mat map1;
    cv::Mat map2;
};

std::mutex g_mu;
std::map<std::string, MapCache> g_maps;

}

// 按内参去畸变，模型时间不变则复用映射表
namespace {
const char kVer_CvUndistort[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_CvUndistort[] = {
    { "1.0.0.0", "首版" },
};
}

std::string Vision::CvUndistort::version() const
{
    return kVer_CvUndistort;
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::CvUndistort::versionNotes() const
{
    return Vision::verNotesFrom(kNotes_CvUndistort, static_cast<int>(sizeof(kNotes_CvUndistort) / sizeof(kNotes_CvUndistort[0])));
}
Vision::AlgToolOut Vision::CvUndistort::run(const Vision::AlgToolIn& in)
{
    AlgToolOut out;
    if (in.image.image.isNull()) {
        out.message = U(L"\u6ca1\u6709\u8f93\u5165\u56fe");
        return out;
    }
    cv::Mat src;
    cv::Mat* srcPtr = &src;
    std::string err;
    if (!hsImageToMat(in.image.image, srcPtr, &err)) {
        out.message = err;
        return out;
    }
    const double fx = in.run.value("fx").toDouble(0);
    const double fy = in.run.value("fy").toDouble(0);
    const double cx = in.run.value("cx").toDouble(0);
    const double cy = in.run.value("cy").toDouble(0);
    cv::Mat K = (cv::Mat_<double>(3, 3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
    cv::Mat dist = (cv::Mat_<double>(1, 5)
        << in.run.value("k1").toDouble(0), in.run.value("k2").toDouble(0),
           in.run.value("p1").toDouble(0), in.run.value("p2").toDouble(0),
           in.run.value("k3").toDouble(0));
    const std::string key = in.run.value("stamp").toString()
        + ":" + std::to_string(src.cols) + "x" + std::to_string(src.rows);
    cv::Mat map1, map2;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        std::map<std::string, MapCache>::iterator it = g_maps.find(key);
        if (it == g_maps.end()) {
            MapCache cache;
            cache.key = key;
            cv::initUndistortRectifyMap(K, dist, cv::Mat(), K, src.size(), CV_16SC2, cache.map1, cache.map2);
            g_maps[key] = cache;
            it = g_maps.find(key);
        }
        map1 = it->second.map1;
        map2 = it->second.map2;
    }
    cv::Mat dst;
    cv::remap(src, dst, map1, map2, cv::INTER_LINEAR);
    HsCommData::HsImage image;
    HsCommData::HsImage* imagePtr = &image;
    if (!matToHsImage(dst, in.image.image.frameId, imagePtr, &err)) {
        out.message = err;
        return out;
    }
    out.image.image = image;
    out.ok = true;
    out.message = U(L"\u5df2\u6821\u6b63");
    return out;
}
