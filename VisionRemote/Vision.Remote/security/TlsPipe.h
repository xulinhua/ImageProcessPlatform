/**
 * @file TlsPipe.h
 * @brief 在已连接的 SOCKET 上做 Schannel TLS 握手与加解密
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_TLS_PIPE_H
#define VISION_REMOTE_TLS_PIPE_H

#include "CommPeer.h"
#include "RemoteIdentity.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class TlsPipe
 * @brief 接管 SOCKET：handshake 后 read/write 走 TLS。不拥有套接字寿命（close 不关 sock）
 */
class TlsPipe {
public:
    TlsPipe();
    ~TlsPipe();

    /**
     * @brief TLS 握手
     * @param sock 已连上的阻塞套接字
     * @param asServer true 为服务端（控制端监听侧）
     * @param identity 本机证书
     * @param peerFingerprintOut 对端证书指纹（可空）
     */
    /**
     * @param serverName 客户端 SNI。空则不带，局域网自签沿用空
     */
    bool handshake(uintptr_t sock, bool asServer, const RemoteIdentity& identity,
                   std::string* peerFingerprintOut, std::string* err, const wchar_t* serverName = nullptr);

    /** @brief 加密后发出。timeoutMs<=0 按 3000 */
    bool write(const CommBytes& data, int timeoutMs, std::string* err);
    /** @brief 只加密，不碰套接字。密文追加到 out */
    bool seal(const unsigned char* data, size_t len, std::vector<unsigned char>* out, std::string* err);
    /** @brief 解密一次。超时返回空且保持连接 */
    CommBytes read(int maxBytes, int timeoutMs, std::string* err);
    /** @brief 把已经读到的密文解成明文。半截记录留着，下次再喂 */
    CommBytes unseal(const unsigned char* data, size_t len, std::string* err);

    /** @brief 握手已完成 */
    bool ready() const;
    /** @brief 丢掉握手状态，套接字仍由调用方持有 */
    void reset();
    /** @brief 对端证书指纹。未握手为空 */
    const std::string& peerFingerprint() const;

private:
    struct Impl;
    Impl* m_impl;               ///< Schannel 句柄，构造时创建
};

}

#endif
