/**
 * @file UdpChannel.cpp
 * @brief UdpChannel实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "UdpChannel.h"
#include "TextEncoding.h"
#include "CommNet.h"
#include <cstring>

namespace Vision {

// 套接字置为未打开
UdpChannel::UdpChannel()
    : m_sock(INVALID_SOCKET)
    , m_hasRemote(false)
{
    std::memset(&m_remote, 0, sizeof(m_remote));
}

// 析构时关闭套接字
UdpChannel::~UdpChannel()
{
    close();
}

// 绑定本机端口，并记下参数里的对端
bool UdpChannel::open(const ParamMap& params, std::string* err)
{
    close();
    commNetEnsure();
    const int port = params.value("port").toInt(9000);
    if (port < 0 || port > 65535) {
        setCommErr(err, Vision::acpToUtf8("端口无效"));
        return false;
    }
    m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_sock == INVALID_SOCKET) {
        setCommErr(err, Vision::acpToUtf8("创建套接字失败"));
        return false;
    }
    sockaddr_in local;
    std::memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(static_cast<u_short>(port));
    if (bind(m_sock, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
        setCommErr(err, Vision::acpToUtf8("端口绑定失败"));
        close();
        return false;
    }
    const std::string host = params.value("host").toString();
    const int remotePort = params.value("remotePort").toInt(0);
    if (!host.empty() && remotePort > 0 && remotePort <= 65535) {
        unsigned long ip = 0;
        if (!commResolveIpv4(host, &ip, err)) {
            close();
            return false;
        }
        m_remote.sin_family = AF_INET;
        m_remote.sin_addr.s_addr = ip;
        m_remote.sin_port = htons(static_cast<u_short>(remotePort));
        m_hasRemote = true;
        m_remoteText = commEndpointText(m_remote);
    }
    return true;
}

// 关闭套接字并清对端
void UdpChannel::close()
{
    if (m_sock != INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    m_hasRemote = false;
    m_remoteText.clear();
}

// 套接字是否有效
bool UdpChannel::isOpen() const
{
    return m_sock != INVALID_SOCKET;
}

// 已配置对端时返回其地址
std::vector<CommPeer> UdpChannel::peers() const
{
    std::vector<CommPeer> list;
    if (isOpen() && m_hasRemote) {
        CommPeer p;
        p.remote = m_remoteText;
        list.push_back(p);
    }
    return list;
}

// 发往打开时记下的对端
bool UdpChannel::write(const std::string&, const CommBytes& data, int timeoutMs, std::string* err)
{
    if (!isOpen()) {
        setCommErr(err, Vision::acpToUtf8("通道未打开"));
        return false;
    }
    if (!m_hasRemote) {
        setCommErr(err, Vision::acpToUtf8("未填写对端地址"));
        return false;
    }
    if (data.empty()) {
        return true;
    }
    const int w = commWaitSocket(m_sock, false, timeoutMs <= 0 ? 1000 : timeoutMs);
    if (w <= 0) {
        setCommErr(err, w == 0 ? "写超时" : "写失败");
        return false;
    }
    const int n = sendto(m_sock, reinterpret_cast<const char*>(&data[0]), static_cast<int>(data.size()), 0,
                          reinterpret_cast<sockaddr*>(&m_remote), sizeof(m_remote));
    if (n != static_cast<int>(data.size())) {
        setCommErr(err, Vision::acpToUtf8("写入不完整"));
        return false;
    }
    return true;
}

// 收一包；来源地址记成下一次发送目标
CommBytes UdpChannel::read(const std::string&, int maxBytes, int timeoutMs, std::string* err)
{
    CommBytes out;
    if (!isOpen()) {
        setCommErr(err, Vision::acpToUtf8("通道未打开"));
        return out;
    }
    if (maxBytes <= 0) {
        maxBytes = 4096;
    }
    const int w = commWaitSocket(m_sock, true, timeoutMs);
    if (w == 0) {
        if (timeoutMs > 0) {
            setCommErr(err, Vision::acpToUtf8("读超时"));
        }
        return out;
    }
    if (w < 0) {
        setCommErr(err, Vision::acpToUtf8("读失败"));
        return out;
    }
    out.resize(static_cast<size_t>(maxBytes));
    sockaddr_in from;
    int fromLen = sizeof(from);
    const int n = recvfrom(m_sock, reinterpret_cast<char*>(&out[0]), maxBytes, 0,
                            reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (n <= 0) {
        out.clear();
        setCommErr(err, Vision::acpToUtf8("读失败"));
        return out;
    }
    out.resize(static_cast<size_t>(n));
    m_remote = from;
    m_hasRemote = true;
    m_remoteText = commEndpointText(from);
    return out;
}

// 用零超时把接收队列读空
void UdpChannel::drain(const std::string&)
{
    if (!isOpen()) {
        return;
    }
    CommBytes junk;
    std::string ignored;
    while (true) {
        junk = read(std::string(), 4096, 0, &ignored);
        if (junk.empty()) {
            break;
        }
    }
}

// UDP 不区分接入端，始终返回空
std::string UdpChannel::waitIncoming(int)
{
    return std::string();
}

// 传输类型
CommTransport UdpChannel::transport() const
{
    return TransportUdp;
}



}
