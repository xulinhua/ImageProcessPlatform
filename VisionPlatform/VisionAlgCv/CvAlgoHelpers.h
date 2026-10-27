/**
 * @file CvAlgoHelpers.h
 * @brief CvAlgoHelpers头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CV_ALGO_HELPERS_H
#define VISION_CV_ALGO_HELPERS_H

#include "AlgToolBase.h"
#include "opencvLib.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @brief 从 AlgToolIn 取灰度图与宽高
 * @param in 算法输入
 * @param gray 输出灰度
 * @param w 图宽
 * @param h 图高
 * @param err 失败原因
 */
bool loadGrayFromAlgIn(const AlgToolIn& in, cv::Mat*& gray, int* w, int* h, std::string* err);

/**
 * @brief 搜索域：本地 ROI 与上游区域的并集。无上游则仅本地，无本地 ROI 则仅上游
 */
cv::Mat searchDomainFromAlgIn(const std::vector<HsCommData::HsObject>& rois,
                                const std::vector<HsCommData::HsObject>& regions,
                                int imgW, int imgH);

/**
 * @brief 连通、按面积/圆度筛选后打包输出
 */
AlgToolOut packConnectedBlobs(const cv::Mat& bin, const AlgToolIn& in,
                                double minArea, double maxArea, double minCirc, double maxCirc,
                                const std::string& okMsg);

/**
 * @brief 预处理：灰度加 ROI 掩膜（work 为整图拷贝）
 */
bool preparePreprocessWork(const AlgToolIn& in, cv::Mat*& gray, cv::Mat*& work,
                           cv::Mat*& roiMask, std::string* err);

/**
 * @brief 预处理写回：仅 ROI 内像素覆盖到原图
 */
AlgToolOut finishPreprocessWork(const AlgToolIn& in, const cv::Mat& gray,
                                  const cv::Mat& work, const cv::Mat& roiMask,
                                  const std::string& okMsg);

/** @brief 失败输出（无图） */
AlgToolOut algorToolFail(const std::string& msg);

}

#endif
