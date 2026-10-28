/**
 * @file ObfusValue.h
 * @brief 虚拟机寄存器里的一个值。整数按 64 位，指针只给外部调用用
 */

#ifndef VISION_OBFUS_VALUE_H
#define VISION_OBFUS_VALUE_H

#include "VisionObfusExport.h"
#include <cstdint>

namespace Vision {

class VISION_OBFUS_API ObfusValue {
public:
    ObfusValue();
    explicit ObfusValue(std::int64_t v);

    std::int64_t asInt() const;
    void setInt(std::int64_t v);

    /** @brief 指针宽度的值。只在 CallOut 的参数和返回值里使用 */
    void* asPtr() const;
    void setPtr(void* p);

private:
    std::uint64_t m_bits;
};

}

#endif
