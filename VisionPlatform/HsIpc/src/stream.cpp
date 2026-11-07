/**
 * @file stream.cpp
 * @brief 命名管道和 TCP。两者都按 32 字节头加体收发
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 管道名是 \\.\pipe\HsIpc.<通道名>，先 CreateNamedPipe 的一方做服务端。
 * 套接字先 bind 成功的一方监听，另一方来连接。两边都要填同一个端口。
 * 管道缓冲和套接字缓冲都放到能容下两个分片，避免同一进程里先写后读时写满堵住。
 */
#include "transport.h"
#include "names.h"

#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

namespace HsIpc {

static int waitOverlap(HANDLE file, OVERLAPPED* ov, int timeoutMs)
{
    const DWORD wait = WaitForSingleObject(ov->hEvent, timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs);
    if (wait == WAIT_TIMEOUT) {
        CancelIoEx(file, ov);
        WaitForSingleObject(ov->hEvent, INFINITE);
        return HS_IPC_TIMEOUT;
    }
    if (wait != WAIT_OBJECT_0) {
        return HS_IPC_ERR_IO;
    }
    return HS_IPC_OK;
}

static int transfer(HANDLE file, bool write, uint8_t* data, uint32_t n, int timeoutMs, HANDLE event)
{
    uint32_t done = 0;
    while (done < n) {
        OVERLAPPED ov;
        memset(&ov, 0, sizeof(ov));
        ov.hEvent = event;
        ResetEvent(event);
        DWORD got = 0;
        const BOOL ok = write
            ? WriteFile(file, data + done, n - done, &got, &ov)
            : ReadFile(file, data + done, n - done, &got, &ov);
        if (!ok) {
            const DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING) {
                return HS_IPC_ERR_IO;
            }
            const int waited = waitOverlap(file, &ov, timeoutMs);
            if (waited != HS_IPC_OK) {
                return waited;
            }
            if (!GetOverlappedResult(file, &ov, &got, FALSE)) {
                return HS_IPC_ERR_IO;
            }
        }
        if (got == 0) {
            return HS_IPC_ERR_IO;
        }
        done += got;
    }
    return HS_IPC_OK;
}

class PipeTransport : public Transport {
public:
    PipeTransport()
        : m_pipe(INVALID_HANDLE_VALUE)
        , m_event(0)
        , m_server(false)
        , m_connected(false)
        , m_connectPending(false)
    {
        memset(&m_connectOv, 0, sizeof(m_connectOv));
    }

    ~PipeTransport() { close(); }

    bool open(const char* name, const char* host, int port, int capacity,
        uint32_t imageSlotBytes, uint32_t fileSlotBytes, std::string* err)
    {
        (void)host;
        (void)port;
        (void)capacity;
        (void)imageSlotBytes;
        (void)fileSlotBytes;
        m_event = CreateEventW(0, TRUE, FALSE, 0);
        if (!m_event) {
            if (err) {
                *err = "创建管道事件失败";
            }
            return false;
        }
        Security security;
        const std::wstring path = L"\\\\.\\pipe\\HsIpc." + widen(name);
        m_pipe = CreateNamedPipeW(path.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
            4u * 1024u * 1024u, 4u * 1024u * 1024u, 0, security.attr());
        if (m_pipe != INVALID_HANDLE_VALUE) {
            m_server = true;
            return true;
        }
        const DWORD created = GetLastError();
        if (created != ERROR_PIPE_BUSY && created != ERROR_ACCESS_DENIED) {
            if (err) {
                *err = "创建命名管道失败";
            }
            return false;
        }
        m_pipe = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, 0);
        if (m_pipe == INVALID_HANDLE_VALUE) {
            if (err) {
                *err = "打开命名管道失败";
            }
            return false;
        }
        m_server = false;
        m_connected = true;
        return true;
    }

    void close()
    {
        if (m_pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
        }
        if (m_event) {
            CloseHandle(m_event);
            m_event = 0;
        }
    }

    int sendFrame(const uint8_t* frame, uint32_t bytes)
    {
        const int connected = ensure(5000);
        if (connected != HS_IPC_OK) {
            return connected;
        }
        return transfer(m_pipe, true, (uint8_t*)frame, bytes, 5000, m_event);
    }

    int recvFrame(std::vector<uint8_t>* frame, int timeoutMs)
    {
        const int connected = ensure(timeoutMs);
        if (connected != HS_IPC_OK) {
            return connected;
        }
        uint8_t head[32];
        const int readHead = transfer(m_pipe, false, head, 32, timeoutMs, m_event);
        if (readHead != HS_IPC_OK) {
            return readHead;
        }
        uint32_t bodyBytes = 0;
        memcpy(&bodyBytes, head + 24, sizeof(bodyBytes));
        if (bodyBytes > HS_IPC_CHUNK_BYTES + 8192u) {
            return HS_IPC_ERR_SIZE;
        }
        frame->resize(32u + bodyBytes);
        memcpy(&(*frame)[0], head, 32);
        if (!bodyBytes) {
            return HS_IPC_OK;
        }
        return transfer(m_pipe, false, &(*frame)[32], bodyBytes, timeoutMs < 0 ? -1 : timeoutMs, m_event);
    }

