/**
 * @file LicenseSign.h
 * @brief 用编进库的公钥验注册码正文。私钥不在本库
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LICENSE_SIGN_H
#define VISION_LICENSE_SIGN_H

#include <string>

namespace Vision {

/**
 * @class LicenseSign
 * @brief ECDSA P-256 验签，公钥内置
 */
class LicenseSign {
public:
    /**
     * @brief 对正文做 SHA-256 后用内置公钥验签
     * @param body 签名正文（LicenseCodec::payload 的结果）
     * @param sig 签名，r||s 共 64 字节
     * @param sigLen 签名长度，不是 64 直接失败
     * @return 签名有效返回 true
     */
    static bool verify(const std::string& body, const unsigned char* sig, int sigLen);
};

}

#endif
