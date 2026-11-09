/**
 * @file RemoteAuth.h
 * @brief 一次性验证码、无人值守密码（PBKDF2）、防爆破计时
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_AUTH_H
#define VISION_REMOTE_AUTH_H

#include <stdint.h>
#include <string>

namespace Vision {

/**
 * @class RemoteAuth
 * @brief 被控端本地认证材料与校验。不涉及网络
 */
class RemoteAuth {
public:
    static const int kCodeLen = 6;
    static const int kCodeTtlMs = 10 * 60 * 1000;    ///< 验证码有效期 10 分钟
    static const int kPbkdf2Iter = 100000;
    static const int kSaltBytes = 16;
    static const int kHashBytes = 32;

    RemoteAuth();

    /** @brief 生成或刷新 6 位数字验证码，返回明文 */
    std::string refreshCode();
    /** @brief 当前验证码；过期则自动刷新 */
    std::string currentCode();
    /** @brief 验证码是否匹配（忽略空白）；成功后刷新 */
    bool checkCode(const std::string& token);

    /**
     * @brief 设置无人值守密码。空串清除
     * @return saltHex + ":" + hashHex；空密码返回空串
     */
    std::string setPassword(const std::string& password);
    /** @brief 从 “salt:hash” 装入已存哈希 */
    bool loadPasswordHash(const std::string& stored);
    /** @brief 是否已设密码 */
    bool hasPassword() const;
    /** @brief 校验密码明文 */
    bool checkPassword(const std::string& password) const;

    /** @brief 记录一次失败；返回是否已锁定，以及剩余锁定秒 */
    bool onFail(uint32_t* lockSecLeft);
    /** @brief 认证成功时清零失败计数 */
    void onSuccess();
    /** @brief 当前是否锁定 */
    bool isLocked(uint32_t* lockSecLeft) const;
    /** @brief 距离锁定还剩几次失败机会 */
    uint8_t triesLeft() const;

    /** @brief 对任意材料做 SHA-256 十六进制；证书指纹优先用 RemoteIdentity::fingerprint */
    static std::string fingerprintOf(const std::string& material);

private:
    /** @brief 字节转小写十六进制 */
    static std::string toHex(const unsigned char* data, size_t n);
    /** @brief 十六进制转字节。长度不对返回 false */
    static bool fromHex(const std::string& hex, unsigned char* out, size_t n);
    static bool pbkdf2(const std::string& password, const unsigned char* salt, size_t saltLen,
                       unsigned char* out, size_t outLen);
    /** @brief 开机以来的毫秒 */
    static uint64_t nowMs();

    std::string m_code;             ///< 当前一次性验证码
    uint64_t m_codeAt;              ///< 验证码生成时刻
    unsigned char m_salt[16];       ///< 密码盐
    unsigned char m_hash[32];       ///< PBKDF2 结果
    bool m_hasPassword;             ///< 是否设过无人值守密码
    int m_failCount;                ///< 连续失败次数
    uint64_t m_lockUntil;           ///< 锁定截止时刻，0 表示未锁定
    int m_lockSec;                  ///< 当前这一轮锁定的秒数
};

}

#endif
