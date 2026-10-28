/**
 * @file LicenseSign.cpp
 * @brief ECDSA P-256 验签
 */

#include "LicenseSign.h"
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

namespace Vision {

namespace {

const unsigned char kPub[] = {
    0x45, 0x43, 0x53, 0x31, 0x20, 0x00, 0x00, 0x00, 0xdb, 0x05, 0x0b, 0x2e, 0x8c, 0x75, 0x28, 0x56,
    0x5d, 0x50, 0xfd, 0xbb, 0xad, 0x95, 0x74, 0x1a, 0xc8, 0x05, 0x91, 0x2e, 0xac, 0xb7, 0x5c, 0x99,
    0x33, 0xd5, 0x1c, 0x02, 0xce, 0xb2, 0x32, 0xc1, 0x48, 0x09, 0x1c, 0x7d, 0x8a, 0x0d, 0x58, 0xe3,
    0x39, 0xa8, 0x41, 0xcb, 0xa4, 0x1f, 0xf9, 0x1d, 0xf5, 0x42, 0x49, 0x2b, 0x88, 0x29, 0xe3, 0x3b,
    0xa7, 0x57, 0x10, 0xb5, 0x06, 0xe2, 0x2b, 0x2c
};

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
    if (!sha256(body, hash)) {
        return false;
    }
    BCRYPT_ALG_HANDLE alg = 0;
    BCRYPT_KEY_HANDLE key = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, 0, 0) != 0) {
        return false;
    }
    if (BCryptImportKeyPair(alg, 0, BCRYPT_ECCPUBLIC_BLOB, &key, const_cast<PUCHAR>(kPub), sizeof(kPub), 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    const NTSTATUS st = BCryptVerifySignature(key, 0, hash, 32, const_cast<PUCHAR>(sig), 64, 0);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0;
}

}
