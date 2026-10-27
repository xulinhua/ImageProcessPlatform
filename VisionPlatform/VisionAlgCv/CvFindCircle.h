/**
 * @file CvFindCircle.h
 * @brief CvFindCircle头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVFINDCIRCLE_H
#define VISION_OPENCVFINDCIRCLE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class CvFindCircle
 * @brief 用 Canny 拟合圆（OpenCV）
 */
class CvFindCircle : public AlgToolBase {
public:
    std::string toolName() const { return "locate.find_circle"; }
    AlgLibKind algLib() const { return AlgLibCv; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 用 Canny 拟合圆 */
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new CvFindCircle(); }
};

}

#endif