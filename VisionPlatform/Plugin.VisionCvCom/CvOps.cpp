/**
 * @file CvOps.cpp
 * @brief CvOps实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CvOps.h"

#include "CvObjectConvert.h"

namespace Vision {

// 掩膜开闭运算。空掩膜直接返回；半径过小则跳过
void morphOpenClose(const cv::Mat& bin, double openR, double closeR, cv::Mat*& out)
{
    if (!out) {
        return;
    }
    *out = bin.clone();
    if (maskIsEmpty(*out)) {
        return;
    }
    if (openR > 0.4) {
        int k = static_cast<int>(openR * 2.0 + 0.5);
        if (k < 3) {
            k = 3;
        }
        if ((k % 2) == 0) {
            ++k;
        }
        cv::Mat ker = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
        cv::morphologyEx(*out, *out, cv::MORPH_OPEN, ker);
    }
    if (closeR > 0.4 && !maskIsEmpty(*out)) {
        int k = static_cast<int>(closeR * 2.0 + 0.5);
        if (k < 3) {
            k = 3;
        }
        if ((k % 2) == 0) {
            ++k;
        }
        cv::Mat ker = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
        cv::morphologyEx(*out, *out, cv::MORPH_CLOSE, ker);
    }
}

}
