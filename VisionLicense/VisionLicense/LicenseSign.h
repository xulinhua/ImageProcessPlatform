/**
 * @file LicenseSign.h
 * @brief 用编进库的公钥表验注册码正文。私钥不在本库
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
 * @brief ECDSA P-256 验签。公钥在表里，正文最后一行是钥匙编号；旧正文没有编号时用 1
 */
class LicenseSign {
public:
    /**
     * @brief 对正文做 SHA-256 后按正文里的钥匙编号验签
     * @param body 签名正文（LicenseCodec::payload 的结果）
     * @param sig 签名，r||s 共 64 字节
     * @param sigLen 签名长度，不是 64 直接失败
     * @return 签名有效返回 true
     */
    static bool verify(const std::string& body, const unsigned char* sig, int sigLen);
    /** @brief 公钥表里的把数 */
    static int count();
    /**
     * @brief 按表中的顺序取出一把公钥
     * @param index 从 0 起，越界返回 false
     * @param id 钥匙编号
     * @param pub 输出缓冲区
     * @param pubCap 缓冲区长度
     * @param pubLen 实际公钥长度
     */
    static bool at(int index, int* id, unsigned char* pub, int pubCap, int* pubLen);
    /** @brief 这把编号下的公钥和表里的是否相同 */
    static bool matches(int id, const unsigned char* pub, int pubLen);
};

}

#endif
