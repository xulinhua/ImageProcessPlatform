/**
 * @file ObfusMachine.h
 * @brief 解释执行 ObfusProgram。跳转只改指令指针。本期没有业务调用方
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OBFUS_MACHINE_H
#define VISION_OBFUS_MACHINE_H

#include "ObfusProgram.h"
#include "ObfusValue.h"

namespace Vision {

/** @brief 虚拟机外面的函数。参数和返回值都是寄存器值，字节码里只存这张表的下标 */
typedef ObfusValue (*ObfusOutFn)(ObfusValue a, ObfusValue b);

/**
 * @class ObfusMachine
 * @brief 寄存器式字节码解释器，无状态，可在多线程下共用
 */
class VISION_OBFUS_API ObfusMachine {
public:
    /**
     * @brief 从入口寄存器初值开始执行
     * 程序空、操作码不认识、跳转越界、CallOut 没有对应函数时 *ok 为 false，不抛异常
     * @param program 已解码的程序
     * @param entry 前 entryCount 个寄存器的初值，其余寄存器为 0
     * @param entryCount 初值个数，不能超过寄存器数
     * @param calls 外部函数表，CallOut 按下标调用
     * @param callCount 外部函数个数
     * @param ok 是否正常执行到 Return，可空；超过 100 万步或移位超过 63 也算失败
     * @return Return 指令指定的寄存器值；失败时为 0
     */
    ObfusValue run(const ObfusProgram& program,
                   const ObfusValue* entry,
                   int entryCount,
                   const ObfusOutFn* calls,
                   int callCount,
                   bool* ok) const;
};

}

#endif
