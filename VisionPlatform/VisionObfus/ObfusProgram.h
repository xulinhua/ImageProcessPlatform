/**
 * @file ObfusProgram.h
 * @brief 一个函数的指令、常量池、寄存器数量和入口。由字节码构造，不是 C++ 源码
 */

#ifndef VISION_OBFUS_PROGRAM_H
#define VISION_OBFUS_PROGRAM_H

#include "ObfusOp.h"
#include "VisionObfusExport.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Vision {

class VISION_OBFUS_API ObfusProgram {
public:
    ObfusProgram();
    ObfusProgram(const unsigned char* bytes, int size);

    bool valid() const;
    int regCount() const;
    int entry() const;
    int opCount() const;
    int constantCount() const;
    ObfusOp opAt(int index) const;
    std::int64_t constant(int index) const;

    /**
     * @brief 把手写指令列表收成字节码。给开发机工具用，不扫描业务 cpp
     * 指令名：LoadImm Move Add Sub Xor And Shl Cmp Jump JumpIf CallOut Return
     * 头：regs、entry、const
     */
    static bool encode(const std::string& text, std::vector<unsigned char>* out, std::string* error);

private:
    bool m_valid;
    int m_regCount;
    int m_entry;
    std::vector<std::int64_t> m_constants;
    std::vector<ObfusOp> m_ops;
};

}

#endif
