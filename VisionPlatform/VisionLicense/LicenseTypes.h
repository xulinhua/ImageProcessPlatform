/**
 * @file LicenseTypes.h
 * @brief 许可结果和种类。软许可与硬件狗共用
 */

#ifndef VISION_LICENSE_TYPES_H
#define VISION_LICENSE_TYPES_H

#include "VisionLicenseExport.h"
#include <string>

namespace Vision {

enum LicenseReason {
    LicenseOk = 0,
    LicenseMissing,         ///< 还没有许可证文件
    LicenseBadSign,         ///< 注册码签名不对，或前缀不是注册码
    LicenseMachine,         ///< 硬件摘要对不上，或解不开本机文件
    LicenseProduct,         ///< 不是本产品
    LicenseExpired,         ///< 到期日已过
    LicenseClock,           ///< 系统时间早于见过的最晚时间
    LicenseRecordsCleared,  ///< 三处时间记录都没有，许可证还在
    LicenseDamaged,         ///< 文件头不对
    LicenseSaveFailed,      ///< 许可证目录写不进去
    LicenseNotAttached,     ///< 硬件狗这一期没有接入
    LicenseSerial           ///< 签发序号不比已落地的更新
};

enum LicenseKind {
    LicensePerpetual = 0,
    LicenseUntil
};

struct VISION_LICENSE_API LicenseInfo {
    LicenseKind kind;
    std::string licenseId;
    std::string until; ///< 到期日 YYYY-MM-DD。永久注册时为空
    int serial;        ///< 签发序号。还没有许可证时是 0
    LicenseInfo() : kind(LicensePerpetual), serial(0) {}
};

}

#endif
