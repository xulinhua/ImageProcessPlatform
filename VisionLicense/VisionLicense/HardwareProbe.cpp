/**
 * @file HardwareProbe.cpp
 * @brief CPUID、SMBIOS 原始表、系统卷总线。不用 WMI，不打开 PhysicalDrive
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HardwareProbe.h"
#include <cstdio>
#include <cstring>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <winioctl.h>
#include <intrin.h>

namespace Vision {

namespace {

std::string hex32(const unsigned char* p)
{
    static const char* k = "0123456789ABCDEF";
    std::string s;
    for (int i = 0; i < 32; ++i) {
        s.push_back(k[p[i] >> 4]);
        s.push_back(k[p[i] & 15]);
    }
    return s;
}

bool sha256(const std::string& text, unsigned char hash[32])
{
    BCRYPT_ALG_HANDLE alg = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, 0, 0) != 0) {
        return false;
    }
    DWORD objLen = 0;
    DWORD cb = 0;
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cb, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    std::vector<unsigned char> obj(objLen);
    BCRYPT_HASH_HANDLE h = 0;
    if (BCryptCreateHash(alg, &h, &obj[0], objLen, 0, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    const NTSTATUS st = BCryptHashData(h, reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())),
                                       static_cast<ULONG>(text.size()), 0);
    const NTSTATUS done = BCryptFinishHash(h, hash, 32, 0);
    BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0 && done == 0;
}

}

std::string HardwareProbe::normalize(const std::string& text) const
{
    std::string out;
    bool space = false;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            space = !out.empty();
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        if (c >= 'a' && c <= 'z') {
            c = static_cast<unsigned char>(c - 'a' + 'A');
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

bool HardwareProbe::usable(const std::string& text) const
{
    const std::string s = normalize(text);
    if (s.empty() || s == "NONE" || s == "DEFAULT STRING" || s == "TO BE FILLED BY O.E.M."
        || s == "SYSTEM SERIAL NUMBER" || s == "0") {
        return false;
    }
    bool all0 = true;
    bool allF = true;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '-') {
            continue;
        }
        if (s[i] != '0') {
            all0 = false;
        }
        if (s[i] != 'F') {
            allF = false;
        }
    }
    return !all0 && !allF;
}

std::string HardwareProbe::cpuPart() const
{
    int info[4] = {0, 0, 0, 0};
    __cpuid(info, 0);
    char vendor[13];
    memcpy(vendor + 0, &info[1], 4);
    memcpy(vendor + 4, &info[3], 4);
    memcpy(vendor + 8, &info[2], 4);
    vendor[12] = 0;
    __cpuid(info, 1);
    const unsigned int eax = static_cast<unsigned int>(info[0]);
    unsigned int family = (eax >> 8) & 0xF;
    unsigned int model = (eax >> 4) & 0xF;
    const unsigned int stepping = eax & 0xF;
    if (family == 6 || family == 15) {
        model += ((eax >> 16) & 0xF) << 4;
    }
    if (family == 15) {
        family += (eax >> 20) & 0xFF;
    }
    char brand[49];
    memset(brand, 0, sizeof(brand));
    int ext[4] = {0, 0, 0, 0};
    __cpuid(ext, 0x80000000);
    if (static_cast<unsigned int>(ext[0]) >= 0x80000004) {
        int b[12];
        __cpuid(b + 0, 0x80000002);
        __cpuid(b + 4, 0x80000003);
        __cpuid(b + 8, 0x80000004);
        memcpy(brand, b, 48);
    }
    const std::string brandText = normalize(brand);
    const std::string vendorText = normalize(vendor);
    if (brandText.empty() && family == 0) {
        return std::string();
    }
    char nums[64];
    sprintf_s(nums, "%u|%u|%u", family, model, stepping);
    return std::string("CPU\n") + vendorText + "|" + nums + "|" + brandText;
}

std::string HardwareProbe::biosPart(bool* haveUuid) const
{
    if (haveUuid) {
        *haveUuid = false;
    }
    // RawSMBIOSData：8 字节头，偏移 4 处是表长
    const DWORD bytes = GetSystemFirmwareTable('RSMB', 0, 0, 0);
    if (bytes < 8) {
        return std::string();
    }
    std::vector<unsigned char> buf(bytes);
    if (GetSystemFirmwareTable('RSMB', 0, &buf[0], bytes) != bytes) {
        return std::string();
    }
    const DWORD length = *reinterpret_cast<DWORD*>(&buf[4]);
    if (8 + length > bytes) {
        return std::string();
    }
    const unsigned char* p = &buf[8];
    const unsigned char* end = p + length;
    std::string uuid;
    std::string serial;
    std::string boardVendor;
    std::string boardProduct;
    std::string boardSerial;
    std::string biosVendor;
    std::string biosVer;
    // 逐个结构：格式化区后接以双 0 结尾的字符串区，字段里存的是字符串序号
    while (p + 4 <= end) {
        const unsigned char type = p[0];
        const unsigned char len = p[1];
        if (len < 4 || p + len > end) {
            break;
        }
        const unsigned char* str = p + len;
        const unsigned char* next = str;
        while (next + 1 < end && !(next[0] == 0 && next[1] == 0)) {
            ++next;
        }
        if (next + 1 >= end) {
            break;
        }
        next += 2;
        auto nth = [&](unsigned char index) -> std::string {
            if (index == 0) {
                return std::string();
            }
            const unsigned char* s = str;
            unsigned char n = 1;
            while (s < next - 2) {
                if (n == index) {
                    return std::string(reinterpret_cast<const char*>(s));
                }
                while (s < next - 2 && *s != 0) {
                    ++s;
                }
                if (s < next - 2) {
                    ++s;
                }
                ++n;
            }
            return std::string();
        };
        // 类型 1 系统信息、类型 2 主板、类型 0 BIOS；UUID 前三段按小端显示
        if (type == 1 && len >= 0x19) {
            unsigned char u[16];
            memcpy(u, p + 8, 16);
            char text[40];
            sprintf_s(text, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                      u[3], u[2], u[1], u[0], u[5], u[4], u[7], u[6],
                      u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
            if (usable(text)) {
                uuid = normalize(text);
                if (haveUuid) {
                    *haveUuid = true;
                }
            }
            if (usable(nth(p[7]))) {
                serial = normalize(nth(p[7]));
            }
        } else if (type == 2 && len >= 0x08) {
            if (usable(nth(p[4]))) {
                boardVendor = normalize(nth(p[4]));
            }
            if (usable(nth(p[5]))) {
                boardProduct = normalize(nth(p[5]));
            }
            if (usable(nth(p[7]))) {
                boardSerial = normalize(nth(p[7]));
            }
        } else if (type == 0 && len >= 0x06) {
            if (usable(nth(p[4]))) {
                biosVendor = normalize(nth(p[4]));
            }
            if (usable(nth(p[5]))) {
                biosVer = normalize(nth(p[5]));
            }
        }
        p = next;
    }
    if (uuid.empty() && boardSerial.empty()) {
        return std::string();
    }
    return std::string("BIOS\n") + uuid + "|" + serial + "|" + boardVendor + "|" + boardProduct
        + "|" + boardSerial + "|" + biosVendor + "|" + biosVer;
}

std::string HardwareProbe::diskPart() const
{
    wchar_t win[MAX_PATH];
    if (GetWindowsDirectoryW(win, MAX_PATH) == 0 || win[1] != L':') {
        return std::string();
    }
    wchar_t vol[16];
    swprintf_s(vol, L"\\\\.\\%c:", win[0]);
    HANDLE h = CreateFileW(vol, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE) {
        return std::string();
    }
    STORAGE_PROPERTY_QUERY query;
    memset(&query, 0, sizeof(query));
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    unsigned char raw[1024];
    DWORD got = 0;
    const BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), raw, sizeof(raw), &got, 0);
    CloseHandle(h);
    if (!ok || got < sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
        return std::string();
    }
    const STORAGE_DEVICE_DESCRIPTOR* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(raw);
    const int bus = static_cast<int>(desc->BusType);
    const bool internal = bus == BusTypeSata || bus == BusTypeAta || bus == BusTypeNvme
        || bus == BusTypeScsi || bus == BusTypeSas || bus == BusTypeRAID;
    if (!internal) {
        return std::string();
    }
    if (desc->SerialNumberOffset == 0 || desc->SerialNumberOffset >= got) {
        return std::string();
    }
    const std::string serial = normalize(reinterpret_cast<const char*>(raw + desc->SerialNumberOffset));
    if (!usable(serial)) {
        return std::string();
    }
    return std::string("DISK\n") + serial;
}

std::string HardwareProbe::digest() const
{
    const std::string cpu = cpuPart();
    bool haveUuid = false;
    const std::string bios = biosPart(&haveUuid);
    const std::string brand = cpu.empty() ? std::string() : cpu.substr(cpu.rfind('|') + 1);
    if (brand.empty() && !haveUuid) {
        return std::string();
    }
    std::string text;
    if (!cpu.empty()) {
        text += cpu;
    }
    if (!bios.empty()) {
        if (!text.empty()) {
            text.push_back('\n');
        }
        text += bios;
    }
    const std::string disk = diskPart();
    if (!disk.empty()) {
        if (!text.empty()) {
            text.push_back('\n');
        }
        text += disk;
    }
    unsigned char hash[32];
    if (!sha256(text, hash)) {
        return std::string();
    }
    return hex32(hash);
}

}
