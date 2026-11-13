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
#include <shellapi.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <bcrypt.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")

/**
 * @class RelayHub
 * @brief 设备登记与一次会话的字节转发
 * @note 口令不对、或登记后 30 秒没有控制端来配对，就关掉等待中的套接字。配对后两边先收到 GO，再原样对拷
 */
class RelayHub {
public:
    /** @brief 未监听 */
    RelayHub() : m_listen(INVALID_SOCKET), m_ws(INVALID_SOCKET), m_stop(false) {}

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

    /**
     * @brief 再听一个本机 HTTP 端口，给 Cloudflare 隧道把识别码连进来
     * @note 局域网中继仍走 start() 的裸 TCP。这里只做 WebSocket 升级后的 REG/OPEN
     */
    bool startWs(int port)
    {
        m_ws = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_ws == INVALID_SOCKET) {
            return false;
        }
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
        m_wsThread = std::thread(&RelayHub::serveWs, this);
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
        if (m_ws != INVALID_SOCKET) {
            closesocket(m_ws);
            m_ws = INVALID_SOCKET;
        }
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
        std::thread other(&RelayHub::streamSide, &b, &leftWrite, &rightWrite, &stop, &bytes);
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
            std::thread(&RelayHub::onWs, this, neu).detach();
        }
    }

    SOCKET m_listen;                         ///< 监听套接字
    SOCKET m_ws;                             ///< 识别码 WebSocket，只绑本机，交给隧道
    std::atomic<bool> m_stop;                ///< 接受线程是否该退出
    std::thread m_thread;                    ///< 接受线程
    std::thread m_wsThread;                  ///< 识别码接受线程
    std::mutex m_mu;                         ///< 保护 m_wait 和 m_waitWs
    std::map<std::string, Waiter> m_wait;    ///< 按设备 ID 等着配对的被控端
    std::map<std::string, Waiter> m_waitWs;  ///< 识别码通道上等待的被控端，连接不断开就一直留着
};

/**
 * @class RelayWindow
 * @brief 中继窗口和托盘。关掉窗口只是隐藏，托盘菜单里的退出才结束进程
 */
class RelayWindow {
public:
    /** @brief 字体和托盘都还没建 */
    RelayWindow()
        : m_hwnd(nullptr)
        , m_icon(nullptr)
        , m_quit(false)
        , m_port(0)
        , m_wsPort(0)
        , m_dpi(96)
        , m_titleFont(nullptr)
        , m_subFont(nullptr)
        , m_labelFont(nullptr)
        , m_valueFont(nullptr)
        , m_bodyFont(nullptr)
    {
        std::memset(&m_nid, 0, sizeof(m_nid));
    }

    /** @brief 已经有一份中继时，把它的窗口拉到前台 */
    static void activateExisting()
    {
        HWND hwnd = FindWindowW(L"VisionRemoteRelayWindow", L"中继");
        if (!hwnd) {
            return;
        }
        ShowWindow(hwnd, SW_SHOW);
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }
        SetForegroundWindow(hwnd);
    }

    /** @brief 建窗口、放托盘图标。失败返回 false */
    bool open(int port, int wsPort)
    {
        m_port = port;
        m_wsPort = wsPort;
        m_quit = false;
        m_icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
        HDC screen = GetDC(nullptr);
        if (screen) {
            m_dpi = GetDeviceCaps(screen, LOGPIXELSX);
            if (m_dpi < 96) {
                m_dpi = 96;
            }
            ReleaseDC(nullptr, screen);
        }
        WNDCLASSW wc = {};
        wc.lpfnWndProc = &RelayWindow::proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hIcon = m_icon;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"VisionRemoteRelayWindow";
        RegisterClassW(&wc);
        const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT frame = { 0, 0, px(640), px(400) };
        AdjustWindowRectEx(&frame, style, FALSE, 0);
        const int width = frame.right - frame.left;
        const int height = frame.bottom - frame.top;
        const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
        m_hwnd = CreateWindowExW(0, wc.lpszClassName, L"中继",
            style, x, y, width, height, nullptr, nullptr, wc.hInstance, this);
        if (!m_hwnd) {
            return false;
        }
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        return true;
    }

    /** @brief 消息循环。返回前托盘图标已去掉 */
    int loop()
    {
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return 0;
    }

