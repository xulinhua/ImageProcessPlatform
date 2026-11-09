/**
 * @file LicenseSign.cpp
 * @brief ECDSA P-256 验签
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LicenseSign.h"
#include <cstring>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

namespace Vision {

namespace {

struct Pub {
    int id;
    const unsigned char* data;
    int size;
};

#include "LicenseKeyTable.inc"

/** @brief 正文第 7 行是钥匙编号。旧正文没有这一行时返回 1，写了但不是正整数时返回 0 */
int keyIdOf(const std::string& body)
{
    int index = 0;
    std::string cur;
    bool found = false;
    for (size_t i = 0; i <= body.size(); ++i) {
        if (i == body.size() || body[i] == '\n') {
            if (index == 6) {
                found = true;
                break;
            }
            ++index;
            cur.clear();
        } else {
            cur.push_back(body[i]);
        }
    }
    if (!found || cur.empty()) {
        return 1;
    }
    int value = 0;
    for (size_t i = 0; i < cur.size(); ++i) {
        if (cur[i] < '0' || cur[i] > '9') {
            return 0;
        }
        const int digit = cur[i] - '0';
        if (value > (2147483647 - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
    }
    return value >= 1 ? value : 0;
}

const Pub* findPub(int id)
{
    const int n = (int)(sizeof(kPubs) / sizeof(kPubs[0]));
    for (int i = 0; i < n; ++i) {
        if (kPubs[i].id == id) {
            return &kPubs[i];
        }
    }
    return 0;
}

bool sha256(const std::string& body, unsigned char hash[32])
{
    BCRYPT_ALG_HANDLE alg = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, 0, 0) != 0) {
        return false;
    }
    BCRYPT_HASH_HANDLE h = 0;
    DWORD objLen = 0;
    DWORD cb = 0;
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &cb, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    std::vector<unsigned char> obj(objLen);
    if (BCryptCreateHash(alg, &h, &obj[0], objLen, 0, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    const NTSTATUS st = BCryptHashData(h, reinterpret_cast<PUCHAR>(const_cast<char*>(body.data())),
                                       static_cast<ULONG>(body.size()), 0);
    const NTSTATUS done = BCryptFinishHash(h, hash, 32, 0);
    BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0 && done == 0;
}

}

bool LicenseSign::verify(const std::string& body, const unsigned char* sig, int sigLen)
{
    if (!sig || sigLen != 64) {
        return false;
    }
    unsigned char hash[32];
    const int keyId = keyIdOf(body);
    const Pub* pub = keyId > 0 ? findPub(keyId) : 0;
    if (!pub || !sha256(body, hash)) {
        return false;
    }
    BCRYPT_ALG_HANDLE alg = 0;
    BCRYPT_KEY_HANDLE key = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, 0, 0) != 0) {
        return false;
    }
    if (BCryptImportKeyPair(alg, 0, BCRYPT_ECCPUBLIC_BLOB, &key,
                            const_cast<PUCHAR>(pub->data), static_cast<ULONG>(pub->size), 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    const NTSTATUS st = BCryptVerifySignature(key, 0, hash, 32, const_cast<PUCHAR>(sig), 64, 0);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0;
}

int LicenseSign::count()
{
    return (int)(sizeof(kPubs) / sizeof(kPubs[0]));
}

bool LicenseSign::at(int index, int* id, unsigned char* pub, int pubCap, int* pubLen)
{
    const int n = count();
    if (!id || !pub || !pubLen || index < 0 || index >= n) {
        return false;
    }
    if (pubCap < kPubs[index].size) {
        return false;
    }
    *id = kPubs[index].id;
    *pubLen = kPubs[index].size;
    memcpy(pub, kPubs[index].data, static_cast<size_t>(kPubs[index].size));
    return true;
}

bool LicenseSign::matches(int id, const unsigned char* pub, int pubLen)
{
    const Pub* found = findPub(id);
    if (!found || !pub || pubLen != found->size) {
        return false;
    }
    return memcmp(pub, found->data, static_cast<size_t>(pubLen)) == 0;
}

}
