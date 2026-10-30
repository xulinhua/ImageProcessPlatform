/**
 * @file DongleLicense.h
 * @brief 硬件狗空子类。本期不构造
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DONGLE_LICENSE_H
#define VISION_DONGLE_LICENSE_H

#include "LicenseSource.h"

namespace Vision {

/**
 * @class DongleLicense
 * @brief 硬件狗许可占位实现，校验一律返回未接入
 */
class VISION_LICENSE_API DongleLicense : public LicenseSource {
public:
    /** @brief 固定返回 LicenseNotAttached */
    virtual LicenseReason open();
    /** @brief 固定返回 LicenseNotAttached */
    virtual LicenseReason poll();
    /** @brief 返回默认许可信息 */
    virtual LicenseInfo describe() const;
    /** @brief 本机硬件摘要，与软许可同一算法 */
    virtual std::string machineId() const;
};

}

#endif
