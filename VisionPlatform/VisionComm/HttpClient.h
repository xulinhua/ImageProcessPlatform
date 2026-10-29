/**
 * @file HttpClient.h
 * @brief HTTP/HTTPS 客户端。一次请求一次应答，不走 Modbus 和节点帧
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HTTP_CLIENT_H
#define VISION_HTTP_CLIENT_H

#include "ICommChannel.h"
#include <mutex>
#include <string>

namespace Vision {

/**
 * @class HttpClient
 * @brief 用 WinHTTP 发请求。证书走系统信任库
 */
class HttpClient : public ICommChannel {
public:
    /** @brief 尚未打开 */
    HttpClient();
    /** @brief 关掉会话 */
    ~HttpClient();

    /** @brief 传输种类为 HTTP */
    CommTransport transport() const;
    /** @brief 记下 url、超时和可选 token，并打开 WinHTTP 会话 */
    bool open(const ParamMap& params, std::string* err);
    /** @brief 关掉会话 */
    void close();
    /** @brief 会话是否已打开 */
    bool isOpen() const;
    /** @brief 已打开时返回一条，id 为空，remote 为 url */
    std::vector<CommPeer> peers() const;
    /** @brief HTTP 不按字节写 */
    bool write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err);
    /** @brief HTTP 不按字节读 */
    CommBytes read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err);
    /** @brief 无接收缓冲 */
    void drain(const std::string& peerId);
    /** @brief 没有推送连接 */
    std::string waitIncoming(int timeoutMs);

    /**
     * @brief 发一次请求并读完正文
     * @param path 为空则用 url 里的路径
     */
    bool exchange(const std::string& method, const std::string& path, const std::string& contentType,
                  const std::string& body, int* status, std::string* response, std::string* err);

private:
    mutable std::mutex m_mu; ///< 保护句柄
    void* m_session;          ///< HINTERNET 会话
    std::string m_scheme;     ///< http 或 https
    std::string m_host;       ///< 主机
    int m_port;               ///< 端口
    std::string m_path;       ///< url 路径
    std::string m_url;        ///< 原始 url
    std::string m_token;      ///< 非空时加 Authorization: Bearer
    int m_timeoutMs;          ///< 收发超时
    bool m_secure;            ///< https
};

}

#endif
