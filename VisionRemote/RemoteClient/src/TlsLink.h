/**
 * @file TlsLink.h
 * @brief 与中控台的 TLS 1.2 连接
 * @note 中控台的 RemoteTcpServer 入站握手是服务端。直连时套接字由本机 accept，
 *       反连时由本机 connect，两边都要 SSL_connect，不能在本机做 SSL_accept。
 */

#ifndef REMOTE_CLIENT_TLS_LINK_H
#define REMOTE_CLIENT_TLS_LINK_H

#include <openssl/ssl.h>

#include <string>
#include <vector>

namespace Vision {

class TlsLink {
public:
    TlsLink();
    ~TlsLink();

    /** @brief 在已连接的套接字上完成客户端握手，成功后套接字改为非阻塞 */
    bool handshake(int fd, SSL_CTX* ctx, std::string* err);
    /** @brief 读明文。暂时没有数据时 out 为空且返回 true；对端关闭返回 false */
    bool read(std::vector<unsigned char>* out, std::string* err);
    /** @brief 把一整段明文写完。写一半会等到套接字可写 */
    bool write(const std::vector<unsigned char>& data, std::string* err);
    /** @brief 关闭 TLS 和套接字 */
    void close();
    /** @brief 当前套接字。未连接时为 -1 */
    int fd() const;

private:
    SSL* m_ssl;
    int m_fd;
};

}

#endif
