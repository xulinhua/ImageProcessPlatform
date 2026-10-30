/**
 * @file CfgStore.cpp
 * @brief 配置 AES-GCM。密钥只在本文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CfgStore.h"
#include "TextEncoding.h"

#include <cstring>
#include <io.h>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

namespace Vision {

namespace {

const unsigned char kCfgKey[32] = {
    0x91, 0x47, 0x77, 0x62, 0x45, 0xa7, 0xd0, 0x1a, 0xa3, 0xce, 0xca, 0xbc, 0xa0, 0x12, 0x83, 0xb2,
    0x60, 0x30, 0x3a, 0x94, 0x5d, 0xaf, 0x98, 0xfa, 0x82, 0x8d, 0x6c, 0xf9, 0xa1, 0x97, 0x8b, 0x3d
};

/** @brief 内置 256 位配置密钥 */
class CfgKey {
public:
    /** @brief 密钥字节 */
    const unsigned char* bytes() const { return kCfgKey; }
    /** @brief 密钥长度，32 字节 */
    int size() const { return 32; }
};

/** @brief VPC1 密文格式：魔数 4 + 版本 2 + 随机数 12 + 标签 16 + 密文 */
class AesGcm {
public:
    /** @brief 用随机数加密明文并拼成 VPC1 文件内容 */
    bool seal(const std::string& plain, std::string* out) const
    {
        if (!out) {
            return false;
        }
        // 每次写都换随机数，同一内容密文也不同
        unsigned char nonce[12];
        if (BCryptGenRandom(0, nonce, sizeof(nonce), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
            return false;
        }
        std::vector<unsigned char> cipher(plain.size());
        unsigned char tag[16];
        if (!crypt(true, nonce, plain.empty() ? 0 : reinterpret_cast<const unsigned char*>(plain.data()),
                   static_cast<ULONG>(plain.size()), cipher.empty() ? 0 : &cipher[0], tag)) {
            return false;
        }
        out->clear();
        out->append("VPC1", 4);
        const unsigned short ver = 1;
        out->append(reinterpret_cast<const char*>(&ver), 2);
        out->append(reinterpret_cast<const char*>(nonce), 12);
        out->append(reinterpret_cast<const char*>(tag), 16);
        if (!cipher.empty()) {
            out->append(reinterpret_cast<const char*>(&cipher[0]), cipher.size());
        }
        return true;
    }

    /** @brief 校验魔数、版本和标签后解密；任一项不对返回 false */
    bool open(const std::string& file, std::string* json) const
    {
        if (!json || file.size() < 4 + 2 + 12 + 16) {
            return false;
        }
        if (file.compare(0, 4, "VPC1") != 0) {
            return false;
        }
        unsigned short ver = 0;
        memcpy(&ver, file.data() + 4, 2);
        if (ver != 1) {
            return false;
        }
        const unsigned char* nonce = reinterpret_cast<const unsigned char*>(file.data() + 6);
        const unsigned char* tag = nonce + 12;
        const unsigned char* cipher = tag + 16;
        const ULONG n = static_cast<ULONG>(file.size() - (4 + 2 + 12 + 16));
        std::vector<unsigned char> plain(n);
        unsigned char tagCopy[16];
        memcpy(tagCopy, tag, 16);
        if (!crypt(false, nonce, cipher, n, plain.empty() ? 0 : &plain[0], tagCopy)) {
            return false;
        }
        json->assign(plain.empty() ? "" : reinterpret_cast<const char*>(&plain[0]), plain.size());
        return true;
    }

private:
    /** @brief BCrypt AES-GCM 加密或解密；加密时输出标签，解密时校验标签 */
    bool crypt(bool enc, const unsigned char* nonce, const unsigned char* in, ULONG n,
               unsigned char* out, unsigned char* tag) const
    {
        BCRYPT_ALG_HANDLE alg = 0;
        BCRYPT_KEY_HANDLE key = 0;
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, 0, 0) != 0) {
            return false;
        }
        const wchar_t* mode = BCRYPT_CHAIN_MODE_GCM;
        if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(mode)),
                              sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) {
            BCryptCloseAlgorithmProvider(alg, 0);
            return false;
        }
        CfgKey material;
        if (BCryptGenerateSymmetricKey(alg, &key, 0, 0, const_cast<PUCHAR>(material.bytes()),
                                       static_cast<ULONG>(material.size()), 0) != 0) {
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
            st = BCryptEncrypt(key, const_cast<PUCHAR>(in), n, &info, 0, 0, out, n, &done, 0);
        } else {
            st = BCryptDecrypt(key, const_cast<PUCHAR>(in), n, &info, 0, 0, out, n, &done, 0);
        }
        BCryptDestroyKey(key);
        BCryptCloseAlgorithmProvider(alg, 0);
        return st == 0;
    }
};

