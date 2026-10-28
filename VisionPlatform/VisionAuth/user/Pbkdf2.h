/**
 * @file Pbkdf2.h
 * @brief Pbkdf2头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PBKDF2_H
#define VISION_PBKDF2_H
#include "VisionAuthExport.h"

#include <stddef.h>
#include <string>
#include <vector>

namespace Vision {

enum {
    HashKindSha256 = 0,      ///< 口令哈希：SHA-256
    HashKindPbkdf2 = 1,      ///< 口令哈希：PBKDF2
    Pbkdf2Iterations = 10000 ///< 默认迭代次数
};

/** @brief HMAC-SHA256，输出 32 字节 */
VISION_AUTH_API void hmacSha256(const unsigned char* key, size_t keyLen,
                const unsigned char* data, size_t dataLen,
                unsigned char out[32]);
/** @brief 用盐和口令派生 32 字节密钥 */
VISION_AUTH_API std::vector<unsigned char> pbkdf2HmacSha256(const std::vector<unsigned char>& salt,
                                            const std::string& password,
                                            int iterations);

}

#endif
