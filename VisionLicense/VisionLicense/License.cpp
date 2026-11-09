/**
 * @file License.cpp
 * @brief 对外的复查接口和申请机接口。实现仍走内部的 SoftLicense
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "VisionLicense.h"
#include "VisionLicenseTool.h"
#include "SoftLicense.h"

namespace Vision {

class License::Impl {
public:
    SoftLicense core;
};

License::License()
    : m(new Impl)
{
}

License::~License()
{
    delete m;
}

LicenseReason License::open()
{
    return m->core.open();
}

LicenseReason License::poll()
{
    return m->core.poll();
}

std::string License::reasonText(LicenseReason reason) const
{
    return m->core.reasonText(reason);
}

LicenseView License::view() const
{
    const LicenseInfo info = m->core.describe();
    LicenseView out;
    out.licenseId = info.licenseId;
    out.until = info.until;
    out.perpetual = !info.licenseId.empty() && info.kind != LicenseUntil;
    out.serial = info.serial;
    return out;
}

class LicenseTool::Impl {
public:
    SoftLicense core;
};

LicenseTool::LicenseTool()
    : m(new Impl)
{
}

LicenseTool::~LicenseTool()
{
    delete m;
}

std::string LicenseTool::requestCode() const
{
    return m->core.requestCode();
}

LicenseReason LicenseTool::accept(const std::string& registrationCode)
{
    return m->core.accept(registrationCode);
}

LicenseReason LicenseTool::open()
{
    return m->core.open();
}

LicenseInfo LicenseTool::describe() const
{
    return m->core.describe();
}

std::string LicenseTool::reasonText(LicenseReason reason) const
{
    return m->core.reasonText(reason);
}

}