private:
    int ensure(int timeoutMs)
    {
        if (!m_server) {
            return m_connected ? HS_IPC_OK : HS_IPC_ERR_IO;
        }
        if (m_connected) {
            return HS_IPC_OK;
        }
        if (!m_connectPending) {
            memset(&m_connectOv, 0, sizeof(m_connectOv));
            m_connectOv.hEvent = m_event;
            ResetEvent(m_event);
            const BOOL ok = ConnectNamedPipe(m_pipe, &m_connectOv);
            if (ok) {
                m_connected = true;
                return HS_IPC_OK;
            }
            const DWORD error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED) {
                m_connected = true;
                return HS_IPC_OK;
            }
            if (error != ERROR_IO_PENDING) {
                return HS_IPC_ERR_IO;
            }
            m_connectPending = true;
        }
        const int waited = waitOverlap(m_pipe, &m_connectOv, timeoutMs);
        if (waited != HS_IPC_OK) {
            m_connectPending = false;
            return waited;
        }
        m_connected = true;
        m_connectPending = false;
        return HS_IPC_OK;
    }

    HANDLE m_pipe;
    HANDLE m_event;
    OVERLAPPED m_connectOv;
    bool m_server;
    bool m_connected;
    bool m_connectPending;
};

class SocketTransport : public Transport {
public:
    SocketTransport()
        : m_listen(INVALID_SOCKET)
        , m_sock(INVALID_SOCKET)
        , m_server(false)
    {
    }

    ~SocketTransport() { close(); }

