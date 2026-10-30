/**
 * @file HardwareProbe.h
 * @brief CPU、BIOS、系统所在内置盘。管理员和标准用户走同一条读取
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HARDWARE_PROBE_H
#define VISION_HARDWARE_PROBE_H

#include <string>

namespace Vision {

/**
 * @class HardwareProbe
 * @brief 采集 CPU、SMBIOS、系统盘信息，拼接后取 SHA-256 作为本机硬件摘要
 */
class HardwareProbe {
public:
    /** @brief 摘要（64 位大写十六进制）。CPU 品牌和系统 UUID 都没有时返回空 */
    std::string digest() const;

private:
    /** @brief CPUID 厂商、family/model/stepping、品牌串。都取不到时为空 */
    std::string cpuPart() const;
    /**
     * @brief 解析 SMBIOS 原始表：系统 UUID/序列号、主板、BIOS 厂商与版本
     * @param haveUuid 输出是否取到可用的系统 UUID，可空
     * @return UUID 与主板序列号都没有时为空
     */
    std::string biosPart(bool* haveUuid) const;
    /** @brief 系统卷所在内置盘（SATA/ATA/NVMe/SCSI/SAS/RAID）的序列号。外接盘或取不到为空 */
    std::string diskPart() const;
    /** @brief 去首尾空白、连续空白压成一个空格、转大写 */
    std::string normalize(const std::string& text) const;
    /** @brief 排除空串、全 0、全 F 和厂商占位串（如 TO BE FILLED BY O.E.M.） */
    bool usable(const std::string& text) const;
};

}

#endif
