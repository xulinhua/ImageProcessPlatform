/**
 * @file HalTmpl.h
 * @brief HalTmpl头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HALCONTEMPLATE_H
#define VISION_HALCONTEMPLATE_H

#include "AlgToolBase.h"

namespace Vision {

/**
 * @class HalTmpl
 * @brief NCC 与形状模板定位（Halcon）
 */
class HalTmpl : public AlgToolBase {
public:
    std::string toolName() const { return "locate.template"; }
    AlgLibKind algLib() const { return AlgLibHal; }
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 用 NCC 或形状模板定位 */
    AlgToolOut run(const AlgToolIn& in);
    AlgToolBase* clone() const { return new HalTmpl(); }
};

}

#endif