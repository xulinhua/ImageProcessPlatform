/**
 * @file ObfusOp.cpp
 * @brief 一条指令的四个字段
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ObfusOp.h"

namespace Vision {

ObfusOp::ObfusOp()
    : m_code(0)
    , m_dest(0)
    , m_src0(0)
    , m_src1(0)
{
}

ObfusOp::ObfusOp(int code, int dest, int src0, int src1)
    : m_code(code)
    , m_dest(dest)
    , m_src0(src0)
    , m_src1(src1)
{
}

int ObfusOp::code() const
{
    return m_code;
}

int ObfusOp::dest() const
{
    return m_dest;
}

int ObfusOp::src0() const
{
    return m_src0;
}

int ObfusOp::src1() const
{
    return m_src1;
}

}
