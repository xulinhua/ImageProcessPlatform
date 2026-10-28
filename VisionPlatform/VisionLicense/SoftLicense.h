/**
 * @file SoftLicense.h
 * @brief 软许可。申请码、注册码、许可证文件
 */

#ifndef VISION_SOFT_LICENSE_H
#define VISION_SOFT_LICENSE_H

#include "LicenseSource.h"

namespace Vision {

class VISION_LICENSE_API SoftLicense : public LicenseSource {
public:
    SoftLicense();
    virtual LicenseReason open();
    virtual LicenseReason poll();
    virtual LicenseInfo describe() const;
    virtual std::string machineId() const;
    /** @brief 按当前硬件现算申请码。硬件不够时为空 */
    std::string requestCode() const;
    /** @brief 验注册码。序号更大才覆盖许可证文件，并重写三处时间记录 */
    LicenseReason accept(const std::string& registrationCode);

private:
    LicenseReason checkFile(bool writeRecords);
    std::string m_digest;
    LicenseInfo m_info;
    bool m_ready;
};

}

#endif
