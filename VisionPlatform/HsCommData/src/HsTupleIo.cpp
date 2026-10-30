/**
 * @file HsTupleIo.cpp
 * @brief HsTupleIo实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsTuple.h>
#include <HsCommData/HsException.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

namespace HsCommData {

namespace {

void failTupleFile(const char* proc, HsError code)
{
    raiseException(HsException(proc, code));
}

void putBytes(FILE* f, const void* p, size_t n, const char* proc)
{
    if (n == 0) {
        return;
    }
    if (fwrite(p, 1, n, f) != n) {
        failTupleFile(proc, HS_ERR_FILE_IO);
    }
}

void getBytes(FILE* f, void* p, size_t n, const char* proc)
{
    if (n == 0) {
        return;
    }
    if (fread(p, 1, n, f) != n) {
        failTupleFile(proc, HS_ERR_FILE_IO);
    }
}

void putI32(FILE* f, int32_t v, const char* proc)
{
    putBytes(f, &v, sizeof(v), proc);
}

void putI64(FILE* f, int64_t v, const char* proc)
{
    putBytes(f, &v, sizeof(v), proc);
}

void putF64(FILE* f, double v, const char* proc)
{
    putBytes(f, &v, sizeof(v), proc);
}

int32_t getI32(FILE* f, const char* proc)
{
    int32_t v = 0;
    getBytes(f, &v, sizeof(v), proc);
    return v;
}

int64_t getI64(FILE* f, const char* proc)
{
    int64_t v = 0;
    getBytes(f, &v, sizeof(v), proc);
    return v;
}

double getF64(FILE* f, const char* proc)
{
    double v = 0.0;
    getBytes(f, &v, sizeof(v), proc);
    return v;
}

void putStr(FILE* f, const std::string& text, const char* proc)
{
    if (text.size() > static_cast<size_t>(0x7fffffff)) {
        failTupleFile(proc, HS_ERR_FILE_IO);
    }
    putI32(f, static_cast<int32_t>(text.size()), proc);
    if (!text.empty()) {
        putBytes(f, text.data(), text.size(), proc);
    }
}

std::string getStr(FILE* f, const char* proc)
{
    const int32_t n = getI32(f, proc);
    if (n < 0) {
        failTupleFile(proc, HS_ERR_FILE_FORMAT);
    }
    std::string text;
    text.resize(static_cast<size_t>(n));
    if (n > 0) {
        getBytes(f, &text[0], static_cast<size_t>(n), proc);
    }
    return text;
}

}

// 写入本地元组文件
void HsTuple::writeTuple(const char* fileName) const
{
    const char* proc = "writeTuple";
    if (!fileName || !fileName[0]) {
        failTupleFile(proc, HS_ERR_FILE_OPEN);
    }
    FILE* f = fopen(fileName, "wb");
    if (!f) {
        failTupleFile(proc, HS_ERR_FILE_OPEN);
    }
    try {
        putBytes(f, "HSTP", 4, proc);
        putI32(f, 1, proc);
        const int n = length();
        putI32(f, n, proc);
        for (int i = 0; i < n; ++i) {
            int64_t li = 0;
            double d = 0.0;
            std::string text;
            if (getLong(i, &li)) {
                putI32(f, static_cast<int32_t>(ElemLong), proc);
                putI64(f, li, proc);
            } else if (getDouble(i, &d)) {
                putI32(f, static_cast<int32_t>(ElemDouble), proc);
                putF64(f, d, proc);
            } else if (getString(i, &text)) {
                putI32(f, static_cast<int32_t>(ElemString), proc);
                putStr(f, text, proc);
            } else {
                failTupleFile(proc, HS_ERR_TYPE);
            }
        }
    } catch (...) {
        fclose(f);
        throw;
    }
    if (fclose(f) != 0) {
        failTupleFile(proc, HS_ERR_FILE_IO);
    }
}

// 从本地文件读入元组
void HsTuple::readTuple(const char* fileName)
{
    const char* proc = "readTuple";
    if (!fileName || !fileName[0]) {
        failTupleFile(proc, HS_ERR_FILE_OPEN);
    }
    FILE* f = fopen(fileName, "rb");
    if (!f) {
        failTupleFile(proc, HS_ERR_FILE_OPEN);
    }
    try {
        char magic[4];
        getBytes(f, magic, 4, proc);
        if (std::memcmp(magic, "HSTP", 4) != 0) {
            failTupleFile(proc, HS_ERR_FILE_FORMAT);
        }
        const int32_t ver = getI32(f, proc);
        if (ver != 1) {
            failTupleFile(proc, HS_ERR_FILE_FORMAT);
        }
        const int32_t n = getI32(f, proc);
        if (n < 0) {
            failTupleFile(proc, HS_ERR_FILE_FORMAT);
        }
        HsTuple out;
        for (int32_t i = 0; i < n; ++i) {
            const int32_t kind = getI32(f, proc);
            if (kind == ElemLong) {
                out.append(getI64(f, proc));
            } else if (kind == ElemDouble) {
                out.append(getF64(f, proc));
            } else if (kind == ElemString) {
                out.append(getStr(f, proc));
            } else {
                failTupleFile(proc, HS_ERR_FILE_FORMAT);
            }
        }
        *this = out;
    } catch (...) {
        fclose(f);
        throw;
    }
    if (fclose(f) != 0) {
        failTupleFile(proc, HS_ERR_FILE_IO);
    }
}

// 自由函数转调成员
void writeTuple(const HsTuple& tuple, const char* fileName)
{
    tuple.writeTuple(fileName);
}

// 自由函数转调成员。空指针抛类型错
void readTuple(HsTuple* tuple, const char* fileName)
{
    if (!tuple) {
        failTupleFile("readTuple", HS_ERR_TYPE);
    }
    tuple->readTuple(fileName);
}

}
