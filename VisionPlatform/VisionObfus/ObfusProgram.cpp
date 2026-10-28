/**
 * @file ObfusProgram.cpp
 * @brief 字节码布局 VOB1：寄存器数、入口、常量池、指令
 */

#include "ObfusProgram.h"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace Vision {

namespace {

const int kMaxRegs = 256;
const unsigned kOpBytes = 6;

void appendU8(std::vector<unsigned char>* out, unsigned v)
{
    out->push_back(static_cast<unsigned char>(v & 0xffu));
}

void appendU16(std::vector<unsigned char>* out, unsigned v)
{
    appendU8(out, v);
    appendU8(out, v >> 8);
}

void appendU32(std::vector<unsigned char>* out, unsigned v)
{
    appendU16(out, v);
    appendU16(out, v >> 16);
}

void appendI64(std::vector<unsigned char>* out, std::int64_t v)
{
    const std::uint64_t u = static_cast<std::uint64_t>(v);
    for (int i = 0; i < 8; ++i) {
        appendU8(out, static_cast<unsigned>(u >> (8 * i)));
    }
}

bool readU8(const unsigned char* p, int n, int* off, unsigned* o)
{
    if (*off + 1 > n) {
        return false;
    }
    *o = p[*off];
    *off += 1;
    return true;
}

bool readU16(const unsigned char* p, int n, int* off, unsigned* o)
{
    unsigned lo = 0;
    unsigned hi = 0;
    if (!readU8(p, n, off, &lo) || !readU8(p, n, off, &hi)) {
        return false;
    }
    *o = lo | (hi << 8);
    return true;
}

bool readU32(const unsigned char* p, int n, int* off, unsigned* o)
{
    unsigned lo = 0;
    unsigned hi = 0;
    if (!readU16(p, n, off, &lo) || !readU16(p, n, off, &hi)) {
        return false;
    }
    *o = lo | (hi << 16);
    return true;
}

bool readI64(const unsigned char* p, int n, int* off, std::int64_t* o)
{
    std::uint64_t u = 0;
    for (int i = 0; i < 8; ++i) {
        unsigned b = 0;
        if (!readU8(p, n, off, &b)) {
            return false;
        }
        u |= static_cast<std::uint64_t>(b) << (8 * i);
    }
    *o = static_cast<std::int64_t>(u);
    return true;
}

std::string trim(const std::string& s)
{
    std::size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) {
        ++a;
    }
    std::size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) {
        --b;
    }
    return s.substr(a, b - a);
}

std::vector<std::string> tokens(const std::string& line)
{
    std::vector<std::string> out;
    std::string cur;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == ' ' || c == '\t' || c == ',') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

bool parseI64(const std::string& s, std::int64_t* out)
{
    if (s.empty()) {
        return false;
    }
    errno = 0;
    char* end = 0;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0' || errno == ERANGE) {
        return false;
    }
    *out = static_cast<std::int64_t>(v);
    return true;
}

bool parseReg(const std::string& s, int regCount, int* out, std::string* error, int lineNo)
{
    std::int64_t v = 0;
    if (!parseI64(s, &v) || v < 0 || v >= regCount) {
        if (error) {
            *error = "line " + std::to_string(lineNo) + ": register is out of range";
        }
        return false;
    }
    *out = static_cast<int>(v);
    return true;
}

int opcodeOf(const std::string& name)
{
    if (name == "LoadImm") return ObfusLoadImm;
    if (name == "Move") return ObfusMove;
    if (name == "Add") return ObfusAdd;
    if (name == "Sub") return ObfusSub;
    if (name == "Xor") return ObfusXor;
    if (name == "And") return ObfusAnd;
    if (name == "Shl") return ObfusShl;
    if (name == "Cmp") return ObfusCmp;
    if (name == "Jump") return ObfusJump;
    if (name == "JumpIf") return ObfusJumpIf;
    if (name == "CallOut") return ObfusCallOut;
    if (name == "Return") return ObfusReturn;
    return 0;
}

bool isBinary(int code)
{
    return code == ObfusAdd || code == ObfusSub || code == ObfusXor
        || code == ObfusAnd || code == ObfusShl || code == ObfusCmp;
}

struct DraftOp {
    int code;
    int dest;
    int src0;
    int src1;
    int lineNo;
};

void failLine(std::string* error, int lineNo, const char* text)
{
    if (error) {
        *error = "line " + std::to_string(lineNo) + ": " + text;
    }
}

}

ObfusProgram::ObfusProgram()
    : m_valid(false)
    , m_regCount(0)
    , m_entry(0)
{
}

