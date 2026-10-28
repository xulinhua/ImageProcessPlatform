/**
 * @file Pbkdf2.cpp
 * @brief Pbkdf2实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Pbkdf2.h"
#include "Sha256.h"
#include <string.h>

namespace Vision {

// HMAC-SHA256，输出 32 字节
void hmacSha256(const unsigned char* key, size_t keyLen,
                const unsigned char* data, size_t dataLen,
                unsigned char out[32])
{
    unsigned char k[64];
    memset(k, 0, sizeof(k));
    if (keyLen > 64) {
        sha256(key, keyLen, k);
    } else if (key && keyLen) {
        memcpy(k, key, keyLen);
    }
    unsigned char ipad[64];
    unsigned char opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = static_cast<unsigned char>(k[i] ^ 0x36);
        opad[i] = static_cast<unsigned char>(k[i] ^ 0x5c);
    }
    std::vector<unsigned char> inner;
    inner.insert(inner.end(), ipad, ipad + 64);
    if (data && dataLen) {
        inner.insert(inner.end(), data, data + dataLen);
    }
    unsigned char innerHash[32];
    sha256(inner.empty() ? 0 : &inner[0], inner.size(), innerHash);
    unsigned char outer[64 + 32];
    memcpy(outer, opad, 64);
    memcpy(outer + 64, innerHash, 32);
    sha256(outer, sizeof(outer), out);
}

// 用盐和口令派生 32 字节密钥
std::vector<unsigned char> pbkdf2HmacSha256(const std::vector<unsigned char>& salt,
                                            const std::string& password,
                                            int iterations)
{
    if (iterations < 1) {
        iterations = Pbkdf2Iterations;
    }
    unsigned char block[4] = { 0, 0, 0, 1 };
    std::vector<unsigned char> msg = salt;
    msg.insert(msg.end(), block, block + 4);
    unsigned char u[32];
    hmacSha256(password.empty() ? 0 : reinterpret_cast<const unsigned char*>(password.data()),
               password.size(),
               msg.empty() ? 0 : &msg[0], msg.size(), u);
    unsigned char dk[32];
    memcpy(dk, u, 32);
    for (int i = 1; i < iterations; ++i) {
        hmacSha256(password.empty() ? 0 : reinterpret_cast<const unsigned char*>(password.data()),
                   password.size(), u, 32, u);
        for (int b = 0; b < 32; ++b) {
            dk[b] ^= u[b];
        }
    }
    return std::vector<unsigned char>(dk, dk + 32);
}

}
