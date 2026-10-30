/**
 * @file ObfusProgram.h
 * @brief 一个函数的指令、常量池、寄存器数量和入口。由字节码构造，不是 C++ 源码
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OBFUS_PROGRAM_H
#define VISION_OBFUS_PROGRAM_H

#include "ObfusOp.h"
#include "VisionObfusExport.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class ObfusProgram
 * @brief VOB1 字节码解码后的只读程序；也提供把文本汇编编码成字节码的静态方法
 * @note 构造失败不抛异常，valid() 为 false
 */
class VISION_OBFUS_API ObfusProgram {
public:
    /************************************************** 解码 **************************************************/
    /** @brief 空程序，valid() 为 false */
    ObfusProgram();
    /**
     * @brief 解码 VOB1 字节码
     * @param bytes 字节码，可空
     * @param size 字节数，必须恰好用完，多余尾部视为无效
     */
    ObfusProgram(const unsigned char* bytes, int size);

    /************************************************** 查询 **************************************************/
    /** @brief 解码成功 */
    bool valid() const;
    /** @brief 寄存器个数，1~256 */
    int regCount() const;
    /** @brief 入口指令下标 */
    int entry() const;
    /** @brief 指令条数 */
    int opCount() const;
    /** @brief 常量池大小 */
    int constantCount() const;
    /** @brief 第 index 条指令，越界返回全 0 指令 */
    ObfusOp opAt(int index) const;
    /** @brief 第 index 个常量，越界返回 0 */
    std::int64_t constant(int index) const;

    /************************************************** 编码 **************************************************/
    /**
     * @brief 把手写指令列表收成字节码。给开发机工具用，不扫描业务 cpp
     * 指令名：LoadImm Move Add Sub Xor And Shl Cmp Jump JumpIf CallOut Return
     * 头：regs、entry、const
     * @param text 每行一条，# 开头为注释；regs 须在指令前，const 须在引用它的 LoadImm 前
     * @param out 输出字节码，失败时清空
     * @param error 失败原因（英文，带行号），可空
     * @return 编码成功返回 true
     */
    static bool encode(const std::string& text, std::vector<unsigned char>* out, std::string* error);

private:
    bool m_valid;                            ///< 字节码解码成功
    int m_regCount;                          ///< 寄存器个数
    int m_entry;                             ///< 入口指令下标
    std::vector<std::int64_t> m_constants;   ///< 常量池，LoadImm 按下标取
    std::vector<ObfusOp> m_ops;              ///< 指令表
};

}

#endif
