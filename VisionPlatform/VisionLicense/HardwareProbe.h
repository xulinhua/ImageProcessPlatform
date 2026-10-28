/**
 * @file HardwareProbe.h
 * @brief CPU、BIOS、系统所在内置盘。管理员和标准用户走同一条读取
 */

#ifndef VISION_HARDWARE_PROBE_H
#define VISION_HARDWARE_PROBE_H

#include <string>

namespace Vision {

class HardwareProbe {
public:
    /** @brief 摘要。CPU 品牌和系统 UUID 都没有时返回空 */
    std::string digest() const;

private:
    std::string cpuPart() const;
    std::string biosPart(bool* haveUuid) const;
    std::string diskPart() const;
    std::string normalize(const std::string& text) const;
    bool usable(const std::string& text) const;
};

}

#endif
