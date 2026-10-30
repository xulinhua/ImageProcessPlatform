/**
 * @file ObfusOp.h
 * @brief 一条字节码指令。操作码、目标寄存器、两个源
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OBFUS_OP_H
#define VISION_OBFUS_OP_H

#include "VisionObfusExport.h"

namespace Vision {

/** @brief 够表达一段没有 C++ 异常的整数逻辑 */
enum ObfusOpcode {
    ObfusLoadImm = 1,   ///< dest = 常量表[src0]
    ObfusMove,          ///< dest = src0 寄存器
    ObfusAdd,           ///< dest = src0 + src1，按 64 位无符号回绕
    ObfusSub,           ///< dest = src0 - src1，按 64 位无符号回绕
    ObfusXor,           ///< dest = src0 ^ src1
    ObfusAnd,           ///< dest = src0 & src1
    ObfusShl,           ///< dest = src0 << src1，移位数超过 63 时执行失败
    ObfusCmp,           ///< dest = (src0 == src1) ? 1 : 0
    ObfusJump,          ///< 无条件跳到指令下标 src0
    ObfusJumpIf,        ///< dest 寄存器非 0 时跳到指令下标 src0
    ObfusCallOut,       ///< dest = 外部函数表[src0](低 8 位寄存器, 次 8 位寄存器)，参数寄存器打包在 src1
    ObfusReturn         ///< 结束，返回 dest 寄存器
};

/**
 * @class ObfusOp
 * @brief 一条指令：操作码加三个整数操作数，含义随操作码变化（见 ObfusOpcode）
 */
class VISION_OBFUS_API ObfusOp {
public:
    /************************************************** 构造 **************************************************/
    /** @brief 全 0，操作码 0 不合法 */
    ObfusOp();
    /** @brief 按操作码和三个操作数构造，不做校验 */
    ObfusOp(int code, int dest, int src0, int src1);

    /************************************************** 字段 **************************************************/
    /** @brief 操作码，取值见 ObfusOpcode */
    int code() const;
    /** @brief 目标寄存器（JumpIf、Return 为被读的寄存器） */
    int dest() const;
    /** @brief 第一操作数：寄存器、常量下标、跳转目标或外部函数下标 */
    int src0() const;
    /** @brief 第二操作数：寄存器，CallOut 时为打包的两个参数寄存器 */
    int src1() const;

private:
    int m_code;   ///< 操作码
    int m_dest;   ///< 目标寄存器
    int m_src0;   ///< 第一操作数
    int m_src1;   ///< 第二操作数
};

}

#endif
