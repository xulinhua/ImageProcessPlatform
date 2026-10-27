/**
 * @file CvFindCircle.cpp
 * @brief CvFindCircle实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvFindCircle.h"
#include "TextEncoding.h"
#include "CvAlgoHelpers.h"
#include "CvObjectConvert.h"
#include "ParamKeys.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Vision {

// 找圆：轮廓圆度筛选
namespace {
const char kVer_CvFindCircle[] = "1.0.0.0";
const Vision::VerNoteRaw kNotes_CvFindCircle[] = {
    { "1.0.0.0", "首版" },
};
}

std::string CvFindCircle::version() const
{
    return kVer_CvFindCircle;
}

std::vector<Vision::VerNote> CvFindCircle::versionNotes() const
{
    return Vision::verNotesFrom(kNotes_CvFindCircle, static_cast<int>(sizeof(kNotes_CvFindCircle) / sizeof(kNotes_CvFindCircle[0])));
}
AlgToolOut CvFindCircle::run(const AlgToolIn& in)
{
    try {
        cv::Mat gray;
        int w = 0;
        int h = 0;
        std::string err;
        cv::Mat* pGray = &gray;
        if (!loadGrayFromAlgIn(in, pGray, &w, &h, &err)) {
            return algorToolFail(err);
        }
        const double sigma = in.run.value("sigma").toDouble(1.0);
        const double low = in.run.value("low").toDouble(20.0);
        const double high = in.run.value("high").toDouble(40.0);
        const double minR = in.run.value("minR").toDouble(8.0);
        const double maxR = in.run.value("maxR").toDouble(400.0);
        cv::Mat mask = toolRoisToMask(plainRois(in.rois), w, h);
        cv::Mat local;
        gray.copyTo(local, mask);
        cv::Mat blur;
        const double s = sigma < 0.4 ? 0.4 : sigma;
        int ksize = static_cast<int>(s * 6.0 + 1.0);
        if (ksize < 3) {
            ksize = 3;
        }
        if ((ksize % 2) == 0) {
            ++ksize;
        }
        cv::GaussianBlur(local, blur, cv::Size(ksize, ksize), s);
        cv::Mat edges;
        cv::Canny(blur, edges, low, high);
        cv::bitwise_and(edges, mask, edges);
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
            // 圆度过低则跳过
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
        AlgToolOut out;
        out.image.image = in.image.image;
        if (!found) {
            out.message = Vision::acpToUtf8("未找到圆");
            return out;
        }
        out.ok = true;
        out.values.insert("cx", bestCx);
        out.values.insert("cy", bestCy);
        out.values.insert("r", bestR);
        paramSet(&out.values, ValueKey::X, bestCx);
        paramSet(&out.values, ValueKey::Y, bestCy);
        paramSet(&out.values, ValueKey::Angle, 0.0);
        paramSet(&out.values, ValueKey::Score, 1.0);
        out.objects.push_back(circleObject(bestCx, bestCy, bestR));
        out.message = Vision::acpToUtf8("找圆完成");
        return out;
    } catch (const cv::Exception& e) {
        return algorToolFail(e.what() ? std::string(e.what()) : std::string("OpenCV error"));
    }
}

}