ObfusProgram::ObfusProgram(const unsigned char* bytes, int size)
    : m_valid(false)
    , m_regCount(0)
    , m_entry(0)
{
    if (!bytes || size < 20) {
        return;
    }
    if (std::memcmp(bytes, "VOB1", 4) != 0) {
        return;
    }
    int off = 4;
    unsigned version = 0;
    unsigned regs = 0;
    unsigned entry = 0;
    unsigned constCount = 0;
    if (!readU16(bytes, size, &off, &version) || version != 1) {
        return;
    }
    if (!readU16(bytes, size, &off, &regs) || regs == 0 || regs > static_cast<unsigned>(kMaxRegs)) {
        return;
    }
    if (!readU32(bytes, size, &off, &entry) || !readU32(bytes, size, &off, &constCount)) {
        return;
    }
    std::vector<std::int64_t> constants;
    constants.reserve(constCount);
    for (unsigned i = 0; i < constCount; ++i) {
        std::int64_t v = 0;
        if (!readI64(bytes, size, &off, &v)) {
            return;
        }
        constants.push_back(v);
    }
    unsigned opCount = 0;
    if (!readU32(bytes, size, &off, &opCount) || opCount == 0 || entry >= opCount) {
        return;
    }
    std::vector<ObfusOp> ops;
    ops.reserve(opCount);
    for (unsigned i = 0; i < opCount; ++i) {
        unsigned code = 0;
        unsigned dest = 0;
        unsigned src0 = 0;
        unsigned src1 = 0;
        if (!readU8(bytes, size, &off, &code) || !readU8(bytes, size, &off, &dest)
            || !readU16(bytes, size, &off, &src0) || !readU16(bytes, size, &off, &src1)) {
            return;
        }
        if (code < ObfusLoadImm || code > ObfusReturn) {
            return;
        }
        ops.push_back(ObfusOp(static_cast<int>(code), static_cast<int>(dest),
                              static_cast<int>(src0), static_cast<int>(src1)));
    }
    if (off != size) {
        return;
    }
    m_regCount = static_cast<int>(regs);
    m_entry = static_cast<int>(entry);
    m_constants.swap(constants);
    m_ops.swap(ops);
    m_valid = true;
}

bool ObfusProgram::valid() const
{
    return m_valid;
}

int ObfusProgram::regCount() const
{
    return m_regCount;
}

int ObfusProgram::entry() const
{
    return m_entry;
}

int ObfusProgram::opCount() const
{
    return static_cast<int>(m_ops.size());
}

int ObfusProgram::constantCount() const
{
    return static_cast<int>(m_constants.size());
}

ObfusOp ObfusProgram::opAt(int index) const
{
    if (index < 0 || index >= opCount()) {
        return ObfusOp();
    }
    return m_ops[static_cast<std::size_t>(index)];
}

std::int64_t ObfusProgram::constant(int index) const
{
    if (index < 0 || index >= constantCount()) {
        return 0;
    }
    return m_constants[static_cast<std::size_t>(index)];
}

