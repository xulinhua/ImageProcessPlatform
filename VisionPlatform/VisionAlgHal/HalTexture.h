/**
 * @file HalTexture.h
 * @brief HalTexture头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONTEXTURE_H
#define VISION_HALCONTEXTURE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalTexture
 * @brief 用纹理模型比较缺陷（Halcon）
 */
class HalTexture : public AlgToolBase {
public:
    std::string toolName() const { return "detect.texture"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 用纹理模型比较缺陷 */
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new HalTexture(); }
};

}

#endif