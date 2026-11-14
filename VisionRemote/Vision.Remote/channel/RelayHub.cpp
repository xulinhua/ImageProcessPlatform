/**
 * @file RelayHub.cpp
 * @brief 中继：登记设备、配对一次会话、原样转发两边的字节。中控台进程内使用，独立中继程序也用它
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

#include <bcrypt.h>

#include "RelayHub.h"

namespace {
class Hub {
public:
    /** @brief 未监听 */
    Hub() : m_listen(INVALID_SOCKET), m_ws(INVALID_SOCKET), m_stop(false), m_port(0), m_wsPort(0) {}

    /**
     * @brief 在全部网卡上听 port
     * @return 绑定或 listen 失败返回 false
     */
    bool start(int port)
    {
        if (m_listen != INVALID_SOCKET) {
            return false;
        }
        m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listen == INVALID_SOCKET) {
            return false;
        }
        SetHandleInformation(reinterpret_cast<HANDLE>(m_listen), HANDLE_FLAG_INHERIT, 0);
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
        m_port = port;
        m_thread = std::thread(&Hub::serve, this);
        return true;
    }

    /**
     * @brief 再听一个本机 HTTP 端口，给 Cloudflare 隧道把识别码连进来
     * @note 局域网中继仍走 start() 的裸 TCP。这里只做 WebSocket 升级后的 REG/OPEN
     */
    bool startWs(int port)
    {
        if (m_ws != INVALID_SOCKET) {
            return false;
        }
        m_ws = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_ws == INVALID_SOCKET) {
            return false;
        }
        SetHandleInformation(reinterpret_cast<HANDLE>(m_ws), HANDLE_FLAG_INHERIT, 0);
        BOOL reuse = TRUE;
        setsockopt(m_ws, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(static_cast<u_short>(port));
        if (bind(m_ws, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(m_ws, 32) != 0) {
            closesocket(m_ws);
            m_ws = INVALID_SOCKET;
            return false;
        }
        m_wsPort = port;
        m_wsThread = std::thread(&Hub::serveWs, this);
        return true;
    }

    /** @brief 局域网口正在听 */
    bool listening() const
    {
        return m_listen != INVALID_SOCKET;
    }

    /** @brief 局域网端口。没在听时为 0 */
    int port() const
    {
        return m_port;
    }

    /** @brief 识别码端口。没在听时为 0 */
    int wsPort() const
    {
        return m_wsPort;
    }

    /** @brief 关掉监听口并等接受线程退出。已经在转发的会话线程不在这里收 */
    void stop()
    {
        m_stop = true;
        if (m_listen != INVALID_SOCKET) {
            closesocket(m_listen);
            m_listen = INVALID_SOCKET;
        }
        if (m_ws != INVALID_SOCKET) {
            closesocket(m_ws);
            m_ws = INVALID_SOCKET;
        }
        m_port = 0;
        m_wsPort = 0;
        if (m_thread.joinable()) {
            m_thread.join();
        }
        if (m_wsThread.joinable()) {
            m_wsThread.join();
        }
    }

private:
    /** @brief 已登记、还在等控制端来配对的被控端 */
    struct Waiter {
        SOCKET sock;            ///< 被控端套接字
        std::string token;      ///< 登记口令
        uint64_t at;            ///< 登记时刻，超过 30 秒作废
        SOCKET peer;            ///< 识别码通道上，控制端套接字。配对前为空
        /** @brief 空套接字 */
        Waiter() : sock(INVALID_SOCKET), at(0), peer(INVALID_SOCKET) {}
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
            std::thread(&Hub::splice, peer, sock, device).detach();
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
            std::thread(&Hub::onClient, this, neu).detach();
        }
    }

    static std::string b64(const unsigned char* data, int len)
    {
        static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        for (int i = 0; i < len; i += 3) {
            const unsigned int n = (static_cast<unsigned int>(data[i]) << 16)
                | ((i + 1 < len ? data[i + 1] : 0) << 8)
                | (i + 2 < len ? data[i + 2] : 0);
            out.push_back(tbl[(n >> 18) & 63]);
            out.push_back(tbl[(n >> 12) & 63]);
            out.push_back(i + 1 < len ? tbl[(n >> 6) & 63] : '=');
            out.push_back(i + 2 < len ? tbl[n & 63] : '=');
        }
        return out;
    }

    static bool sha1(const std::string& text, unsigned char dig[20])
    {
        BCRYPT_ALG_HANDLE alg = 0;
        BCRYPT_HASH_HANDLE hash = 0;
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, 0, 0) != 0) {
            return false;
        }
        DWORD objLen = 0;
        DWORD got = 0;
        BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen), sizeof(objLen), &got, 0);
        std::vector<unsigned char> obj(objLen ? objLen : 256);
        const bool ok = BCryptCreateHash(alg, &hash, obj.data(), static_cast<ULONG>(obj.size()), 0, 0, 0) == 0
            && BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())), static_cast<ULONG>(text.size()), 0) == 0
            && BCryptFinishHash(hash, dig, 20, 0) == 0;
        if (hash) {
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
        return ok;
    }

    /** @brief 读到 HTTP 头结束。慢客户端最多等套接字自己的超时 */
    static std::string readHeader(SOCKET sock)
    {
        std::string text;
        while (text.size() < 8192 && text.find("\r\n\r\n") == std::string::npos) {
            char ch = 0;
            if (recv(sock, &ch, 1, 0) <= 0) {
                return std::string();
            }
            text.push_back(ch);
        }
        return text;
    }

    static std::string headerValue(const std::string& header, const char* key)
    {
        const std::string want = std::string(key) + ":";
        size_t at = 0;
        while (at < header.size()) {
            const size_t end = header.find("\r\n", at);
            const std::string line = header.substr(at, end == std::string::npos ? std::string::npos : end - at);
            if (line.size() >= want.size()) {
                bool same = true;
                for (size_t i = 0; i < want.size(); ++i) {
                    char a = line[i];
                    char b = want[i];
                    if (a >= 'A' && a <= 'Z') {
                        a = static_cast<char>(a - 'A' + 'a');
                    }
                    if (b >= 'A' && b <= 'Z') {
                        b = static_cast<char>(b - 'A' + 'a');
                    }
                    if (a != b) {
                        same = false;
                        break;
                    }
                }
                if (same) {
                    size_t v = want.size();
                    while (v < line.size() && line[v] == ' ') {
                        v += 1;
                    }
                    return line.substr(v);
                }
            }
            if (end == std::string::npos) {
                break;
            }
            at = end + 2;
        }
        return std::string();
    }

    struct WsConn {
        SOCKET sock;
        std::string buf;
        WsConn() : sock(INVALID_SOCKET) {}
    };

    /** @brief 读一帧。文本和二进制都把负载交给调用方。连接断了返回 false */
    static bool readFrame(WsConn* conn, std::string* payload, int* opcodeOut)
    {
        for (;;) {
            while (conn->buf.size() < 2) {
                char tmp[4096];
                const int n = recv(conn->sock, tmp, sizeof(tmp), 0);
                if (n <= 0) {
                    return false;
                }
                conn->buf.append(tmp, tmp + n);
            }
            const unsigned char b1 = static_cast<unsigned char>(conn->buf[1]);
            const bool masked = (b1 & 0x80) != 0;
            unsigned long long len = b1 & 0x7f;
            size_t head = 2;
            if (len == 126) {
                head = 4;
            } else if (len == 127) {
                head = 10;
            }
            if (masked) {
                head += 4;
            }
            while (conn->buf.size() < head) {
                char tmp[4096];
                const int n = recv(conn->sock, tmp, sizeof(tmp), 0);
                if (n <= 0) {
                    return false;
                }
                conn->buf.append(tmp, tmp + n);
            }
            if ((b1 & 0x7f) == 126) {
                len = (static_cast<unsigned char>(conn->buf[2]) << 8) | static_cast<unsigned char>(conn->buf[3]);
            } else if ((b1 & 0x7f) == 127) {
                len = 0;
                for (int i = 0; i < 8; ++i) {
                    len = (len << 8) | static_cast<unsigned char>(conn->buf[2 + i]);
                }
            }
            if (len > 8 * 1024 * 1024 || conn->buf.size() > 8 * 1024 * 1024) {
                return false;
            }
            while (conn->buf.size() < head + static_cast<size_t>(len)) {
                char tmp[16384];
                const int n = recv(conn->sock, tmp, sizeof(tmp), 0);
                if (n <= 0) {
                    return false;
                }
                conn->buf.append(tmp, tmp + n);
            }
            const int opcode = static_cast<unsigned char>(conn->buf[0]) & 0x0f;
            std::string body = conn->buf.substr(head, static_cast<size_t>(len));
            if (masked) {
                const unsigned char* key = reinterpret_cast<const unsigned char*>(conn->buf.data() + head - 4);
                for (size_t i = 0; i < body.size(); ++i) {
                    body[i] = static_cast<char>(static_cast<unsigned char>(body[i]) ^ key[i % 4]);
                }
            }
            conn->buf.erase(0, head + static_cast<size_t>(len));
            if (opcode == 0x8) {
                return false;
            }
            if (opcode == 0x9) {
                writeFrame(conn->sock, 0xA, body);
                continue;
            }
            if (opcode == 0xA) {
                continue;
            }
            if (opcodeOut) {
                *opcodeOut = opcode;
            }
            if (payload) {
                payload->swap(body);
            }
            return true;
        }
    }

    static bool writeFrame(SOCKET sock, int opcode, const std::string& body)
    {
        std::string frame;
        frame.push_back(static_cast<char>(0x80 | opcode));
        if (body.size() < 126) {
            frame.push_back(static_cast<char>(body.size()));
        } else if (body.size() <= 65535) {
            frame.push_back(126);
            frame.push_back(static_cast<char>((body.size() >> 8) & 0xff));
            frame.push_back(static_cast<char>(body.size() & 0xff));
        } else {
            frame.push_back(127);
            unsigned long long n = body.size();
            char ext[8];
            for (int i = 7; i >= 0; --i) {
                ext[i] = static_cast<char>(n & 0xff);
                n >>= 8;
            }
            frame.append(ext, ext + 8);
        }
        frame.append(body);
        return writeAll(sock, frame);
    }

    struct WsWrite {
        SOCKET sock;
        std::mutex mu;
        WsWrite() : sock(INVALID_SOCKET) {}
    };

    static bool sendFrameTo(WsWrite* dst, int opcode, const std::string& body)
    {
        if (!dst) {
            return false;
        }
        std::lock_guard<std::mutex> lock(dst->mu);
        return writeFrame(dst->sock, opcode, body);
    }

    /**
     * @brief 从一侧读 WebSocket，读到一块就写到另一侧
     * @note 整帧攒齐再转发时，一张大画面会让对端超过 3 秒收不到字节，中控台会当成掉线
     */
    static void streamSide(WsConn* from, WsWrite* to, WsWrite* self, std::atomic<bool>* stop, std::atomic<unsigned long long>* bytes)
    {
        while (stop && !stop->load()) {
            while (from->buf.size() < 2) {
                char tmp[4096];
                const int n = recv(from->sock, tmp, sizeof(tmp), 0);
                if (n <= 0) {
                    stop->store(true);
                    return;
                }
                from->buf.append(tmp, tmp + n);
            }
            const unsigned char b1 = static_cast<unsigned char>(from->buf[1]);
            const bool masked = (b1 & 0x80) != 0;
            unsigned long long len = b1 & 0x7f;
            size_t head = 2;
            if (len == 126) {
                head = 4;
            } else if (len == 127) {
                head = 10;
            }
            if (masked) {
                head += 4;
            }
            while (from->buf.size() < head) {
                char tmp[4096];
                const int n = recv(from->sock, tmp, sizeof(tmp), 0);
                if (n <= 0) {
                    stop->store(true);
                    return;
                }
                from->buf.append(tmp, tmp + n);
            }
            if ((b1 & 0x7f) == 126) {
                len = (static_cast<unsigned long long>(static_cast<unsigned char>(from->buf[2])) << 8)
                    | static_cast<unsigned char>(from->buf[3]);
            } else if ((b1 & 0x7f) == 127) {
                len = 0;
                for (int i = 0; i < 8; ++i) {
                    len = (len << 8) | static_cast<unsigned char>(from->buf[2 + i]);
                }
            }
            if (len > 8 * 1024 * 1024) {
                stop->store(true);
                return;
            }
            unsigned char mask[4] = {};
            if (masked) {
                std::memcpy(mask, from->buf.data() + head - 4, 4);
            }
            const int opcode = static_cast<unsigned char>(from->buf[0]) & 0x0f;
            from->buf.erase(0, head);
            const bool forward = opcode == 0x1 || opcode == 0x2 || opcode == 0x0;
            std::string control;
            unsigned long long done = 0;
            unsigned long long left = len;
            while (left > 0) {
                if (from->buf.empty()) {
                    char tmp[16384];
                    const int n = recv(from->sock, tmp, sizeof(tmp), 0);
                    if (n <= 0) {
                        stop->store(true);
                        return;
                    }
                    from->buf.append(tmp, tmp + n);
                }
                size_t take = from->buf.size();
                if (take > left) {
                    take = static_cast<size_t>(left);
                }
                if (take > 16384) {
                    take = 16384;
                }
                std::string piece = from->buf.substr(0, take);
                from->buf.erase(0, take);
                if (masked) {
                    for (size_t i = 0; i < piece.size(); ++i) {
                        piece[i] = static_cast<char>(static_cast<unsigned char>(piece[i]) ^ mask[(done + i) % 4]);
                    }
                }
                done += piece.size();
                left -= piece.size();
                if (forward) {
                    if (!piece.empty() && !sendFrameTo(to, 2, piece)) {
                        stop->store(true);
                        return;
                    }
                    if (bytes) {
                        bytes->fetch_add(piece.size());
                    }
                } else {
                    control.append(piece);
                }
            }
            if (opcode == 0x8) {
                stop->store(true);
                return;
            }
            if (opcode == 0x9 && !sendFrameTo(self, 0xA, control)) {
                stop->store(true);
                return;
            }
        }
    }

    static void spliceWs(SOCKET left, SOCKET right, const std::string& id, const std::string& leftBuf, const std::string& leftPre)
    {
        WsConn a;
        WsConn b;
        a.sock = left;
        a.buf = leftBuf;
        b.sock = right;
        WsWrite leftWrite;
        WsWrite rightWrite;
        leftWrite.sock = left;
        rightWrite.sock = right;
        if (!leftPre.empty() && !sendFrameTo(&rightWrite, 2, leftPre)) {
            closesocket(left);
            closesocket(right);
            return;
        }
        const uint64_t began = nowMs();
        std::atomic<bool> stop(false);
        std::atomic<unsigned long long> bytes(0);
        std::thread other(&Hub::streamSide, &b, &leftWrite, &rightWrite, &stop, &bytes);
        streamSide(&a, &rightWrite, &leftWrite, &stop, &bytes);
        stop.store(true);
        shutdown(left, SD_BOTH);
        shutdown(right, SD_BOTH);
        if (other.joinable()) {
            other.join();
        }
        closesocket(left);
        closesocket(right);
        const unsigned long long forwarded = bytes.load();
        FILE* file = 0;
        if (fopen_s(&file, "relay-audit.jsonl", "ab") == 0 && file) {
            fprintf(file, "{\"event\":\"relay.access\",\"id\":\"%s\",\"ms\":%llu,\"bytes\":%llu}\n",
                id.c_str(), static_cast<unsigned long long>(nowMs() - began), forwarded);
            fclose(file);
        }
    }

    void onWs(SOCKET sock)
    {
        const std::string header = readHeader(sock);
        const std::string key = headerValue(header, "sec-websocket-key");
        if (key.empty()) {
            closesocket(sock);
            return;
        }
        unsigned char dig[20];
        if (!sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", dig)) {
            closesocket(sock);
            return;
        }
        const std::string accept = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "
            + b64(dig, 20) + "\r\n\r\n";
        if (!writeAll(sock, accept)) {
            closesocket(sock);
            return;
        }
        WsConn conn;
        conn.sock = sock;
        std::string line;
        while (line.find('\n') == std::string::npos) {
            std::string payload;
            if (!readFrame(&conn, &payload, 0)) {
                closesocket(sock);
                return;
            }
            line.append(payload);
            if (line.size() > 512) {
                closesocket(sock);
                return;
            }
        }
        char kind[16];
        char id[128];
        char token[128];
        kind[0] = id[0] = token[0] = 0;
        if (sscanf_s(line.c_str(), "%15s %127s %127s", kind, (unsigned)sizeof(kind), id, (unsigned)sizeof(id), token, (unsigned)sizeof(token)) < 2) {
            writeFrame(sock, 1, "DENY bad\n");
            closesocket(sock);
            return;
        }
        const std::string device = id;
        {
        std::lock_guard<std::mutex> lock(m_mu);
        if (std::strcmp(kind, "REG") == 0) {
            std::map<std::string, Waiter>::iterator old = m_waitWs.find(device);
            if (old != m_waitWs.end() && old->second.sock != INVALID_SOCKET) {
                closesocket(old->second.sock);
            }
            Waiter waiter;
            waiter.sock = sock;
            waiter.token = token;
            waiter.at = nowMs();
            m_waitWs[device] = waiter;
            writeFrame(sock, 1, "WAIT\n");
        } else if (std::strcmp(kind, "OPEN") == 0) {
            std::map<std::string, Waiter>::iterator it = m_waitWs.find(device);
            if (it == m_waitWs.end() || it->second.token != token) {
                writeFrame(sock, 1, "DENY\n");
                closesocket(sock);
                return;
            }
            it->second.peer = sock;
            writeFrame(it->second.sock, 1, "GO\n");
            writeFrame(sock, 1, "GO\n");
            return;
        } else {
            writeFrame(sock, 1, "DENY\n");
            closesocket(sock);
            return;
        }
        }
        // 登记后留在这条线程里应答 ping，直到控制端来配对。连接还在就一直等
        std::string early;
        for (;;) {
            SOCKET peer = INVALID_SOCKET;
            {
                std::lock_guard<std::mutex> hold(m_mu);
                std::map<std::string, Waiter>::iterator it = m_waitWs.find(device);
                if (it == m_waitWs.end() || it->second.sock != sock) {
                    closesocket(sock);
                    return;
                }
                if (it->second.peer != INVALID_SOCKET) {
                    peer = it->second.peer;
                    m_waitWs.erase(it);
                }
            }
            if (peer != INVALID_SOCKET) {
                spliceWs(sock, peer, device, conn.buf, early);
                return;
            }
            fd_set set;
            FD_ZERO(&set);
            FD_SET(sock, &set);
            timeval wait;
            wait.tv_sec = 0;
            wait.tv_usec = 200000;
            const int rc = select(0, &set, 0, 0, &wait);
            if (rc < 0) {
                std::lock_guard<std::mutex> hold(m_mu);
                m_waitWs.erase(device);
                closesocket(sock);
                return;
            }
            if (rc == 0) {
                continue;
            }
            std::string payload;
            int opcode = 0;
            if (!readFrame(&conn, &payload, &opcode)) {
                std::lock_guard<std::mutex> hold(m_mu);
                std::map<std::string, Waiter>::iterator it = m_waitWs.find(device);
                if (it != m_waitWs.end() && it->second.sock == sock) {
                    m_waitWs.erase(it);
                }
                closesocket(sock);
                return;
            }
            if (opcode == 0x2 || opcode == 0x1 || opcode == 0x0) {
                early.append(payload);
            }
        }
        writeFrame(sock, 1, "DENY\n");
        closesocket(sock);
    }

    void serveWs()
    {
        while (!m_stop) {
            SOCKET neu = accept(m_ws, 0, 0);
            if (neu == INVALID_SOCKET) {
                break;
            }
            int timeout = 30000;
            setsockopt(neu, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            std::thread(&Hub::onWs, this, neu).detach();
        }
    }

    SOCKET m_listen;                         ///< 监听套接字
    SOCKET m_ws;                             ///< 识别码 WebSocket，只绑本机，交给隧道
    int m_port;                              ///< 局域网端口，没在听时为 0
    int m_wsPort;                            ///< 识别码端口，没在听时为 0
    std::atomic<bool> m_stop;                ///< 接受线程是否该退出
    std::thread m_thread;                    ///< 接受线程
    std::thread m_wsThread;                  ///< 识别码接受线程
    std::mutex m_mu;                         ///< 保护 m_wait 和 m_waitWs
    std::map<std::string, Waiter> m_wait;    ///< 按设备 ID 等着配对的被控端
    std::map<std::string, Waiter> m_waitWs;  ///< 识别码通道上等待的被控端，连接不断开就一直留着
};

} // namespace

namespace Vision {

struct RelayHub::Impl {
    Hub hub;
};

RelayHub::RelayHub()
    : m_impl(new Impl())
{
}

RelayHub::~RelayHub()
{
    stop();
    delete m_impl;
    m_impl = 0;
}

bool RelayHub::start(int port)
{
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
    return m_impl->hub.start(port);
}

bool RelayHub::startWs(int port)
{
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
    return m_impl->hub.startWs(port);
}

void RelayHub::stop()
{
    if (!m_impl) {
        return;
    }
    m_impl->hub.stop();
}

bool RelayHub::listening() const
{
    return m_impl && m_impl->hub.listening();
}

int RelayHub::port() const
{
    return m_impl ? m_impl->hub.port() : 0;
}

int RelayHub::wsPort() const
{
    return m_impl ? m_impl->hub.wsPort() : 0;
}

} // namespace Vision