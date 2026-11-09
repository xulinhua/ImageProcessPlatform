/**
 * @file DongleLicense.cpp
 * @brief 硬件狗空实现。运行时不构造
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DongleLicense.h"
#include "HardwareProbe.h"

namespace Vision {

LicenseReason DongleLicense::open()
{
    return LicenseNotAttached;
}

LicenseReason DongleLicense::poll()
{
    return LicenseNotAttached;
}

LicenseInfo DongleLicense::describe() const
{
    return LicenseInfo();
}

std::string DongleLicense::machineId() const
{
    return HardwareProbe().digest();
}

}
