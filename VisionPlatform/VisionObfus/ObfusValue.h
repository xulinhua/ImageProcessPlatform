/**
 * @file ObfusValue.h
 * @brief 虚拟机寄存器里的一个值。整数按 64 位，指针只给外部调用用
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OBFUS_VALUE_H
#define VISION_OBFUS_VALUE_H

#include "VisionObfusExport.h"
#include <cstdint>

namespace Vision {

/**
 * @class ObfusValue
 * @brief 64 位寄存器值，可按整数或指针读写，同一份位模式
 */
class VISION_OBFUS_API ObfusValue {
public:
    /************************************************** 构造 **************************************************/
    /** @brief 值为 0 */
    ObfusValue();
    /** @brief 用整数初始化 */
    explicit ObfusValue(std::int64_t v);

    /************************************************** 读写 **************************************************/
    /** @brief 按有符号 64 位整数读 */
    std::int64_t asInt() const;
    /** @brief 按整数写 */
    void setInt(std::int64_t v);

    /** @brief 指针宽度的值。只在 CallOut 的参数和返回值里使用 */
    void* asPtr() const;
    /** @brief 写入指针，32 位进程高位为 0 */
    void setPtr(void* p);

private:
    std::uint64_t m_bits;   ///< 原始 64 位，整数与指针共用
};

}

#endif
