/**
 * @file ObfusMachine.cpp
 * @brief 逐条解释。失败只返回，不抛异常
 */

#include "ObfusMachine.h"
#include <cstdint>
#include <vector>

namespace Vision {

namespace {

const int kMaxSteps = 1000000;

bool regOk(int index, int regCount)
{
    return index >= 0 && index < regCount;
}

std::int64_t regInt(const std::vector<ObfusValue>& regs, int index)
{
    return regs[static_cast<std::size_t>(index)].asInt();
}

void setReg(std::vector<ObfusValue>* regs, int index, std::int64_t v)
{
    (*regs)[static_cast<std::size_t>(index)].setInt(v);
}

}

ObfusValue ObfusMachine::run(const ObfusProgram& program,
                             const ObfusValue* entry,
                             int entryCount,
                             const ObfusOutFn* calls,
                             int callCount,
                             bool* ok) const
{
    if (ok) {
        *ok = false;
    }
    if (!program.valid() || program.opCount() <= 0 || program.regCount() <= 0) {
        return ObfusValue();
    }
    if (entryCount < 0 || (entryCount > 0 && !entry)) {
        return ObfusValue();
    }
    if (entryCount > program.regCount()) {
        return ObfusValue();
    }
    if (callCount < 0 || (callCount > 0 && !calls)) {
        return ObfusValue();
    }

    std::vector<ObfusValue> regs(static_cast<std::size_t>(program.regCount()));
    for (int i = 0; i < entryCount; ++i) {
        regs[static_cast<std::size_t>(i)] = entry[i];
    }

    int ip = program.entry();
    for (int step = 0; step < kMaxSteps; ++step) {
        if (ip < 0 || ip >= program.opCount()) {
            return ObfusValue();
        }
        const ObfusOp op = program.opAt(ip);
        const int code = op.code();
        const int dest = op.dest();
        const int src0 = op.src0();
        const int src1 = op.src1();
        if (code == ObfusJump) {
            ip = src0;
            continue;
        }
        if (code == ObfusJumpIf) {
            if (!regOk(dest, program.regCount())) {
                return ObfusValue();
            }
            if (regInt(regs, dest) != 0) {
                ip = src0;
                continue;
            }
            ++ip;
            continue;
        }
        if (code == ObfusReturn) {
            if (!regOk(dest, program.regCount())) {
                return ObfusValue();
            }
            if (ok) {
                *ok = true;
            }
            return regs[static_cast<std::size_t>(dest)];
        }
        if (code == ObfusLoadImm) {
            if (!regOk(dest, program.regCount()) || src0 < 0 || src0 >= program.constantCount()) {
                return ObfusValue();
            }
            setReg(&regs, dest, program.constant(src0));
        } else if (code == ObfusMove) {
            if (!regOk(dest, program.regCount()) || !regOk(src0, program.regCount())) {
                return ObfusValue();
            }
            regs[static_cast<std::size_t>(dest)] = regs[static_cast<std::size_t>(src0)];
        } else if (code == ObfusAdd || code == ObfusSub || code == ObfusXor
                   || code == ObfusAnd || code == ObfusShl || code == ObfusCmp) {
            if (!regOk(dest, program.regCount()) || !regOk(src0, program.regCount())
                || !regOk(src1, program.regCount())) {
                return ObfusValue();
            }
            const std::uint64_t a = static_cast<std::uint64_t>(regInt(regs, src0));
            const std::uint64_t b = static_cast<std::uint64_t>(regInt(regs, src1));
            std::uint64_t r = 0;
            if (code == ObfusAdd) {
                r = a + b;
            } else if (code == ObfusSub) {
                r = a - b;
            } else if (code == ObfusXor) {
                r = a ^ b;
            } else if (code == ObfusAnd) {
                r = a & b;
            } else if (code == ObfusShl) {
                if (b > 63) {
                    return ObfusValue();
                }
                r = a << b;
            } else {
                r = a == b ? 1u : 0u;
            }
            setReg(&regs, dest, static_cast<std::int64_t>(r));
        } else if (code == ObfusCallOut) {
            const int arg0 = src1 & 0xff;
            const int arg1 = (src1 >> 8) & 0xff;
            if (!regOk(dest, program.regCount()) || !regOk(arg0, program.regCount())
                || !regOk(arg1, program.regCount()) || src0 < 0 || src0 >= callCount || !calls[src0]) {
                return ObfusValue();
            }
            regs[static_cast<std::size_t>(dest)] = calls[src0](regs[static_cast<std::size_t>(arg0)],
                                                               regs[static_cast<std::size_t>(arg1)]);
        } else {
            return ObfusValue();
        }
        ++ip;
    }
    return ObfusValue();
}

}
