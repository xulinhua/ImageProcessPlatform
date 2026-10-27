/**
 * @file CvBlob.h
 * @brief CvBlob头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OPENCVBLOB_H
#define VISION_OPENCVBLOB_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class CvBlob
 * @brief 绝对、相对、动态阈值 Blob（OpenCV）
 */
class CvBlob : public AlgToolBase {
public:
    std::string toolName() const { return "detect.blob"; }
    AlgLibKind algLib() const { return AlgLibCv; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 在搜索域里做阈值分割并输出连通域 */
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new CvBlob(); }
};

}

#endif