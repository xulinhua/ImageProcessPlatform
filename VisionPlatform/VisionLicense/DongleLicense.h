/**
 * @file DongleLicense.h
 * @brief 硬件狗空子类。本期不构造
 */

#ifndef VISION_DONGLE_LICENSE_H
#define VISION_DONGLE_LICENSE_H

#include "LicenseSource.h"

namespace Vision {

class VISION_LICENSE_API DongleLicense : public LicenseSource {
public:
    virtual LicenseReason open();
    virtual LicenseReason poll();
    virtual LicenseInfo describe() const;
    virtual std::string machineId() const;
};

}

#endif
