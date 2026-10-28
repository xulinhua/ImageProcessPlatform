/**
 * @file LicenseSource.cpp
 * @brief 基类只提供原因文案
 */

#include "LicenseSource.h"

namespace Vision {

LicenseSource::~LicenseSource() {}

std::string LicenseSource::reasonText(LicenseReason reason) const
{
    switch (reason) {
    case LicenseOk: return "许可有效";
    case LicenseMissing: return "没有许可证";
    case LicenseBadSign: return "注册码签名不对";
    case LicenseMachine: return "硬件与许可证不一致";
    case LicenseProduct: return "不是本产品的许可证";
    case LicenseExpired: return "许可证已过期";
    case LicenseClock: return "系统时间被往回改";
    case LicenseRecordsCleared: return "时间记录被清掉";
    case LicenseDamaged: return "许可证文件已损坏";
    case LicenseSaveFailed: return "无法保存许可证";
    case LicenseNotAttached: return "硬件狗未接入";
    case LicenseSerial: return "注册码序号不是更新的";
    default: return "许可无效";
    }
}

}
