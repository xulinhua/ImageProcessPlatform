/**
 * @file RelayMain.cpp
 * @brief 中继：登记设备、配对一次会话、原样转发两边的字节
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

#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

/**
 * @class RelayHub
 * @brief 设备登记与一次会话的字节转发
 * @note 口令不对、或登记后 30 秒没有控制端来配对，就关掉等待中的套接字。配对后两边先收到 GO，再原样对拷
 */
class RelayHub {
public:
    /** @brief 未监听 */
    RelayHub() : m_listen(INVALID_SOCKET), m_stop(false) {}

    /**
     * @brief 在全部网卡上听 port
     * @return 绑定或 listen 失败返回 false
     */
    bool start(int port)
    {
        m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listen == INVALID_SOCKET) {
            return false;
        }
        BOOL reuse = TRUE;
        setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(static_cast<u_short>(port));
        if (bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(m_listen, 32) != 0) {
            closesocket(m_listen);
            m_listen = INVALID_SOCKET;
            return false;
        }
        m_stop = false;
        m_thread = std::thread(&RelayHub::serve, this);
        return true;
    }

    /** @brief 关掉监听口并等接受线程退出。已经在转发的会话线程不在这里收 */
    void stop()
    {
        m_stop = true;
        if (m_listen != INVALID_SOCKET) {
            closesocket(m_listen);
            m_listen = INVALID_SOCKET;
        }
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

private:
    /** @brief 已登记、还在等控制端来配对的被控端 */
    struct Waiter {
        SOCKET sock;            ///< 被控端套接字
        std::string token;      ///< 登记口令
        uint64_t at;            ///< 登记时刻，超过 30 秒作废
        /** @brief 空套接字 */
        Waiter() : sock(INVALID_SOCKET), at(0) {}
    };

    /** @brief 开机以来的毫秒 */
    static uint64_t nowMs()
    {
        return GetTickCount64();
    }

    /** @brief 把一整段发出去。对端断开返回 false */
    static bool writeAll(SOCKET sock, const std::string& text)
    {
        int sent = 0;
        while (sent < static_cast<int>(text.size())) {
            const int n = send(sock, text.data() + sent, static_cast<int>(text.size()) - sent, 0);
            if (n <= 0) {
                return false;
            }
            sent += n;
        }
        return true;
    }

    /** @brief 读到换行或 512 字节。对端断开返回空 */
    static std::string readLine(SOCKET sock)
    {
        std::string line;
        while (line.size() < 512) {
            char ch = 0;
            const int n = recv(sock, &ch, 1, 0);
            if (n <= 0) {
                return std::string();
            }
            line.push_back(ch);
            if (ch == '\n') {
                break;
            }
        }
        return line;
    }

    /** @brief 两边对拷直到任一方断开，然后写一行审计。调用方不再使用这两个套接字 */
    static void splice(SOCKET left, SOCKET right, const std::string& id)
    {
        const uint64_t began = nowMs();
        unsigned long long bytes = 0;
        while (true) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(left, &set);
            FD_SET(right, &set);
            timeval wait;
            wait.tv_sec = 1;
            wait.tv_usec = 0;
            if (select(0, &set, 0, 0, &wait) < 0) {
                break;
            }
            SOCKET from = INVALID_SOCKET;
            SOCKET to = INVALID_SOCKET;
            if (FD_ISSET(left, &set)) {
                from = left;
                to = right;
            } else if (FD_ISSET(right, &set)) {
                from = right;
                to = left;
            } else {
                continue;
            }
            char buf[16384];
            const int n = recv(from, buf, sizeof(buf), 0);
            if (n <= 0) {
                break;
            }
            int sent = 0;
            while (sent < n) {
                const int w = send(to, buf + sent, n - sent, 0);
                if (w <= 0) {
                    sent = -1;
                    break;
                }
                sent += w;
            }
            if (sent < 0) {
                break;
            }
            bytes += static_cast<unsigned long long>(n);
        }
        closesocket(left);
        closesocket(right);
        FILE* file = 0;
        if (fopen_s(&file, "relay-audit.jsonl", "ab") == 0 && file) {
            fprintf(file, "{\"event\":\"relay.session\",\"id\":\"%s\",\"ms\":%llu,\"bytes\":%llu}\n",
                id.c_str(), static_cast<unsigned long long>(nowMs() - began), bytes);
            fclose(file);
        }
    }

    /** @brief 丢掉登记超过 30 秒的被控端。调用方须持有 m_mu */
    void dropExpired()
    {
        const uint64_t now = nowMs();
        for (std::map<std::string, Waiter>::iterator it = m_wait.begin(); it != m_wait.end();) {
            if (now - it->second.at > 30000) {
                if (it->second.sock != INVALID_SOCKET) {
                    writeAll(it->second.sock, "DENY timeout\n");
                    closesocket(it->second.sock);
                }
                it = m_wait.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief 处理一行登记或配对
     * @note REG 留下等 OPEN。口令一致才回 GO 并开始转发
     */
    void onClient(SOCKET sock)
    {
        const std::string line = readLine(sock);
        if (line.size() < 4) {
            closesocket(sock);
            return;
        }
        char kind[16];
        char id[128];
        char token[128];
        kind[0] = id[0] = token[0] = 0;
        if (sscanf_s(line.c_str(), "%15s %127s %127s", kind, (unsigned)sizeof(kind), id, (unsigned)sizeof(id), token, (unsigned)sizeof(token)) < 2) {
            writeAll(sock, "DENY bad\n");
            closesocket(sock);
            return;
        }
        std::lock_guard<std::mutex> lock(m_mu);
        dropExpired();
        const std::string device = id;
        if (std::strcmp(kind, "REG") == 0) {
            std::map<std::string, Waiter>::iterator old = m_wait.find(device);
            if (old != m_wait.end() && old->second.sock != INVALID_SOCKET) {
                closesocket(old->second.sock);
            }
            Waiter waiter;
            waiter.sock = sock;
            waiter.token = token;
            waiter.at = nowMs();
            m_wait[device] = waiter;
            writeAll(sock, "WAIT\n");
            return;
        }
        if (std::strcmp(kind, "OPEN") == 0) {
            std::map<std::string, Waiter>::iterator it = m_wait.find(device);
            if (it == m_wait.end() || it->second.token != token) {
                writeAll(sock, "DENY\n");
                closesocket(sock);
                return;
            }
            SOCKET peer = it->second.sock;
            m_wait.erase(it);
            writeAll(peer, "GO\n");
            writeAll(sock, "GO\n");
            std::thread(&RelayHub::splice, peer, sock, device).detach();
            return;
        }
        writeAll(sock, "DENY\n");
        closesocket(sock);
    }

    /** @brief 接受连接。每条连接单独读第一行，避免慢客户端堵住监听 */
    void serve()
    {
        while (!m_stop) {
            SOCKET neu = accept(m_listen, 0, 0);
            if (neu == INVALID_SOCKET) {
                break;
            }
            int timeout = 30000;
            setsockopt(neu, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            std::thread(&RelayHub::onClient, this, neu).detach();
        }
    }

    SOCKET m_listen;                         ///< 监听套接字
    std::atomic<bool> m_stop;                ///< 接受线程是否该退出
    std::thread m_thread;                    ///< 接受线程
    std::mutex m_mu;                         ///< 保护 m_wait
    std::map<std::string, Waiter> m_wait;    ///< 按设备 ID 等着配对的被控端
};

/** @brief 端口默认 19200，第一个参数可改端口 */
int main(int argc, char** argv)
{
    int port = 19200;
    if (argc >= 2) {
        port = atoi(argv[1]);
    }
    if (port < 1 || port > 65535) {
        port = 19200;
    }
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return 1;
    }
    RelayHub hub;
    if (!hub.start(port)) {
        fprintf(stderr, "listen %d failed\n", port);
        WSACleanup();
        return 1;
    }
    printf("RemoteRelay listening %d\n", port);
    for (;;) {
        Sleep(1000);
    }
}