private:
    static const UINT kTray = WM_APP + 1;

    /** @brief 窗口过程。指针放在 GWLP_USERDATA */
    static LRESULT CALLBACK proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        RelayWindow* self = reinterpret_cast<RelayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            CREATESTRUCTW* created = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = reinterpret_cast<RelayWindow*>(created->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->m_hwnd = hwnd;
        }
        if (!self) {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }
        if (message == WM_CREATE) {
            self->onCreate(hwnd);
            return 0;
        }
        if (message == WM_ERASEBKGND) {
            return 1;
        }
        if (message == WM_PAINT) {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            self->paint(dc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        if (message == WM_CLOSE) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        if (message == kTray) {
            self->onTray(lparam);
            return 0;
        }
        if (message == WM_COMMAND && LOWORD(wparam) == 1) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            return 0;
        }
        if (message == WM_COMMAND && LOWORD(wparam) == 2) {
            self->quit();
            return 0;
        }
        if (message == WM_DESTROY) {
            self->releaseFonts();
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    /** @brief 按当前 DPI 把设计像素换成屏幕像素 */
    int px(int dip) const
    {
        return MulDiv(dip, m_dpi, 96);
    }

    /** @brief 标题、数字和说明各一把字体，窗口关掉时释放 */
    void ensureFonts()
    {
        if (m_titleFont) {
            return;
        }
        m_titleFont = makeFont(28, FW_SEMIBOLD);
        m_subFont = makeFont(13, FW_NORMAL);
        m_labelFont = makeFont(12, FW_NORMAL);
        m_valueFont = makeFont(26, FW_SEMIBOLD);
        m_bodyFont = makeFont(13, FW_NORMAL);
    }

    /** @brief Segoe UI，按设计字号随 DPI 放大 */
    HFONT makeFont(int dip, int weight) const
    {
        return CreateFontW(-px(dip), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    /** @brief 整块铺色 */
    void fill(HDC dc, const RECT& box, COLORREF color) const
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &box, brush);
        DeleteObject(brush);
    }

    /** @brief 圆角卡片。边框和填充同一支笔刷画出来 */
    void fillRound(HDC dc, const RECT& box, int radius, COLORREF color, COLORREF edge) const
    {
        HBRUSH brush = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_SOLID, 1, edge);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        RoundRect(dc, box.left, box.top, box.right, box.bottom, radius, radius);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    /** @brief 透明底的一行字 */
    void drawText(HDC dc, const RECT& box, const wchar_t* text, HFONT font, COLORREF color, UINT format) const
    {
        HGDIOBJ old = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        RECT area = box;
        DrawTextW(dc, text, -1, &area, format);
        SelectObject(dc, old);
    }

    /** @brief 页头右侧的两个白点，和任务栏图标同一个图形 */
    void drawMark(HDC dc, int x, int y, int size) const
    {
        HPEN pen = CreatePen(PS_SOLID, px(3), RGB(255, 255, 255));
        HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        const int cy = y + size / 2;
        const int rad = size / 7;
        MoveToEx(dc, x + size / 4, cy, nullptr);
        LineTo(dc, x + size * 3 / 4, cy);
        Ellipse(dc, x + size / 5 - rad, cy - rad, x + size / 5 + rad, cy + rad);
        Ellipse(dc, x + size * 4 / 5 - rad, cy - rad, x + size * 4 / 5 + rad, cy + rad);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    /** @brief 双缓冲画页头、两张端口卡和托盘说明 */
    void paint(HDC target)
    {
        ensureFonts();
        RECT client;
        GetClientRect(m_hwnd, &client);
        const int width = client.right;
        const int height = client.bottom;
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

        RECT page = { 0, 0, width, height };
        fill(dc, page, RGB(245, 245, 245));
        RECT head = { 0, 0, width, px(108) };
        fill(dc, head, RGB(194, 65, 12));
        RECT title = { px(28), px(26), width - px(96), px(62) };
        drawText(dc, title, L"中继", m_titleFont, RGB(255, 255, 255), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        RECT sub = { px(28), px(62), width - px(96), px(92) };
        drawText(dc, sub, L"局域网配对和识别码转发", m_subFont, RGB(255, 220, 200), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        drawMark(dc, width - px(72), px(36), px(36));

        const int gap = px(16);
        const int cardTop = px(132);
        const int cardH = px(168);
        const int cardW = (width - gap * 3) / 2;
        RECT left = { gap, cardTop, gap + cardW, cardTop + cardH };
        RECT right = { gap * 2 + cardW, cardTop, gap * 2 + cardW * 2, cardTop + cardH };
        fillRound(dc, left, px(12), RGB(255, 255, 255), RGB(232, 232, 232));
        fillRound(dc, right, px(12), RGB(255, 255, 255), RGB(232, 232, 232));

        wchar_t lan[32];
        wchar_t access[64];
        swprintf_s(lan, L"%d", m_port);
        swprintf_s(access, L"127.0.0.1:%d", m_wsPort);
        paintCard(dc, left, L"局域网", lan, L"同一网段的中控台和被控端在这里配对。");
        paintCard(dc, right, L"识别码", access, L"只监听本机，交给隧道转发出去。");

        RECT foot = { px(20), height - px(48), width - px(20), height - px(16) };
        drawText(dc, foot, L"关闭窗口会留在任务栏托盘。要退出，在托盘图标上右键。",
            m_bodyFont, RGB(140, 140, 140), DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }

    /** @brief 一张端口卡：小标题、大数字、一行说明 */
    void paintCard(HDC dc, const RECT& box, const wchar_t* name, const wchar_t* value, const wchar_t* note) const
    {
        RECT label = { box.left + px(20), box.top + px(18), box.right - px(16), box.top + px(40) };
        drawText(dc, label, name, m_labelFont, RGB(140, 140, 140), DT_LEFT | DT_SINGLELINE);
        RECT number = { box.left + px(20), box.top + px(44), box.right - px(12), box.top + px(88) };
        drawText(dc, number, value, m_valueFont, RGB(38, 38, 38), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT body = { box.left + px(20), box.top + px(96), box.right - px(16), box.bottom - px(16) };
        drawText(dc, body, note, m_bodyFont, RGB(89, 89, 89), DT_LEFT | DT_WORDBREAK);
    }

    /** @brief 托盘图标。窗口内容在 WM_PAINT 里画 */
    void onCreate(HWND hwnd)
    {
        ensureFonts();
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(m_icon));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(m_icon));

        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = kTray;
        nid.hIcon = m_icon;
        wcscpy_s(nid.szTip, L"中继");
        Shell_NotifyIconW(NIM_ADD, &nid);
        m_nid = nid;
    }

    /** @brief 单击托盘打开窗口，右键弹出菜单 */
    void onTray(LPARAM lparam)
    {
        if (lparam == WM_LBUTTONUP || lparam == WM_LBUTTONDBLCLK) {
            ShowWindow(m_hwnd, SW_SHOW);
            SetForegroundWindow(m_hwnd);
            return;
        }
        if (lparam != WM_RBUTTONUP) {
            return;
        }
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 1, L"打开");
        AppendMenuW(menu, MF_STRING, 2, L"退出");
        POINT point;
        GetCursorPos(&point);
        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, m_hwnd, nullptr);
        DestroyMenu(menu);
    }

    /** @brief 去掉托盘图标并销毁窗口，消息循环随后结束 */
    void quit()
    {
        if (m_quit) {
            return;
        }
        m_quit = true;
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        DestroyWindow(m_hwnd);
    }

    /** @brief 窗口销毁前丢掉字体 */
    void releaseFonts()
    {
        HFONT* fonts[] = { &m_titleFont, &m_subFont, &m_labelFont, &m_valueFont, &m_bodyFont };
        for (int i = 0; i < 5; ++i) {
            if (*fonts[i]) {
                DeleteObject(*fonts[i]);
                *fonts[i] = nullptr;
            }
        }
    }

    HWND m_hwnd;
    NOTIFYICONDATAW m_nid;
    HICON m_icon;
    bool m_quit;
    int m_port;
    int m_wsPort;
    int m_dpi;
    HFONT m_titleFont;
    HFONT m_subFont;
    HFONT m_labelFont;
    HFONT m_valueFont;
    HFONT m_bodyFont;
};

/** @brief 第一个参数是局域网 TCP 端口，第二个是识别码 WebSocket 端口 */
int main(int argc, char** argv)
{
    int port = 19200;
    int wsPort = 19280;
    if (argc >= 2) {
        port = atoi(argv[1]);
    }
    if (argc >= 3) {
        wsPort = atoi(argv[2]);
    }
    if (port < 1 || port > 65535) {
        port = 19200;
    }
    if (wsPort < 1 || wsPort > 65535) {
        wsPort = 19280;
    }
    SetLastError(ERROR_SUCCESS);
    HANDLE single = CreateMutexW(nullptr, TRUE, L"Local\\VisionRemoteRelay");
    if (!single || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (single) {
            CloseHandle(single);
        }
        RelayWindow::activateExisting();
        return 0;
    }
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        CloseHandle(single);
        return 1;
    }
    RelayHub hub;
    if (!hub.start(port)) {
        MessageBoxW(nullptr, L"局域网端口绑定失败。", L"中继", MB_ICONERROR);
        WSACleanup();
        CloseHandle(single);
        return 1;
    }
    if (!hub.startWs(wsPort)) {
        MessageBoxW(nullptr, L"识别码端口绑定失败，局域网中继仍可用。", L"中继", MB_ICONWARNING);
    }
    RelayWindow window;
    if (!window.open(port, wsPort)) {
        hub.stop();
        WSACleanup();
        CloseHandle(single);
        return 1;
    }
    const int code = window.loop();
    hub.stop();
    WSACleanup();
    CloseHandle(single);
    return code;
}

