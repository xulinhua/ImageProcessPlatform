/**
 * @file LicenseSource.h
 * @brief 运行时只认这个基类
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LICENSE_SOURCE_H
#define VISION_LICENSE_SOURCE_H

#include "LicenseTypes.h"

namespace Vision {

/**
 * @class LicenseSource
 * @brief 许可来源接口：软许可、硬件狗都从这里派生
 */
class VISION_LICENSE_API LicenseSource {
public:
    /** @brief 虚析构，允许经基类指针释放 */
    virtual ~LicenseSource();
    /** @brief 启动时建立许可。失败不倒计时 */
    virtual LicenseReason open() = 0;
    /** @brief 运行中复查。失败由调用方倒计时，这里不停止工站 */
    virtual LicenseReason poll() = 0;
    /** @brief 当前解开的种类和到期日 */
    virtual LicenseInfo describe() const = 0;
    /** @brief 本机硬件摘要。采集失败时为空 */
    virtual std::string machineId() const = 0;
    /** @brief 给界面的一句原因 */
    virtual std::string reasonText(LicenseReason reason) const;
};

}

#endif
