/**
 * @file CvAlgoHelpers.cpp
 * @brief CvAlgoHelpers实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvAlgoHelpers.h"

#include "TextEncoding.h"
#include "CvImageConvert.h"
#include "CvObjectConvert.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Vision {

// 从输入取灰度与尺寸
bool loadGrayFromAlgIn(const AlgToolIn& in, cv::Mat*& gray, int* w, int* h, std::string* err)
{
    if (inputImage(in).isNull()) {
        if (err) {
            *err = "无输入图";
        }
        return false;
    }
    if (!hsImageToMatGray(inputImage(in), gray, err)) {
        return false;
    }
    *w = gray->cols;
    *h = gray->rows;
    return true;
}

// 本地 ROI 与上游区域并集。空 ROI 列表表示无本地约束
cv::Mat searchDomainFromAlgIn(const std::vector<HsCommData::HsObject>& rois,
                                const std::vector<HsCommData::HsObject>& regions,
                                int imgW, int imgH)
{
    if (regions.empty()) {
        return toolRoisToMask(rois, imgW, imgH);
    }
    cv::Mat mask = hsRegionsToMask(regions, imgW, imgH);
    if (rois.empty()) {
        return mask;
    }
    cv::Mat local = toolRoisToMask(rois, imgW, imgH);
    cv::Mat uni;
    cv::bitwise_or(local, mask, uni);
    if (maskIsEmpty(uni)) {
        return local;
    }
    return uni;
}

// 连通域筛选打包
AlgToolOut packConnectedBlobs(const cv::Mat& bin, const AlgToolIn& in,
                                double minArea, double maxArea, double minCirc, double maxCirc,
                                const std::string& okMsg)
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
    AlgToolOut out;
    out.image.image = inputImage(in);
    if (count < 1) {
        out.message = "未找到连通域";
        return out;
    }
    out.ok = true;
    out.values.insert("count", count);
    out.values.insert("area", sumA);
    out.objects.push_back(maskToRegionObject(sel));
    out.message = okMsg;
    return out;
}

// ROI 内预处理底图
bool preparePreprocessWork(const AlgToolIn& in, cv::Mat*& gray, cv::Mat*& work, cv::Mat*& roiMask, std::string* err)
{
    int w = 0;
    int h = 0;
    if (!loadGrayFromAlgIn(in, gray, &w, &h, err)) {
        return false;
    }
    *roiMask = toolRoisToMask(plainRois(in.rois), w, h);
    *work = gray->clone();
    return true;
}

// 仅 ROI 写回
AlgToolOut finishPreprocessWork(const AlgToolIn& in, const cv::Mat& gray, const cv::Mat& work,
                                  const cv::Mat& roiMask, const std::string& okMsg)
{
    cv::Mat merged = gray.clone();
    work.copyTo(merged, roiMask);
    AlgToolOut out;
    std::string err;
    HsCommData::HsImage* pImage = &out.image.image;
    if (!matToHsImage(merged, inputImage(in).frameId, pImage, &err)) {
        return algorToolFail(err);
    }
    out.ok = true;
    out.message = okMsg;
    return out;
}

// 统一失败包
AlgToolOut algorToolFail(const std::string& msg)
{
    AlgToolOut o;
    o.ok = false;
    o.errCode = 1; o.message = msg;
    return o;
}

}
