/**
 * @file ObfusMachine.h
 * @brief 解释执行 ObfusProgram。跳转只改指令指针。本期没有业务调用方
 */

#ifndef VISION_OBFUS_MACHINE_H
#define VISION_OBFUS_MACHINE_H

#include "ObfusProgram.h"
#include "ObfusValue.h"

namespace Vision {

/** @brief 虚拟机外面的函数。参数和返回值都是寄存器值，字节码里只存这张表的下标 */
typedef ObfusValue (*ObfusOutFn)(ObfusValue a, ObfusValue b);

class VISION_OBFUS_API ObfusMachine {
public:
    /**
     * @brief 从入口寄存器初值开始执行
     * 程序空、操作码不认识、跳转越界、CallOut 没有对应函数时 *ok 为 false，不抛异常
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