bool ObfusProgram::encode(const std::string& text, std::vector<unsigned char>* out, std::string* error)
{
    if (!out) {
        if (error) {
            *error = "output buffer is missing";
        }
        return false;
    }
    out->clear();
    int regCount = -1;
    int entry = 0;
    bool entrySet = false;
    std::vector<std::int64_t> constants;
    std::vector<DraftOp> ops;
    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    while (std::getline(in, raw)) {
        ++lineNo;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> tok = tokens(line);
        if (tok.empty()) {
            continue;
        }
        if (tok[0] == "regs") {
            std::int64_t n = 0;
            if (tok.size() != 2 || !parseI64(tok[1], &n) || n < 1 || n > kMaxRegs || regCount >= 0) {
                failLine(error, lineNo, "regs needs one count from 1 to 256");
                return false;
            }
            regCount = static_cast<int>(n);
            continue;
        }
        if (tok[0] == "entry") {
            std::int64_t n = 0;
            if (tok.size() != 2 || !parseI64(tok[1], &n) || n < 0 || entrySet) {
                failLine(error, lineNo, "entry needs one instruction index");
                return false;
            }
            entry = static_cast<int>(n);
            entrySet = true;
            continue;
        }
        if (tok[0] == "const") {
            std::int64_t n = 0;
            if (tok.size() != 2 || !parseI64(tok[1], &n)) {
                failLine(error, lineNo, "const needs one integer");
                return false;
            }
            constants.push_back(n);
            continue;
        }
        const int code = opcodeOf(tok[0]);
        if (code == 0) {
            failLine(error, lineNo, "unknown opcode");
            return false;
        }
        if (regCount < 0) {
            failLine(error, lineNo, "regs must come before instructions");
            return false;
        }
        DraftOp op;
        op.code = code;
        op.dest = 0;
        op.src0 = 0;
        op.src1 = 0;
        op.lineNo = lineNo;
        if (code == ObfusLoadImm) {
            std::int64_t pool = 0;
            if (tok.size() != 3 || !parseReg(tok[1], regCount, &op.dest, error, lineNo)
                || !parseI64(tok[2], &pool) || pool < 0 || pool >= static_cast<std::int64_t>(constants.size())) {
                if (error && error->empty()) {
                    failLine(error, lineNo, "LoadImm needs a register and a constant index");
                }
                return false;
            }
            op.src0 = static_cast<int>(pool);
        } else if (code == ObfusMove) {
            if (tok.size() != 3 || !parseReg(tok[1], regCount, &op.dest, error, lineNo)
                || !parseReg(tok[2], regCount, &op.src0, error, lineNo)) {
                return false;
            }
        } else if (code == ObfusReturn) {
            if (tok.size() != 2 || !parseReg(tok[1], regCount, &op.dest, error, lineNo)) {
                return false;
            }
        } else if (isBinary(code)) {
            if (tok.size() != 4 || !parseReg(tok[1], regCount, &op.dest, error, lineNo)
                || !parseReg(tok[2], regCount, &op.src0, error, lineNo)
                || !parseReg(tok[3], regCount, &op.src1, error, lineNo)) {
                return false;
            }
        } else if (code == ObfusJump) {
            std::int64_t target = 0;
            if (tok.size() != 2 || !parseI64(tok[1], &target) || target < 0) {
                failLine(error, lineNo, "Jump needs an instruction index");
                return false;
            }
            op.src0 = static_cast<int>(target);
        } else if (code == ObfusJumpIf) {
            std::int64_t target = 0;
            if (tok.size() != 3 || !parseReg(tok[1], regCount, &op.dest, error, lineNo)
                || !parseI64(tok[2], &target) || target < 0) {
                if (error && error->empty()) {
                    failLine(error, lineNo, "JumpIf needs a register and an instruction index");
                }
                return false;
            }
            op.src0 = static_cast<int>(target);
        } else if (code == ObfusCallOut) {
            std::int64_t fn = 0;
            int arg0 = 0;
            int arg1 = 0;
            if (tok.size() != 5 || !parseReg(tok[1], regCount, &op.dest, error, lineNo)
                || !parseI64(tok[2], &fn) || fn < 0 || fn > 65535
                || !parseReg(tok[3], regCount, &arg0, error, lineNo)
                || !parseReg(tok[4], regCount, &arg1, error, lineNo)) {
                if (error && error->empty()) {
                    failLine(error, lineNo, "CallOut needs a destination, a table index, and two registers");
                }
                return false;
            }
            op.src0 = static_cast<int>(fn);
            op.src1 = arg0 | (arg1 << 8);
        }
        ops.push_back(op);
    }

    if (regCount < 0) {
        if (error) {
            *error = "regs is missing";
        }
        return false;
    }
    if (ops.empty()) {
        if (error) {
            *error = "program is empty";
        }
        return false;
    }
    if (entry < 0 || entry >= static_cast<int>(ops.size())) {
        if (error) {
            *error = "entry is outside the instruction list";
        }
        return false;
    }
    for (std::size_t i = 0; i < ops.size(); ++i) {
        const DraftOp& op = ops[i];
        if ((op.code == ObfusJump || op.code == ObfusJumpIf) && op.src0 >= static_cast<int>(ops.size())) {
            failLine(error, op.lineNo, "jump target is outside the instruction list");
            return false;
        }
    }

    std::vector<unsigned char> bytes;
    bytes.reserve(20 + constants.size() * 8 + ops.size() * kOpBytes);
    bytes.push_back('V');
    bytes.push_back('O');
    bytes.push_back('B');
    bytes.push_back('1');
    appendU16(&bytes, 1);
    appendU16(&bytes, static_cast<unsigned>(regCount));
    appendU32(&bytes, static_cast<unsigned>(entry));
    appendU32(&bytes, static_cast<unsigned>(constants.size()));
    for (std::size_t i = 0; i < constants.size(); ++i) {
        appendI64(&bytes, constants[i]);
    }
    appendU32(&bytes, static_cast<unsigned>(ops.size()));
    for (std::size_t i = 0; i < ops.size(); ++i) {
        appendU8(&bytes, static_cast<unsigned>(ops[i].code));
        appendU8(&bytes, static_cast<unsigned>(ops[i].dest));
        appendU16(&bytes, static_cast<unsigned>(ops[i].src0));
        appendU16(&bytes, static_cast<unsigned>(ops[i].src1));
    }
    out->swap(bytes);
    return true;
}

}
