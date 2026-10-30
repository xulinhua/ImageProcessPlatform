/**
 * @file ObfusValue.cpp
 * @brief 寄存器值的整数和指针两种读法
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ObfusValue.h"
#include <cstdint>

namespace Vision {

ObfusValue::ObfusValue()
    : m_bits(0)
{
}

ObfusValue::ObfusValue(std::int64_t v)
    : m_bits(static_cast<std::uint64_t>(v))
{
}

std::int64_t ObfusValue::asInt() const
{
    return static_cast<std::int64_t>(m_bits);
}

void ObfusValue::setInt(std::int64_t v)
{
    m_bits = static_cast<std::uint64_t>(v);
}

void* ObfusValue::asPtr() const
{
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(m_bits));
}

void ObfusValue::setPtr(void* p)
{
    m_bits = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p));
}

}
