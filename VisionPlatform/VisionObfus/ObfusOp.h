/**
 * @file ObfusOp.h
 * @brief 一条字节码指令。操作码、目标寄存器、两个源
 */

#ifndef VISION_OBFUS_OP_H
#define VISION_OBFUS_OP_H

#include "VisionObfusExport.h"

namespace Vision {

/** @brief 够表达一段没有 C++ 异常的整数逻辑 */
enum ObfusOpcode {
    ObfusLoadImm = 1,
    ObfusMove,
    ObfusAdd,
    ObfusSub,
    ObfusXor,
    ObfusAnd,
    ObfusShl,
    ObfusCmp,
    ObfusJump,
    ObfusJumpIf,
    ObfusCallOut,
    ObfusReturn
};

class VISION_OBFUS_API ObfusOp {
public:
    ObfusOp();
    ObfusOp(int code, int dest, int src0, int src1);

    int code() const;
    int dest() const;
    int src0() const;
    int src1() const;

private:
    int m_code;
    int m_dest;
    int m_src0;
    int m_src1;
};

}

#endif
