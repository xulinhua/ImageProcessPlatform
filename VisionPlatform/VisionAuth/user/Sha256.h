/**
 * @file Sha256.h
 * @brief Sha256头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SHA256_H
#define VISION_SHA256_H
#include "VisionAuthExport.h"

#include <stddef.h>
#include <stdint.h>

namespace Vision {

/**
 * @brief SHA-256。口令哈希用，不依赖 Qt / CryptoAPI
 * @param data 输入字节
 * @param len 长度
 * @param out 32 字节摘要
 */
VISION_AUTH_API void sha256(const unsigned char* data, size_t len, unsigned char out[32]);

}

#endif
