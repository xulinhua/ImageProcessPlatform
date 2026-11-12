/**
 * @file Identity.h
 * @brief 被控端设备号和自签证书
 * @note 存在 ~/.config/RemoteClient。指纹是证书 DER 的 SHA-256，十六进制小写，
 *       与 Windows 端 RemoteIdentity 的算法一致。证书带服务器认证和客户端认证两种用途。
 */

#ifndef REMOTE_CLIENT_IDENTITY_H
#define REMOTE_CLIENT_IDENTITY_H

#include <openssl/ssl.h>

#include <string>

namespace Vision {

class Identity {
public:
    Identity();
    ~Identity();

    /** @brief 读或生成设备号和证书，并做成 TLS 1.2 客户端上下文 */
    bool loadOrCreate(std::string* err);

    /** @brief 16 位十六进制设备号，首次启动生成后不变 */
    const std::string& deviceId() const;
    /** @brief 证书 DER 的 SHA-256，64 个小写十六进制字符 */
    const std::string& fingerprint() const;
    /** @brief 已装入本机证书的客户端上下文 */
    SSL_CTX* context() const;

private:
    bool createCert(const std::string& dir, std::string* err);
    bool loadCert(const std::string& certPath, const std::string& keyPath, std::string* err);
    bool makeContext(std::string* err);

    std::string m_deviceId;
    std::string m_fingerprint;
    SSL_CTX* m_ctx;
    X509* m_cert;
    EVP_PKEY* m_key;
};

}

#endif
