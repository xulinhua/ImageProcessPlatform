/**
 * @file RemoteIdentity.h
 * @brief 本机自签证书与指纹。DPAPI 加密存放，供 TLS 使用
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_IDENTITY_H
#define VISION_REMOTE_IDENTITY_H

#include <string>

struct _CERT_CONTEXT;
typedef const struct _CERT_CONTEXT* PCCERT_CONTEXT;

namespace Vision {

/**
 * @class RemoteIdentity
 * @brief 首次运行生成自签证书，之后从加密文件加载；对外暴露指纹与证书上下文
 * @note PFX 私钥与证书库同寿，必须保持 m_store 打开，否则 Schannel 握手会崩
 */
class RemoteIdentity {
public:
    RemoteIdentity();
    ~RemoteIdentity();

    /**
     * @brief 加载或生成身份
     * @param role “control” 或 “controlled”，决定文件名
     * @param displayName 写入证书 CN 的显示名
     */
    bool ensure(const std::string& role, const std::string& displayName, std::string* err);

    /** @brief SHA-256 指纹十六进制（小写，64 字符） */
    const std::string& fingerprint() const;
    /** @brief 证书上下文，供 Schannel 使用；未加载为空 */
    PCCERT_CONTEXT certificate() const;
    /** @brief 是否已加载 */
    bool ready() const;

    /** @brief 对任意 DER/编码证书算 SHA-256 指纹 */
    static std::string fingerprintOfCert(PCCERT_CONTEXT cert);
    /** @brief 比较两个指纹（忽略大小写与空白） */
    static bool fingerprintEqual(const std::string& a, const std::string& b);

private:
    /** @brief 从 DPAPI 保护的文件读证书。失败返回 false */
    bool load(const std::wstring& path, std::string* err);
    /** @brief 生成自签证书并写入 path */
    bool create(const std::wstring& path, const std::string& displayName, std::string* err);
    /** @brief 释放证书与存储 */
    void clear();
    /** @brief 按角色给出证书文件路径 */
    static std::wstring storePath(const std::string& role);

    void* m_store;                  ///< HCERTSTORE，持有 PFX 内存私钥，与 m_cert 同寿
    PCCERT_CONTEXT m_cert;          ///< 本机证书，含私钥
    std::string m_fingerprint;      ///< 证书 SHA-256，十六进制
};

}

#endif
