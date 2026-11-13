/**
 * @file AccessTunnel.cpp
 * @brief WebSocket 客户端。外层 TLS 只到域名，里面的字节仍交给原来的会话 TLS
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "AccessTunnel.h"
#include "RemoteIdentity.h"
#include "TlsPipe.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace Vision {

namespace {

const char* kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

struct Pipe {
    TlsPipe tls;
    SOCKET cloud;
    SOCKET local;
    std::atomic<bool> stop;
    std::vector<unsigned char> pending;
    Pipe() : cloud(INVALID_SOCKET), local(INVALID_SOCKET), stop(false) {}
};

std::string b64(const unsigned char* data, int len)
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

bool sha1(const std::string& text, unsigned char dig[20])
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

bool sendAll(SOCKET sock, const char* data, int len)
{
    int sent = 0;
    while (sent < len) {
        const int n = send(sock, data + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    return true;
}

void sendFrame(Pipe* pipe, int opcode, const unsigned char* data, int len, std::string* err)
{
    unsigned char mask[4];
    if (BCryptGenRandom(0, mask, 4, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        mask[0] = 1;
        mask[1] = 2;
        mask[2] = 3;
        mask[3] = 4;
    }
    std::vector<unsigned char> frame;
    frame.push_back(static_cast<unsigned char>(0x80 | opcode));
    if (len < 126) {
        frame.push_back(static_cast<unsigned char>(0x80 | len));
    } else if (len <= 65535) {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<unsigned char>((len >> 8) & 0xff));
        frame.push_back(static_cast<unsigned char>(len & 0xff));
    } else {
        frame.push_back(0x80 | 127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<unsigned char>((static_cast<unsigned long long>(len) >> (i * 8)) & 0xff));
        }
    }
    frame.insert(frame.end(), mask, mask + 4);
    for (int i = 0; i < len; ++i) {
        frame.push_back(data[i] ^ mask[i % 4]);
    }
    pipe->tls.write(frame, 3000, err);
}

bool takeFrame(Pipe* pipe, int* opcode, std::vector<unsigned char>* payload, std::string* err)
{
    while (!pipe->stop) {
        if (pipe->pending.size() < 2) {
            const CommBytes more = pipe->tls.read(4096, 30, err);
            if (err && !err->empty()) {
                return false;
            }
            if (more.empty()) {
                return false;
            }
            pipe->pending.insert(pipe->pending.end(), more.begin(), more.end());
            continue;
        }
        const int op = pipe->pending[0] & 0x0f;
        const bool masked = (pipe->pending[1] & 0x80) != 0;
        unsigned long long len = pipe->pending[1] & 0x7f;
        size_t head = 2;
        if (len == 126) {
            head = 4;
        } else if (len == 127) {
            head = 10;
        }
        if (masked) {
            head += 4;
        }
        if (pipe->pending.size() < head) {
            const CommBytes more = pipe->tls.read(4096, 30, err);
            if (err && !err->empty()) {
                return false;
            }
            if (more.empty()) {
                return false;
            }
            pipe->pending.insert(pipe->pending.end(), more.begin(), more.end());
            continue;
        }
        if ((pipe->pending[1] & 0x7f) == 126) {
            len = (static_cast<unsigned long long>(pipe->pending[2]) << 8) | pipe->pending[3];
        } else if ((pipe->pending[1] & 0x7f) == 127) {
            len = 0;
            for (int i = 0; i < 8; ++i) {
                len = (len << 8) | pipe->pending[2 + i];
            }
        }
        if (len > 8 * 1024 * 1024) {
            if (err) {
                *err = "隧道帧过大";
            }
            return false;
        }
        if (pipe->pending.size() < head + static_cast<size_t>(len)) {
            const CommBytes more = pipe->tls.read(65536, 30, err);
            if (err && !err->empty()) {
                return false;
            }
            if (more.empty()) {
                return false;
            }
            pipe->pending.insert(pipe->pending.end(), more.begin(), more.end());
            continue;
        }
        payload->assign(pipe->pending.begin() + static_cast<std::ptrdiff_t>(head),
            pipe->pending.begin() + static_cast<std::ptrdiff_t>(head + static_cast<size_t>(len)));
        if (masked) {
            const unsigned char* key = pipe->pending.data() + head - 4;
            for (size_t i = 0; i < payload->size(); ++i) {
                (*payload)[i] = static_cast<unsigned char>((*payload)[i] ^ key[i % 4]);
            }
        }
        pipe->pending.erase(pipe->pending.begin(), pipe->pending.begin() + static_cast<std::ptrdiff_t>(head + static_cast<size_t>(len)));
        *opcode = op;
        return true;
    }
    return false;
}

std::vector<unsigned char> clientFrame(int opcode, const unsigned char* data, int len)
{
    unsigned char mask[4];
    if (BCryptGenRandom(0, mask, 4, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        mask[0] = 1;
        mask[1] = 2;
        mask[2] = 3;
        mask[3] = 4;
    }
    std::vector<unsigned char> frame;
    frame.push_back(static_cast<unsigned char>(0x80 | opcode));
    if (len < 126) {
        frame.push_back(static_cast<unsigned char>(0x80 | len));
    } else if (len <= 65535) {
        frame.push_back(static_cast<unsigned char>(0x80 | 126));
        frame.push_back(static_cast<unsigned char>((len >> 8) & 0xff));
        frame.push_back(static_cast<unsigned char>(len & 0xff));
    } else {
        frame.push_back(static_cast<unsigned char>(0x80 | 127));
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<unsigned char>((static_cast<unsigned long long>(len) >> (i * 8)) & 0xff));
        }
    }
    frame.insert(frame.end(), mask, mask + 4);
    for (int i = 0; i < len; ++i) {
        frame.push_back(data[i] ^ mask[i % 4]);
    }
    return frame;
}

bool wouldBlock()
{
    const int wsa = WSAGetLastError();
    return wsa == WSAEWOULDBLOCK || wsa == WSAETIMEDOUT;
}

void pump(const std::shared_ptr<Pipe>& pipe)
{
    u_long nb = 1;
    ioctlsocket(pipe->cloud, FIONBIO, &nb);
    ioctlsocket(pipe->local, FIONBIO, &nb);
    std::vector<unsigned char> outCipher;
    std::string toLocal;
    std::string wsIn;
    int idle = 0;
    while (!pipe->stop) {
        bool worked = false;
        char buf[16384];
        const int n = recv(pipe->local, buf, sizeof(buf), 0);
        if (n > 0) {
            const std::vector<unsigned char> frame = clientFrame(2, reinterpret_cast<unsigned char*>(buf), n);
            std::string err;
            if (!pipe->tls.seal(frame.data(), frame.size(), &outCipher, &err)) {
                break;
            }
            worked = true;
        } else if (n == 0) {
            break;
        } else if (!wouldBlock()) {
            break;
        }
        while (!outCipher.empty()) {
            const int sent = send(pipe->cloud, reinterpret_cast<const char*>(outCipher.data()), static_cast<int>(outCipher.size()), 0);
            if (sent > 0) {
                outCipher.erase(outCipher.begin(), outCipher.begin() + sent);
                worked = true;
                continue;
            }
            if (sent == 0 || !wouldBlock()) {
                pipe->stop = true;
            }
            break;
        }
        if (pipe->stop) {
            break;
        }
        const int got = recv(pipe->cloud, buf, sizeof(buf), 0);
        if (got > 0) {
            std::string err;
            const CommBytes plain = pipe->tls.unseal(reinterpret_cast<unsigned char*>(buf), static_cast<size_t>(got), &err);
            if (!err.empty()) {
                break;
            }
            wsIn.append(reinterpret_cast<const char*>(plain.data()), plain.size());
            worked = true;
        } else if (got == 0) {
            break;
        } else if (!wouldBlock()) {
            break;
        }
        while (wsIn.size() >= 2) {
            const unsigned char b1 = static_cast<unsigned char>(wsIn[1]);
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
            if (wsIn.size() < head) {
                break;
            }
            if ((b1 & 0x7f) == 126) {
                len = (static_cast<unsigned long long>(static_cast<unsigned char>(wsIn[2])) << 8)
                    | static_cast<unsigned char>(wsIn[3]);
            } else if ((b1 & 0x7f) == 127) {
                len = 0;
                for (int i = 0; i < 8; ++i) {
                    len = (len << 8) | static_cast<unsigned char>(wsIn[2 + i]);
                }
            }
            if (len > 8 * 1024 * 1024 || wsIn.size() < head + static_cast<size_t>(len)) {
                if (len > 8 * 1024 * 1024) {
                    pipe->stop = true;
                }
                break;
            }
            std::string body = wsIn.substr(head, static_cast<size_t>(len));
            if (masked) {
                const unsigned char* key = reinterpret_cast<const unsigned char*>(wsIn.data() + head - 4);
                for (size_t i = 0; i < body.size(); ++i) {
                    body[i] = static_cast<char>(static_cast<unsigned char>(body[i]) ^ key[i % 4]);
                }
            }
            const int opcode = static_cast<unsigned char>(wsIn[0]) & 0x0f;
            wsIn.erase(0, head + static_cast<size_t>(len));
            if (opcode == 0x8) {
                pipe->stop = true;
                break;
            }
            if (opcode == 0x9) {
                const std::vector<unsigned char> pong = clientFrame(0xA,
                    reinterpret_cast<const unsigned char*>(body.data()), static_cast<int>(body.size()));
                std::string err;
                if (!pipe->tls.seal(pong.data(), pong.size(), &outCipher, &err)) {
                    pipe->stop = true;
                    break;
                }
                continue;
            }
            if (opcode == 0x1 || opcode == 0x2 || opcode == 0x0) {
                toLocal.append(body);
            }
        }
        while (!toLocal.empty()) {
            const int sent = send(pipe->local, toLocal.data(), static_cast<int>(toLocal.size()), 0);
            if (sent > 0) {
                toLocal.erase(0, static_cast<size_t>(sent));
                worked = true;
                continue;
            }
            if (sent == 0 || !wouldBlock()) {
                pipe->stop = true;
            }
            break;
        }
        if (pipe->stop) {
            break;
        }
        if (worked) {
            idle = 0;
            continue;
        }
        idle += 1;
        if (idle > 400) {
            const unsigned char ping = 0;
            const std::vector<unsigned char> frame = clientFrame(0x9, &ping, 0);
            std::string err;
            if (!pipe->tls.seal(frame.data(), frame.size(), &outCipher, &err)) {
                break;
            }
            idle = 0;
            continue;
        }
        Sleep(5);
    }
    pipe->stop = true;
    if (pipe->local != INVALID_SOCKET) {
        shutdown(pipe->local, SD_BOTH);
        closesocket(pipe->local);
        pipe->local = INVALID_SOCKET;
    }
    if (pipe->cloud != INVALID_SOCKET) {
        closesocket(pipe->cloud);
        pipe->cloud = INVALID_SOCKET;
    }
    pipe->tls.reset();
}

SOCKET tcpConnect(const std::string& host, int port, std::string* err)
{
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        if (err) {
            *err = "网络未初始化";
        }
        return INVALID_SOCKET;
    }
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* info = 0;
    char portText[16];
    sprintf_s(portText, "%d", port);
    if (getaddrinfo(host.c_str(), portText, &hints, &info) != 0 || !info) {
        if (err) {
            *err = "域名解析失败";
        }
        return INVALID_SOCKET;
    }
    SOCKET sock = INVALID_SOCKET;
    for (addrinfo* it = info; it; it = it->ai_next) {
        sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }
        u_long nb = 1;
        ioctlsocket(sock, FIONBIO, &nb);
        connect(sock, it->ai_addr, static_cast<int>(it->ai_addrlen));
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sock, &set);
        timeval wait;
        wait.tv_sec = 5;
        wait.tv_usec = 0;
        if (select(0, 0, &set, 0, &wait) > 0) {
            nb = 0;
            ioctlsocket(sock, FIONBIO, &nb);
            break;
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    freeaddrinfo(info);
    if (sock == INVALID_SOCKET && err) {
        *err = "连不上识别码域名";
    }
    return sock;
}

}

const char* AccessTunnel::token()
{
    return "vision-access";
}

const char* AccessTunnel::defaultHost()
{
    return "remote.subapi.fun";
}

bool AccessTunnel::open(const std::string& host, int port, const RemoteIdentity& identity,
    uintptr_t* localSock, std::string* err)
{
    if (!localSock || host.empty()) {
        if (err) {
            *err = "识别码域名是空的";
        }
        return false;
    }
    if (!identity.ready()) {
        if (err) {
            *err = "本机证书未就绪";
        }
        return false;
    }
    const SOCKET cloud = tcpConnect(host, port > 0 ? port : 443, err);
    if (cloud == INVALID_SOCKET) {
        return false;
    }
    std::shared_ptr<Pipe> pipe(new Pipe());
    pipe->cloud = cloud;
    std::wstring wide(host.begin(), host.end());
    if (!pipe->tls.handshake(static_cast<uintptr_t>(cloud), false, identity, 0, err, wide.c_str())) {
        closesocket(cloud);
        return false;
    }
    unsigned char rnd[16];
    if (BCryptGenRandom(0, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        for (int i = 0; i < 16; ++i) {
            rnd[i] = static_cast<unsigned char>(i * 17);
        }
    }
    const std::string key = b64(rnd, 16);
    const std::string req = "GET /vision HTTP/1.1\r\nHost: " + host
        + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key
        + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    if (!pipe->tls.write(CommBytes(req.begin(), req.end()), 5000, err)) {
        closesocket(cloud);
        return false;
    }
    std::string resp;
    for (int i = 0; i < 40 && resp.find("\r\n\r\n") == std::string::npos; ++i) {
        const CommBytes chunk = pipe->tls.read(2048, 500, err);
        if (err && !err->empty()) {
            closesocket(cloud);
            return false;
        }
        resp.append(chunk.begin(), chunk.end());
    }
    if (resp.find("101") == std::string::npos) {
        closesocket(cloud);
        if (err) {
            *err = "域名没有升成识别码通道";
        }
        return false;
    }
    unsigned char dig[20];
    if (!sha1(key + kGuid, dig)) {
        closesocket(cloud);
        if (err) {
            *err = "校验隧道失败";
        }
        return false;
    }
    const std::string accept = b64(dig, 20);
    if (resp.find(accept) == std::string::npos) {
        closesocket(cloud);
        if (err) {
            *err = "隧道应答对不上";
        }
        return false;
    }
    const size_t headerEnd = resp.find("\r\n\r\n");
    if (headerEnd != std::string::npos && headerEnd + 4 < resp.size()) {
        pipe->pending.assign(resp.begin() + static_cast<std::ptrdiff_t>(headerEnd + 4), resp.end());
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (listener == INVALID_SOCKET || bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0
        || listen(listener, 1) != 0) {
        if (listener != INVALID_SOCKET) {
            closesocket(listener);
        }
        closesocket(cloud);
        if (err) {
            *err = "本机通道创建失败";
        }
        return false;
    }
    int len = sizeof(addr);
    getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &len);
    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKET accepted = INVALID_SOCKET;
    std::thread accepter([&]() { accepted = ::accept(listener, 0, 0); });
    if (connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(listener);
        closesocket(client);
        closesocket(cloud);
        accepter.join();
        if (err) {
            *err = "本机通道连接失败";
        }
        return false;
    }
    accepter.join();
    closesocket(listener);
    if (accepted == INVALID_SOCKET || client == INVALID_SOCKET) {
        if (client != INVALID_SOCKET) {
            closesocket(client);
        }
        closesocket(cloud);
        if (err) {
            *err = "本机通道连接失败";
        }
        return false;
    }
    BOOL nodelay = TRUE;
    setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
    setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
    pipe->local = accepted;
    std::thread(pump, pipe).detach();
    *localSock = static_cast<uintptr_t>(client);
    return true;
}

bool AccessTunnel::writeLine(uintptr_t sock, const std::string& line)
{
    int sent = 0;
    while (sent < static_cast<int>(line.size())) {
        const int n = send(static_cast<SOCKET>(sock), line.data() + sent, static_cast<int>(line.size()) - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    return true;
}

bool AccessTunnel::readLine(uintptr_t sock, std::string* line, int waitMs)
{
    if (!line) {
        return false;
    }
    line->clear();
    const DWORD began = GetTickCount();
    while (line->find('\n') == std::string::npos && line->size() < 128) {
        const int left = waitMs - static_cast<int>(GetTickCount() - began);
        if (left <= 0) {
            return false;
        }
        int tv = left > 1000 ? 1000 : left;
        setsockopt(static_cast<SOCKET>(sock), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        char ch = 0;
        const int n = recv(static_cast<SOCKET>(sock), &ch, 1, 0);
        if (n == 0) {
            return false;
        }
        if (n < 0) {
            const int wsa = WSAGetLastError();
            if (wsa == WSAETIMEDOUT || wsa == WSAEWOULDBLOCK) {
                continue;
            }
            return false;
        }
        line->push_back(ch);
    }
    return line->find('\n') != std::string::npos;
}

void AccessTunnel::discard(uintptr_t sock)
{
    if (sock != 0) {
        closesocket(static_cast<SOCKET>(sock));
    }
}

bool AccessTunnel::alive(uintptr_t sock)
{
    if (sock == 0) {
        return false;
    }
    u_long nb = 1;
    ioctlsocket(static_cast<SOCKET>(sock), FIONBIO, &nb);
    char ch = 0;
    const int n = recv(static_cast<SOCKET>(sock), &ch, 1, MSG_PEEK);
    const int wsa = WSAGetLastError();
    nb = 0;
    ioctlsocket(static_cast<SOCKET>(sock), FIONBIO, &nb);
    if (n == 0) {
        return false;
    }
    if (n < 0 && wsa != WSAEWOULDBLOCK && wsa != WSAETIMEDOUT) {
        return false;
    }
    return true;
}

}
