/**
 * @file CfgStore.cpp
 * @brief 配置 AES-GCM。密钥只在本文件
 */

#include "CfgStore.h"
#include "TextEncoding.h"

#include <cstring>
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

class CfgKey {
public:
    const unsigned char* bytes() const { return kCfgKey; }
    int size() const { return 32; }
};

class AesGcm {
public:
    bool seal(const std::string& plain, std::string* out) const
    {
        if (!out) {
            return false;
        }
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

bool readAll(const std::string& path, std::string* out)
{
    FILE* f = _wfopen(utf8ToWide(path).c_str(), L"rb");
    if (!f) {
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
        out->resize(n);
    }
    fclose(f);
    return true;
}

bool writeAtomic(const std::string& path, const std::string& data)
{
    const std::string tmp = path + ".tmp";
    FILE* f = _wfopen(utf8ToWide(tmp).c_str(), L"wb");
    if (!f) {
        return false;
    }
    const size_t n = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), f);
    const int bad = fflush(f) != 0 || ferror(f);
    fclose(f);
    if (bad || n != data.size()) {
        _wremove(utf8ToWide(tmp).c_str());
        return false;
    }
    const std::wstring wpath = utf8ToWide(path);
    const std::wstring wtmp = utf8ToWide(tmp);
    if (!MoveFileExW(wtmp.c_str(), wpath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        DeleteFileW(wpath.c_str());
        if (!MoveFileExW(wtmp.c_str(), wpath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            _wremove(wtmp.c_str());
            return false;
        }
    }
    return true;
}

}

CfgBytes CfgStore::read(const std::string& path) const
{
    CfgBytes got;
    std::string file;
    if (!readAll(path, &file)) {
        got.kind = CfgFileMissing;
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
