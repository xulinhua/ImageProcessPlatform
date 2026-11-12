/**
 * @file Beacon.cpp
 * @brief UDP 19101 应答，报文和 Windows 被控端的 RemoteBeacon 一致
 */

#include "Beacon.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Vision {

Beacon::Beacon()
    : m_fd(-1)
    , m_run(false)
{
}

Beacon::~Beacon()
{
    close();
}

bool Beacon::listen(std::string* err)
{
    close();
    m_fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_fd < 0) {
        if (err) {
            *err = "发现端口没有打开";
        }
        return false;
    }
    int reuse = 1;
    ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(RemoteConst::kDiscoverPort));
    if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(m_fd);
        m_fd = -1;
        if (err) {
            *err = "发现端口 19101 被占用";
        }
        return false;
    }
    m_run = true;
    m_thread = std::thread(&Beacon::serve, this);
    return true;
}

void Beacon::setInfo(const HelloMsg& info)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_info = info;
}

std::string Beacon::consoleHost()
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_console;
}

void Beacon::close()
{
    m_run = false;
    if (m_fd >= 0) {
        ::shutdown(m_fd, SHUT_RDWR);
        ::close(m_fd);
        m_fd = -1;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void Beacon::serve()
{
    while (m_run.load()) {
        char buf[2048];
        sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        const int n = ::recvfrom(m_fd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (!m_run.load()) {
            break;
        }
        if (n <= 0) {
            continue;
        }
        CommBytes chunk(buf, buf + n);
        Frame frame;
        std::vector<Frame::Packet> packets;
        std::string err;
        if (!frame.push(chunk, &packets, &err) || packets.empty() || packets[0].type != Frame::TypeProbe) {
            continue;
        }
        char ip[64];
        if (inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip)) && std::strcmp(ip, "0.0.0.0") != 0) {
            std::lock_guard<std::mutex> lock(m_mu);
            m_console = ip;
        }
        HelloMsg info;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            info = m_info;
        }
        const CommBytes reply = Messages::packAnnounce(info, &err);
        if (!reply.empty() && m_fd >= 0) {
            ::sendto(m_fd, reply.data(), reply.size(), 0, reinterpret_cast<sockaddr*>(&from), fromLen);
        }
    }
}

}
