/**
 * @file WebSocketClient.cpp
 * @brief WebSocketClient实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "WebSocketClient.h"
#include "NetUrl.h"
#include "TextEncoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

namespace Vision {

WebSocketClient::WebSocketClient()
    : m_session(0)
    , m_connect(0)
    , m_socket(0)
    , m_timeoutMs(10000)
{
}

WebSocketClient::~WebSocketClient()
{
    close();
}

CommTransport WebSocketClient::transport() const
{
    return TransportWebSocket;
}

bool WebSocketClient::open(const ParamMap& params, std::string* err)
{
    close();
    const std::string url = params.value("url").toString();
    const int timeoutMs = params.value("timeoutMs").toInt(10000);
    const std::string token = params.value("token").toString();
    std::string scheme;
    std::string host;
    std::string path;
    int port = 0;
    if (!NetUrl::parse(url, &scheme, &host, &port, &path, err)) {
        return false;
    }
    if (scheme != "ws" && scheme != "wss") {
        setCommErr(err, "WebSocket 只接受 ws 或 wss");
        return false;
    }
    // WinHTTP 会话、连接、请求逐级建立；任一步失败都要把已开句柄关掉
    HINTERNET session = WinHttpOpen(L"VisionPlatform/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        setCommErr(err, "WebSocket 会话打开失败");
        return false;
    }
    const int t = timeoutMs < 0 ? 0 : timeoutMs;
    WinHttpSetTimeouts(session, t, t, t, t);
    HINTERNET connect = WinHttpConnect(session, utf8ToWide(host).c_str(), static_cast<INTERNET_PORT>(port), 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        setCommErr(err, "WebSocket 连接失败");
        return false;
    }
    const DWORD flags = scheme == "wss" ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(connect, L"GET", utf8ToWide(path).c_str(), 0, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        setCommErr(err, "WebSocket 打开请求失败");
        return false;
    }
    if (!WinHttpSetOption(req, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, 0, 0)) {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        setCommErr(err, "WebSocket 升级选项失败");
        return false;
    }
    if (!token.empty()) {
        const std::wstring header = L"Authorization: Bearer " + utf8ToWide(token);
        WinHttpAddRequestHeaders(req, header.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    }
    if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(req, 0)) {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        setCommErr(err, "WebSocket 握手失败");
        return false;
    }
    // 握手完成后请求句柄即可关闭，保留会话、连接和 WebSocket 句柄
    HINTERNET socket = WinHttpWebSocketCompleteUpgrade(req, 0);
    WinHttpCloseHandle(req);
    if (!socket) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        setCommErr(err, "WebSocket 升级失败");
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    m_session = session;
    m_connect = connect;
    m_socket = socket;
    m_url = url;
    m_timeoutMs = t;
    return true;
}

void WebSocketClient::close()
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_socket) {
        WinHttpWebSocketClose(static_cast<HINTERNET>(m_socket), WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, 0, 0);
        WinHttpCloseHandle(static_cast<HINTERNET>(m_socket));
        m_socket = 0;
    }
    if (m_connect) {
        WinHttpCloseHandle(static_cast<HINTERNET>(m_connect));
        m_connect = 0;
    }
    if (m_session) {
        WinHttpCloseHandle(static_cast<HINTERNET>(m_session));
        m_session = 0;
    }
    m_url.clear();
}

bool WebSocketClient::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_socket != 0;
}

std::vector<CommPeer> WebSocketClient::peers() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    std::vector<CommPeer> out;
    if (m_socket) {
        CommPeer peer;
        peer.remote = m_url;
        out.push_back(peer);
    }
    return out;
}

bool WebSocketClient::write(const std::string&, const CommBytes&, int, std::string* err)
{
    setCommErr(err, "WebSocket 通道不按字节写");
    return false;
}

CommBytes WebSocketClient::read(const std::string&, int, int, std::string* err)
{
    setCommErr(err, "WebSocket 通道不按字节读");
    return CommBytes();
}

void WebSocketClient::drain(const std::string&)
{
}

std::string WebSocketClient::waitIncoming(int)
{
    return std::string();
}

bool WebSocketClient::send(const CommBytes& data, bool text, std::string* err)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (!m_socket) {
        setCommErr(err, "通道未打开");
        return false;
    }
    const DWORD rc = WinHttpWebSocketSend(static_cast<HINTERNET>(m_socket),
                                           text ? WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE
                                                : WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,
                                           data.empty() ? 0 : const_cast<unsigned char*>(data.data()),
                                           static_cast<DWORD>(data.size()));
    if (rc != NO_ERROR) {
        setCommErr(err, "WebSocket 发送失败");
        return false;
    }
    return true;
}

CommBytes WebSocketClient::recv(int timeoutMs, bool* text, std::string* err)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (!m_socket) {
        setCommErr(err, "通道未打开");
        return CommBytes();
    }
    int t = timeoutMs < 0 ? m_timeoutMs : timeoutMs;
    if (t < 0) {
        t = 0;
    }
    WinHttpSetOption(static_cast<HINTERNET>(m_socket), WINHTTP_OPTION_RECEIVE_TIMEOUT, &t, sizeof(t));
    CommBytes out;
    while (true) {
        unsigned char buf[4096];
        DWORD n = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE type = WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
        const DWORD rc = WinHttpWebSocketReceive(static_cast<HINTERNET>(m_socket), buf, sizeof(buf), &n, &type);
        if (rc == ERROR_WINHTTP_TIMEOUT) {
            setCommErr(err, "读超时");
            return CommBytes();
        }
        if (rc != NO_ERROR) {
            setCommErr(err, "WebSocket 接收失败");
            return CommBytes();
        }
        out.insert(out.end(), buf, buf + n);
        if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
            if (text) {
                *text = true;
            }
            return out;
        }
        if (type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
            if (text) {
                *text = false;
            }
            return out;
        }
        if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            setCommErr(err, "对端已关闭");
            return CommBytes();
        }
    }
}

}
