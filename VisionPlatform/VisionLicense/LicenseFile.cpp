/**
 * @file LicenseFile.cpp
 * @brief 魔数 VPL1。密文随随机数变化
 */

#include "LicenseFile.h"
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

void fileKey(const std::string& digest, unsigned char key[32])
{
    std::vector<unsigned char> buf(sizeof(kMaster) + digest.size());
    memcpy(&buf[0], kMaster, sizeof(kMaster));
    if (!digest.empty()) {
        memcpy(&buf[sizeof(kMaster)], digest.data(), digest.size());
    }
    sha256(&buf[0], buf.size(), key);
}

bool aes(bool enc, const unsigned char key[32], const unsigned char nonce[12],
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
    return st == 0 && done == n;
}

bool ensure(const std::wstring& path)
{
    if (CreateDirectoryW(path.c_str(), 0) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    }
    return false;
}

}

std::string LicenseFile::path() const
{
    wchar_t root[MAX_PATH];
    if (FAILED(SHGetFolderPathW(0, CSIDL_COMMON_APPDATA, 0, 0, root))) {
        return std::string();
    }
    const std::wstring parent = std::wstring(root) + L"\\VisionPlatform";
    const std::wstring dir = parent + L"\\license";
    if (!ensure(parent) || !ensure(dir)) {
        return std::string();
    }
    const std::wstring full = dir + L"\\license.dat";
    std::string utf8;
    const int n = WideCharToMultiByte(CP_UTF8, 0, full.c_str(), -1, 0, 0, 0, 0);
    if (n <= 1) {
        return std::string();
    }
    utf8.assign(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, full.c_str(), -1, &utf8[0], n, 0, 0);
    return utf8;
}

bool LicenseFile::exists() const
{
    const std::string p = path();
    if (p.empty()) {
        return false;
    }
    wchar_t wide[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, wide, MAX_PATH * 2) <= 0) {
        return false;
    }
    const DWORD a = GetFileAttributesW(wide);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool LicenseFile::write(const std::string& digest, const std::string& body, const unsigned char* sig) const
{
    const std::string p = path();
    if (p.empty() || !sig || body.size() > 0xFFFF) {
        return false;
    }
    std::string packed;
    const unsigned short len = static_cast<unsigned short>(body.size());
    packed.append(reinterpret_cast<const char*>(&len), 2);
    packed.append(body);
    packed.append(reinterpret_cast<const char*>(sig), 64);
    unsigned char key[32];
    fileKey(digest, key);
    unsigned char nonce[12];
    if (BCryptGenRandom(0, nonce, 12, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return false;
    }
    std::vector<unsigned char> cipher(packed.size());
    unsigned char tag[16];
    if (!aes(true, key, nonce, reinterpret_cast<const unsigned char*>(packed.data()),
             static_cast<ULONG>(packed.size()), &cipher[0], tag)) {
        return false;
    }
    std::string file("VPL1", 4);
    const unsigned short ver = 1;
    file.append(reinterpret_cast<const char*>(&ver), 2);
    file.append(reinterpret_cast<const char*>(nonce), 12);
    file.append(reinterpret_cast<const char*>(tag), 16);
    if (!cipher.empty()) {
        file.append(reinterpret_cast<const char*>(&cipher[0]), cipher.size());
    }
    wchar_t wide[MAX_PATH * 2];
    if (MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, wide, MAX_PATH * 2) <= 0) {
        return false;
    }
    HANDLE h = CreateFileW(wide, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD wrote = 0;
    const BOOL ok = WriteFile(h, file.data(), static_cast<DWORD>(file.size()), &wrote, 0);
    CloseHandle(h);
    return ok && wrote == file.size();
}

bool LicenseFile::read(const std::string& digest, std::string* body, unsigned char sig[64], bool* damaged) const
{
    if (damaged) {
        *damaged = false;
    }
    if (!body || !sig) {
        return false;
    }
    body->clear();
    wchar_t wide[MAX_PATH * 2];
    const std::string p = path();
    if (p.empty() || MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, wide, MAX_PATH * 2) <= 0) {
        return false;
    }
    HANDLE h = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    std::string file;
    char buf[1024];
    DWORD n = 0;
    while (ReadFile(h, buf, sizeof(buf), &n, 0) && n > 0) {
        file.append(buf, buf + n);
    }
    CloseHandle(h);
    if (file.size() < 4 + 2 + 12 + 16 || file.compare(0, 4, "VPL1") != 0) {
        if (damaged) {
            *damaged = true;
        }
        return false;
    }
    unsigned short ver = 0;
    memcpy(&ver, file.data() + 4, 2);
    if (ver != 1) {
        if (damaged) {
            *damaged = true;
        }
        return false;
    }
    unsigned char key[32];
    fileKey(digest, key);
    const unsigned char* nonce = reinterpret_cast<const unsigned char*>(file.data() + 6);
    unsigned char tag[16];
    memcpy(tag, file.data() + 18, 16);
    const unsigned char* cipher = reinterpret_cast<const unsigned char*>(file.data() + 34);
    const ULONG cn = static_cast<ULONG>(file.size() - 34);
    std::vector<unsigned char> plain(cn);
    if (!aes(false, key, nonce, cipher, cn, plain.empty() ? 0 : &plain[0], tag)) {
        return false;
    }
    if (plain.size() < 2 + 64) {
        if (damaged) {
            *damaged = true;
        }
        return false;
    }
    unsigned short len = 0;
    memcpy(&len, &plain[0], 2);
    if (static_cast<size_t>(2 + len + 64) != plain.size()) {
        if (damaged) {
            *damaged = true;
        }
        return false;
    }
    body->assign(reinterpret_cast<const char*>(&plain[2]), len);
    memcpy(sig, &plain[2 + len], 64);
    return true;
}

}
