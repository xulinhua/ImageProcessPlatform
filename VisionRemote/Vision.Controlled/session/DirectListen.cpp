/**
 * @file DirectListen.cpp
 * @brief 直连监听
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "DirectListen.h"
#include "AgentSession.h"

#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace Vision {

DirectListen::DirectListen()
    : m_session(0)
    , m_stop(false)
    , m_listen(static_cast<uintptr_t>(INVALID_SOCKET))
    , m_port(0)
{
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
}

DirectListen::~DirectListen()
{
    stop();
}

bool DirectListen::running() const
{
    return m_thread.joinable() && !m_stop.load();
}

bool DirectListen::start(AgentSession* session, int port, std::string* err)
{
    stop();
    if (!session || port < 1 || port > 65535) {
        if (err) {
            *err = "直连端口无效";
        }
        return false;
    }
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        if (err) {
            *err = "创建直连套接字失败";
        }
        return false;
    }
    BOOL reuse = TRUE;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<u_short>(port));
    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(listenSock, 4) != 0) {
        closesocket(listenSock);
        if (err) {
            *err = "直连端口被占用";
        }
        return false;
    }
    m_session = session;
    m_listen = static_cast<uintptr_t>(listenSock);
    m_port = port;
    m_stop = false;
    m_thread = std::thread(&DirectListen::serve, this);
    return true;
}

void DirectListen::stop()
{
    m_stop = true;
    if (m_listen != static_cast<uintptr_t>(INVALID_SOCKET)) {
        closesocket(static_cast<SOCKET>(m_listen));
        m_listen = static_cast<uintptr_t>(INVALID_SOCKET);
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_session = 0;
    m_port = 0;
}

void DirectListen::serve()
{
    while (!m_stop.load()) {
        sockaddr_in from;
        int fromLen = sizeof(from);
        SOCKET neu = accept(static_cast<SOCKET>(m_listen), reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (neu == INVALID_SOCKET) {
            break;
        }
        char ip[64];
        inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
        char text[80];
        sprintf_s(text, "%s:%d", ip, ntohs(from.sin_port));
        if (!m_session || !m_session->offerInbound(static_cast<uintptr_t>(neu), text)) {
            closesocket(neu);
        }
    }
}

}