// 跳过 UTF-8 BOM 和空白后以 { 或 [ 开头即视为旧明文 JSON
bool isPlainJson(const std::string& file)
{
    size_t i = 0;
    if (file.size() >= 3 && static_cast<unsigned char>(file[0]) == 0xEF) {
        i = 3;
    }
    while (i < file.size() && (file[i] == ' ' || file[i] == '\t' || file[i] == '\r' || file[i] == '\n')) {
        ++i;
    }
    return i < file.size() && (file[i] == '{' || file[i] == '[');
}

bool readAll(const std::string& path, std::string* out, bool* missing)
{
    *missing = false;
    const std::wstring wpath = utf8ToWide(path);
    FILE* f = _wfopen(wpath.c_str(), L"rb");
    if (!f) {
        // 只有确实不存在才算缺失；被占用、无权限等要报错，不能当成空配置
        const DWORD attr = GetFileAttributesW(wpath.c_str());
        const DWORD code = GetLastError();
        *missing = attr == INVALID_FILE_ATTRIBUTES && (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND);
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    const long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return false;
    }
    rewind(f);
    out->assign(static_cast<size_t>(sz), '\0');
    if (sz > 0) {
        const size_t n = fread(&(*out)[0], 1, static_cast<size_t>(sz), f);
        if (n != static_cast<size_t>(sz)) {
            fclose(f);
            out->clear();
            return false;
        }
    }
    fclose(f);
    return true;
}

bool writeAtomic(const std::string& path, const std::string& data)
{
    const std::wstring wpath = utf8ToWide(path);
    const std::wstring wtmp = utf8ToWide(path + ".tmp");
    FILE* f = _wfopen(wtmp.c_str(), L"wb");
    if (!f) {
        return false;
    }
    const size_t n = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), f);
    // 临时文件先落盘再替换，断电后目标要么是旧文件、要么是完整的新文件
    const int bad = fflush(f) != 0 || ferror(f) || _commit(_fileno(f)) != 0;
    fclose(f);
    if (bad || n != data.size()) {
        _wremove(wtmp.c_str());
        return false;
    }
    // 同目录改名一步替换，任何时候都不先删原文件；目标被短暂占用时稍等再试，最终失败只删临时文件
    for (int i = 0;; ++i) {
        if (MoveFileExW(wtmp.c_str(), wpath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
        if (i >= 4) {
            break;
        }
        Sleep(50);
    }
    _wremove(wtmp.c_str());
    return false;
}

}

CfgBytes CfgStore::read(const std::string& path) const
{
    CfgBytes got;
    std::string file;
    bool missing = false;
    if (!readAll(path, &file, &missing)) {
        got.kind = missing ? CfgFileMissing : CfgFileUnreadable;
        return got;
    }
    if (isPlainJson(file)) {
        got.kind = CfgFilePlain;
        got.json = file;
        return got;
    }
    AesGcm aes;
    if (!aes.open(file, &got.json)) {
        got.kind = CfgFileCorrupt;
        got.json.clear();
        return got;
    }
    got.kind = CfgFileSealed;
    return got;
}

bool CfgStore::write(const std::string& path, const std::string& json) const
{
    std::string sealed;
    AesGcm aes;
    if (!aes.seal(json, &sealed)) {
        return false;
    }
    return writeAtomic(path, sealed);
}

}
