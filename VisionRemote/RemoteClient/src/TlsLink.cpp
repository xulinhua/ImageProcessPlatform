/**
 * @file TlsLink.cpp
 * @brief TLS 1.2 客户端
 */

#include "TlsLink.h"

#include <openssl/err.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Vision {

namespace {

bool waitFd(int fd, bool write, int timeoutMs)
{
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = write ? POLLOUT : POLLIN;
    pfd.revents = 0;
    return poll(&pfd, 1, timeoutMs) > 0;
}

}

TlsLink::TlsLink()
    : m_ssl(0)
    , m_fd(-1)
{
}

TlsLink::~TlsLink()
{
    close();
}

int TlsLink::fd() const
{
    return m_fd;
}

void TlsLink::close()
{
    if (m_ssl) {
        SSL_shutdown(m_ssl);
        SSL_free(m_ssl);
        m_ssl = 0;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool TlsLink::handshake(int fd, SSL_CTX* ctx, std::string* err)
{
    close();
    m_fd = fd;
    int one = 1;
    setsockopt(m_fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
    timeval tv;
    tv.tv_sec = 8;
    tv.tv_usec = 0;
    setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // 本机是客户端。直连进来的套接字同样 SSL_connect，因为中控台 accept 之后做的是服务端握手。
    m_ssl = SSL_new(ctx);
    if (!m_ssl) {
        if (err) {
            *err = "SSL_new 失败";
        }
        close();
        return false;
    }
    SSL_set_fd(m_ssl, m_fd);
    SSL_set_connect_state(m_ssl);
    const int rc = SSL_connect(m_ssl);
    if (rc != 1) {
        if (err) {
            *err = "TLS 握手失败";
        }
        close();
        return false;
    }
    const int flags = fcntl(m_fd, F_GETFL, 0);
    fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
    return true;
}

bool TlsLink::read(std::vector<unsigned char>* out, std::string* err)
{
    out->clear();
    if (!m_ssl) {
        if (err) {
            *err = "没有会话";
        }
        return false;
    }
    unsigned char buf[16384];
    for (;;) {
        const int n = SSL_read(m_ssl, buf, sizeof(buf));
        if (n > 0) {
            out->insert(out->end(), buf, buf + n);
            if (static_cast<size_t>(n) < sizeof(buf) && !SSL_pending(m_ssl)) {
                return true;
            }
            continue;
        }
        const int code = SSL_get_error(m_ssl, n);
        if (!out->empty() && (code == SSL_ERROR_WANT_READ || code == SSL_ERROR_WANT_WRITE)) {
            return true;
        }
        if (code == SSL_ERROR_WANT_READ || code == SSL_ERROR_WANT_WRITE) {
            return true;
        }
        if (code == SSL_ERROR_ZERO_RETURN) {
            if (err) {
                *err = "对端关闭";
            }
            return false;
        }
        if (err) {
            *err = "读失败";
        }
        return false;
    }
}

bool TlsLink::write(const std::vector<unsigned char>& data, std::string* err)
{
    if (!m_ssl) {
        if (err) {
            *err = "没有会话";
        }
        return false;
    }
    size_t off = 0;
    while (off < data.size()) {
        const int n = SSL_write(m_ssl, data.data() + off, static_cast<int>(data.size() - off));
        if (n > 0) {
            off += static_cast<size_t>(n);
            continue;
        }
        const int code = SSL_get_error(m_ssl, n);
        if (code == SSL_ERROR_WANT_READ || code == SSL_ERROR_WANT_WRITE) {
            if (!waitFd(m_fd, code == SSL_ERROR_WANT_WRITE, 2000)) {
                if (err) {
                    *err = "写超时";
                }
                return false;
            }
            continue;
        }
        if (err) {
            *err = "写失败";
        }
        return false;
    }
    return true;
}

}
