/**
 * @file ClockLog.cpp
 * @brief 注册表、ProgramData、本地应用数据各一份
 */

#include "ClockLog.h"
#include <cstring>
#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>

namespace Vision {

namespace {

const unsigned char kMaster[32] = {
    0xb1, 0xea, 0xe6, 0x01, 0x41, 0x40, 0x87, 0x83, 0x68, 0x06, 0xb1, 0x93, 0x7b, 0x64, 0x0b, 0x0b,
    0x19, 0x99, 0xe7, 0xf0, 0x9c, 0x30, 0x42, 0xaf, 0x18, 0x36, 0x21, 0x15, 0xbd, 0xc1, 0x84, 0x88
};

bool sha256(const unsigned char* data, size_t n, unsigned char hash[32])
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
    const NTSTATUS st = BCryptHashData(h, const_cast<PUCHAR>(data), static_cast<ULONG>(n), 0);
    const NTSTATUS done = BCryptFinishHash(h, hash, 32, 0);
    BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0 && done == 0;
}

void keyFor(const char* label, unsigned char key[32])
{
    std::vector<unsigned char> buf(sizeof(kMaster) + strlen(label));
    memcpy(&buf[0], kMaster, sizeof(kMaster));
    memcpy(&buf[sizeof(kMaster)], label, strlen(label));
    sha256(&buf[0], buf.size(), key);
}

bool crypt(bool enc, const unsigned char key[32], const unsigned char nonce[12],
           const unsigned char* in, ULONG n, unsigned char* out, unsigned char tag[16])
{
    BCRYPT_ALG_HANDLE alg = 0;
    BCRYPT_KEY_HANDLE h = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, 0, 0) != 0) {
        return false;
    }
    if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    if (BCryptGenerateSymmetricKey(alg, &h, 0, 0, const_cast<PUCHAR>(key), 32, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = const_cast<PUCHAR>(nonce);
    info.cbNonce = 12;
    info.pbTag = tag;
    info.cbTag = 16;
    ULONG done = 0;
    NTSTATUS st;
    if (enc) {
        st = BCryptEncrypt(h, const_cast<PUCHAR>(in), n, &info, 0, 0, out, n, &done, 0);
    } else {
        st = BCryptDecrypt(h, const_cast<PUCHAR>(in), n, &info, 0, 0, out, n, &done, 0);
    }
    BCryptDestroyKey(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0;
}

bool sealStamp(const char* label, long long stamp, std::vector<unsigned char>* out)
{
    unsigned char key[32];
    keyFor(label, key);
    unsigned char nonce[12];
    if (BCryptGenRandom(0, nonce, 12, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return false;
    }
    unsigned char plain[8];
    for (int i = 0; i < 8; ++i) {
        plain[i] = static_cast<unsigned char>((static_cast<unsigned long long>(stamp) >> (8 * i)) & 0xFF);
    }
    unsigned char cipher[8];
    unsigned char tag[16];
    if (!crypt(true, key, nonce, plain, 8, cipher, tag)) {
        return false;
    }
    out->clear();
    out->insert(out->end(), nonce, nonce + 12);
    out->insert(out->end(), tag, tag + 16);
    out->insert(out->end(), cipher, cipher + 8);
    return true;
}

bool openStamp(const char* label, const unsigned char* data, size_t n, long long* stamp)
{
    if (n != 12 + 16 + 8) {
        return false;
    }
    unsigned char key[32];
    keyFor(label, key);
    unsigned char tag[16];
    memcpy(tag, data + 12, 16);
    unsigned char plain[8];
    if (!crypt(false, key, data, data + 12 + 16, 8, plain, tag)) {
        return false;
    }
    unsigned long long v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<unsigned long long>(plain[i]) << (8 * i);
    }
    *stamp = static_cast<long long>(v);
    return true;
}

std::wstring folder(int csidl)
{
    wchar_t path[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, csidl, 0, 0, path))) {
        return std::wstring();
    }
    return path;
}

bool ensureDir(const std::wstring& path)
{
    if (path.empty()) {
        return false;
    }
    if (CreateDirectoryW(path.c_str(), 0) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    }
    return false;
}

std::wstring tickPath(int csidl)
{
    const std::wstring root = folder(csidl);
    if (root.empty()) {
        return std::wstring();
    }
    const std::wstring parent = root + L"\\VisionPlatform";
    const std::wstring dir = parent + L"\\license";
    if (!ensureDir(parent) || !ensureDir(dir)) {
        return std::wstring();
    }
    return dir + L"\\tick.dat";
}

}

long long ClockLog::now() const
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return static_cast<long long>(u.QuadPart / 10000000ULL - 11644473600ULL);
}

bool ClockLog::latest(long long* stamp, int* found) const
{
    if (stamp) {
        *stamp = 0;
    }
    if (found) {
        *found = 0;
    }
    long long best = 0;
    int n = 0;
    HKEY key = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\VisionPlatform\\License", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        unsigned char buf[64];
        DWORD cb = sizeof(buf);
        if (RegQueryValueExW(key, L"tick", 0, 0, buf, &cb) == ERROR_SUCCESS) {
            long long one = 0;
            if (openStamp("reg", buf, cb, &one)) {
                best = one;
                ++n;
            }
        }
        RegCloseKey(key);
    }
    const int places[2] = {CSIDL_COMMON_APPDATA, CSIDL_LOCAL_APPDATA};
    const char* labels[2] = {"prog", "local"};
    for (int i = 0; i < 2; ++i) {
        const std::wstring path = tickPath(places[i]);
        if (path.empty()) {
            continue;
        }
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (h == INVALID_HANDLE_VALUE) {
            continue;
        }
        unsigned char buf[64];
        DWORD got = 0;
        ReadFile(h, buf, sizeof(buf), &got, 0);
        CloseHandle(h);
        long long one = 0;
        if (openStamp(labels[i], buf, got, &one)) {
            if (n == 0 || one > best) {
                best = one;
            }
            ++n;
        }
    }
    if (stamp) {
        *stamp = best;
    }
    if (found) {
        *found = n;
    }
    return n > 0;
}

bool ClockLog::writeNow() const
{
    return reset();
}

bool ClockLog::reset() const
{
    const long long stamp = now();
    std::vector<unsigned char> blob;
    if (!sealStamp("reg", stamp, &blob)) {
        return false;
    }
    HKEY key = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\VisionPlatform\\License", 0, 0, 0, KEY_SET_VALUE, 0, &key, 0) != ERROR_SUCCESS) {
        return false;
    }
    const LONG wr = RegSetValueExW(key, L"tick", 0, REG_BINARY, &blob[0], static_cast<DWORD>(blob.size()));
    RegCloseKey(key);
    if (wr != ERROR_SUCCESS) {
        return false;
    }
    const int places[2] = {CSIDL_COMMON_APPDATA, CSIDL_LOCAL_APPDATA};
    const char* labels[2] = {"prog", "local"};
    for (int i = 0; i < 2; ++i) {
        std::vector<unsigned char> one;
        if (!sealStamp(labels[i], stamp, &one)) {
            return false;
        }
        const std::wstring path = tickPath(places[i]);
        if (path.empty()) {
            return false;
        }
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (h == INVALID_HANDLE_VALUE) {
            return false;
        }
        DWORD wrote = 0;
        const BOOL ok = WriteFile(h, &one[0], static_cast<DWORD>(one.size()), &wrote, 0);
        CloseHandle(h);
        if (!ok || wrote != one.size()) {
            return false;
        }
    }
    return true;
}

}