    bool open(const char* name, const char* host, int port, int capacity,
        uint32_t imageSlotBytes, uint32_t fileSlotBytes, std::string* err)
    {
        (void)name;
        (void)capacity;
        (void)imageSlotBytes;
        (void)fileSlotBytes;
        if (port <= 0 || port > 65535) {
            if (err) {
                *err = "套接字端口无效";
            }
            return false;
        }
        static bool started = false;
        if (!started) {
            WSADATA data;
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
                if (err) {
                    *err = "初始化套接字失败";
                }
                return false;
            }
            started = true;
        }
        const char* address = (host && host[0]) ? host : "127.0.0.1";
        m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listen == INVALID_SOCKET) {
            if (err) {
                *err = "创建套接字失败";
            }
            return false;
        }
        sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons((u_short)port);
        inet_pton(AF_INET, address, &local.sin_addr);
        if (bind(m_listen, (sockaddr*)&local, sizeof(local)) == 0 && listen(m_listen, 1) == 0) {
            m_server = true;
            u_long nb = 1;
            ioctlsocket(m_listen, FIONBIO, &nb);
            return true;
        }
        closesocket(m_listen);
        m_listen = INVALID_SOCKET;
        m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_sock == INVALID_SOCKET || connect(m_sock, (sockaddr*)&local, sizeof(local)) != 0) {
            if (err) {
                *err = "连接套接字失败";
            }
            return false;
        }
        tune(m_sock);
        m_server = false;
        return true;
    }

    void close()
    {
        if (m_sock != INVALID_SOCKET) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
        }
        if (m_listen != INVALID_SOCKET) {
            closesocket(m_listen);
            m_listen = INVALID_SOCKET;
        }
    }

    int sendFrame(const uint8_t* frame, uint32_t bytes)
    {
        const int connected = ensure(5000);
        if (connected != HS_IPC_OK) {
            return connected;
        }
        return sendAll(frame, bytes);
    }

    int recvFrame(std::vector<uint8_t>* frame, int timeoutMs)
    {
        const int connected = ensure(timeoutMs);
        if (connected != HS_IPC_OK) {
            return connected;
        }
        uint8_t head[32];
        const int readHead = recvAll(head, 32, timeoutMs);
        if (readHead != HS_IPC_OK) {
            return readHead;
        }
        uint32_t bodyBytes = 0;
        memcpy(&bodyBytes, head + 24, sizeof(bodyBytes));
        if (bodyBytes > HS_IPC_CHUNK_BYTES + 8192u) {
            return HS_IPC_ERR_SIZE;
        }
        frame->resize(32u + bodyBytes);
        memcpy(&(*frame)[0], head, 32);
        if (!bodyBytes) {
            return HS_IPC_OK;
        }
        return recvAll(&(*frame)[32], bodyBytes, timeoutMs < 0 ? -1 : timeoutMs);
    }

private:
    static void tune(SOCKET sock)
    {
        const int nodelay = 1;
        const int buf = 4 * 1024 * 1024;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&buf, sizeof(buf));
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&buf, sizeof(buf));
    }

    int ensure(int timeoutMs)
    {
        if (!m_server) {
            return m_sock != INVALID_SOCKET ? HS_IPC_OK : HS_IPC_ERR_IO;
        }
        if (m_sock != INVALID_SOCKET) {
            return HS_IPC_OK;
        }
        fd_set set;
        FD_ZERO(&set);
        FD_SET(m_listen, &set);
        timeval tv;
        timeval* ptv = 0;
        if (timeoutMs >= 0) {
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;
            ptv = &tv;
        }
        const int selected = select(0, &set, 0, 0, ptv);
        if (selected == 0) {
            return HS_IPC_TIMEOUT;
        }
        if (selected < 0) {
            return HS_IPC_ERR_IO;
        }
        m_sock = accept(m_listen, 0, 0);
        if (m_sock == INVALID_SOCKET) {
            return HS_IPC_ERR_IO;
        }
        tune(m_sock);
        return HS_IPC_OK;
    }

    int sendAll(const uint8_t* data, uint32_t n)
    {
        uint32_t done = 0;
        while (done < n) {
            const int wrote = send(m_sock, (const char*)data + done, (int)(n - done), 0);
            if (wrote <= 0) {
                return HS_IPC_ERR_IO;
            }
            done += (uint32_t)wrote;
        }
        return HS_IPC_OK;
    }

    int recvAll(uint8_t* data, uint32_t n, int timeoutMs)
    {
        uint32_t done = 0;
        while (done < n) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(m_sock, &set);
            timeval tv;
            timeval* ptv = 0;
            if (timeoutMs >= 0) {
                tv.tv_sec = timeoutMs / 1000;
                tv.tv_usec = (timeoutMs % 1000) * 1000;
                ptv = &tv;
            }
            const int selected = select(0, &set, 0, 0, ptv);
            if (selected == 0) {
                return HS_IPC_TIMEOUT;
            }
            if (selected < 0) {
                return HS_IPC_ERR_IO;
            }
            const int got = recv(m_sock, (char*)data + done, (int)(n - done), 0);
            if (got <= 0) {
                return HS_IPC_ERR_IO;
            }
            done += (uint32_t)got;
        }
        return HS_IPC_OK;
    }

    SOCKET m_listen;
    SOCKET m_sock;
    bool m_server;
};

Transport* createPipe()
{
    return new PipeTransport();
}

Transport* createSocket()
{
    return new SocketTransport();
}

}
