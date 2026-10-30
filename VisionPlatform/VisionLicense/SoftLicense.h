/**
 * @file SoftLicense.h
 * @brief 软许可。申请码、注册码、许可证文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SOFT_LICENSE_H
#define VISION_SOFT_LICENSE_H

#include "LicenseSource.h"

namespace Vision {

/**
 * @class SoftLicense
 * @brief 绑定本机硬件摘要的软许可：申请码、验注册码、落地并复查许可证文件
 */
class VISION_LICENSE_API SoftLicense : public LicenseSource {
public:
    /************************************************** 许可来源 **************************************************/
    /** @brief 尚未建立许可 */
    SoftLicense();
    /** @brief 校验许可证文件并刷新三处时间记录 */
    virtual LicenseReason open();
    /** @brief 与 open 相同：重新校验文件并刷新时间记录 */
    virtual LicenseReason poll();
    /** @brief 最近一次校验通过时解出的许可信息；未通过时保持上次的值 */
    virtual LicenseInfo describe() const;
    /** @brief 现算本机硬件摘要（HardwareProbe），硬件不够时为空 */
    virtual std::string machineId() const;

    /************************************************** 注册 **************************************************/
    /** @brief 按当前硬件现算申请码。硬件不够时为空 */
    std::string requestCode() const;
    /** @brief 验注册码。序号更大才覆盖许可证文件，并重写三处时间记录 */
    LicenseReason accept(const std::string& registrationCode);

private:
    /**
     * @brief 读许可证文件，验签、产品、硬件、到期日和时间记录
     * @param writeRecords 通过后是否把现在写进三处时间记录
     * @return 全部通过返回 LicenseOk
     */
    LicenseReason checkFile(bool writeRecords);
    std::string m_digest;   ///< 本机硬件摘要，checkFile / accept 时更新
    LicenseInfo m_info;     ///< 最近一次通过时的许可信息
    bool m_ready;           ///< 最近一次校验或注册是否通过
};

}

#endif
