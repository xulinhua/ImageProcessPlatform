/**
 * @file LicenseTypes.h
 * @brief 许可结果和种类。软许可与硬件狗共用
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LICENSE_TYPES_H
#define VISION_LICENSE_TYPES_H

#include "VisionLicenseExport.h"
#include <string>

namespace Vision {

/** @brief 许可校验结果。文案见 LicenseSource::reasonText */
enum LicenseReason {
    LicenseOk = 0,          ///< 许可有效
    LicenseMissing,         ///< 还没有许可证文件
    LicenseBadSign,         ///< 注册码签名不对，或前缀不是注册码
    LicenseMachine,         ///< 硬件摘要对不上，或解不开本机文件
    LicenseProduct,         ///< 不是本产品
    LicenseExpired,         ///< 到期日已过
    LicenseClock,           ///< 系统时间早于见过的最晚时间
    LicenseRecordsCleared,  ///< 三处时间记录都没有，许可证还在
    LicenseDamaged,         ///< 文件头或长度不对，或文件内签名、正文校验不过
    LicenseSaveFailed,      ///< 许可证目录写不进去
    LicenseNotAttached,     ///< 硬件狗这一期没有接入
    LicenseSerial           ///< 签发序号不比已落地的更新
};

/** @brief 许可种类，对应载荷里的 kind 字段 */
enum LicenseKind {
    LicensePerpetual = 0,   ///< 永久（kind 为 perpetual）
    LicenseUntil            ///< 限期（kind 为 until），到期日见 LicenseInfo::until
};

/**
 * @class LicenseInfo
 * @brief 解开后的许可摘要，给界面显示
 */
struct VISION_LICENSE_API LicenseInfo {
    LicenseKind kind;       ///< 永久或限期
    std::string licenseId;  ///< 签发方给的许可编号
    std::string until; ///< 到期日 YYYY-MM-DD。永久注册时为空
    int serial;        ///< 签发序号。还没有许可证时是 0
    /** @brief 默认永久、序号 0 */
    LicenseInfo() : kind(LicensePerpetual), serial(0) {}
};

}

#endif
