/**
 * @file HttpPreview.cpp
 * @brief 用浏览器直接看被控端画面
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
#include <iphlpapi.h>

#include "HttpPreview.h"
#include "AgentSession.h"

#include <cstring>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace Vision {

const char* HttpPreview::page()
{
    return
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>\xe8\xa2\xab\xe6\x8e\xa7\xe7\xab\xaf</title>"
        "<style>html,body{margin:0;height:100%;background:#111}"
        "img{width:100%;height:100%;object-fit:contain;background:#111}</style></head><body>"
        "<img src=\"/stream\" alt=\"\">"
        "</body></html>";
}

HttpPreview::HttpPreview()
    : m_session(0)
    , m_stop(false)
    , m_listen(static_cast<uintptr_t>(INVALID_SOCKET))
    , m_port(0)
{
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
}

HttpPreview::~HttpPreview()
{
    stop();
}

void HttpPreview::setSession(AgentSession* session)
{
    m_session = session;
}

bool HttpPreview::running() const
{
    return m_thread.joinable() && !m_stop.load();
}

int HttpPreview::port() const
{
    return m_port;
}

uint64_t HttpPreview::nowMs()
{
    return GetTickCount64();
}

std::vector<std::string> HttpPreview::lanAddresses() const
{
    std::vector<std::string> out;
    ULONG size = 16 * 1024;
    std::vector<unsigned char> buf(size);
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG rc = GetAdaptersAddresses(AF_INET, flags, 0, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &size);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        buf.resize(size);
        rc = GetAdaptersAddresses(AF_INET, flags, 0, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &size);
    }
    if (rc != NO_ERROR) {
        return out;
    }
    for (IP_ADAPTER_ADDRESSES* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()); adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) {
            continue;
        }
        for (IP_ADAPTER_UNICAST_ADDRESS* uni = adapter->FirstUnicastAddress; uni; uni = uni->Next) {
            if (!uni->Address.lpSockaddr || uni->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }
            const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(uni->Address.lpSockaddr);
            char text[64];
            if (!inet_ntop(AF_INET, const_cast<IN_ADDR*>(&addr->sin_addr), text, sizeof(text))) {
                continue;
            }
            if (std::strcmp(text, "127.0.0.1") == 0) {
                continue;
            }
            out.push_back(text);
        }
    }
    return out;
}

std::vector<std::string> HttpPreview::urls() const
{
    std::vector<std::string> hosts = lanAddresses();
    if (hosts.empty()) {
        hosts.push_back("127.0.0.1");
    }
    std::vector<std::string> out;
    const int port = m_port;
    for (size_t i = 0; i < hosts.size(); ++i) {
        out.push_back("http://" + hosts[i] + ":" + std::to_string(port));
    }
    return out;
}

bool HttpPreview::writeAll(uintptr_t sock, const char* data, int size)
{
    int sent = 0;
    while (sent < size) {
        const int n = send(static_cast<SOCKET>(sock), data + sent, size - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    return true;
}

bool HttpPreview::start(int port, std::string* err)
{
    stop();
    if (port < 1 || port > 65535) {
        if (err) {
            *err = "端口无效";
        }
        return false;
    }
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        if (err) {
            *err = "创建套接字失败";
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
    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0
        || ::listen(listenSock, 4) != 0) {
        closesocket(listenSock);
        if (err) {
            *err = "端口被占用，换一个再开";
        }
        return false;
    }
    u_long nonblock = 1;
    ioctlsocket(listenSock, FIONBIO, &nonblock);
    m_listen = static_cast<uintptr_t>(listenSock);
    m_port = port;
    m_stop = false;
    m_thread = std::thread(&HttpPreview::serve, this);
    return true;
}

void HttpPreview::stop()
{
    m_stop = true;
    uintptr_t listenSock = 0;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        listenSock = m_listen;
        m_listen = static_cast<uintptr_t>(INVALID_SOCKET);
    }
    if (listenSock != 0 && listenSock != static_cast<uintptr_t>(INVALID_SOCKET)) {
        closesocket(static_cast<SOCKET>(listenSock));
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_port = 0;
}

void HttpPreview::serve()
{
    std::vector<Client> clients;
    while (!m_stop.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        SOCKET listenSock = INVALID_SOCKET;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            listenSock = static_cast<SOCKET>(m_listen);
        }
        if (listenSock == INVALID_SOCKET) {
            break;
        }
        FD_SET(listenSock, &readSet);
        for (size_t i = 0; i < clients.size(); ++i) {
            FD_SET(static_cast<SOCKET>(clients[i].sock), &readSet);
        }
        timeval wait;
        wait.tv_sec = 0;
        wait.tv_usec = 200000;
        const int ready = select(0, &readSet, 0, 0, &wait);
        if (m_stop.load()) {
            break;
        }
        if (ready > 0 && FD_ISSET(listenSock, &readSet) && clients.size() < 4) {
            SOCKET accepted = accept(listenSock, 0, 0);
            if (accepted != INVALID_SOCKET) {
                u_long block = 0;
                ioctlsocket(accepted, FIONBIO, &block);
                int timeout = 5000;
                setsockopt(accepted, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
                setsockopt(accepted, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
                Client client;
                client.sock = static_cast<uintptr_t>(accepted);
                clients.push_back(client);
            }
        }
        for (size_t i = 0; i < clients.size();) {
            Client* client = &clients[i];
            bool drop = false;
            if (ready > 0 && FD_ISSET(static_cast<SOCKET>(client->sock), &readSet) && !client->streaming) {
                char buf[1024];
                const int n = recv(static_cast<SOCKET>(client->sock), buf, sizeof(buf), 0);
                if (n <= 0) {
                    drop = true;
                } else {
                    client->pending.append(buf, buf + n);
                    if (client->pending.find("\r\n\r\n") != std::string::npos || client->pending.size() > 8192) {
                        handle(client);
                        if (!client->streaming) {
                            drop = true;
                        }
                    }
                }
            }
            if (!drop && client->streaming) {
                drop = !push(client);
            }
            if (drop) {
                closesocket(static_cast<SOCKET>(client->sock));
                clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
    }
    for (size_t i = 0; i < clients.size(); ++i) {
        closesocket(static_cast<SOCKET>(clients[i].sock));
    }
}

void HttpPreview::handle(Client* client)
{
    std::string path = "/";
    const size_t get = client->pending.find("GET ");
    if (get != std::string::npos) {
        const size_t start = get + 4;
        const size_t end = client->pending.find(' ', start);
        if (end != std::string::npos && end > start) {
            path = client->pending.substr(start, end - start);
        }
    }
    const size_t query = path.find('?');
    if (query != std::string::npos) {
        path.resize(query);
    }
    if (path == "/snap.jpg") {
        CommBytes jpeg;
        uint64_t seq = 0;
        if (!m_session || !m_session->copyHttpJpeg(&jpeg, &seq) || jpeg.empty()) {
            const char* miss = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            writeAll(client->sock, miss, static_cast<int>(std::strlen(miss)));
            return;
        }
        std::string head = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: "
            + std::to_string(jpeg.size()) + "\r\n\r\n";
        writeAll(client->sock, head.data(), static_cast<int>(head.size()));
        writeAll(client->sock, reinterpret_cast<const char*>(jpeg.data()), static_cast<int>(jpeg.size()));
        return;
    }
    if (path == "/stream") {
        const char* head =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n\r\n";
        if (!writeAll(client->sock, head, static_cast<int>(std::strlen(head)))) {
            return;
        }
        client->streaming = true;
        client->sentSeq = 0;
        client->lastMs = 0;
        return;
    }
    if (path == "/" || path == "/index.html") {
        writeAll(client->sock, page(), static_cast<int>(std::strlen(page())));
        return;
    }
    const char* miss = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    writeAll(client->sock, miss, static_cast<int>(std::strlen(miss)));
}

bool HttpPreview::push(Client* client)
{
    const uint64_t now = nowMs();
    if (client->lastMs != 0 && now - client->lastMs < 200) {
        return true;
    }
    CommBytes jpeg;
    uint64_t seq = 0;
    if (!m_session || !m_session->copyHttpJpeg(&jpeg, &seq) || jpeg.empty() || seq == client->sentSeq) {
        return true;
    }
    if (jpeg.size() > 2 * 1024 * 1024) {
        return true;
    }
    std::string head = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + std::to_string(jpeg.size()) + "\r\n\r\n";
    if (!writeAll(client->sock, head.data(), static_cast<int>(head.size()))
        || !writeAll(client->sock, reinterpret_cast<const char*>(jpeg.data()), static_cast<int>(jpeg.size()))
        || !writeAll(client->sock, "\r\n", 2)) {
        return false;
    }
    client->sentSeq = seq;
    client->lastMs = now;
    return true;
}

}
