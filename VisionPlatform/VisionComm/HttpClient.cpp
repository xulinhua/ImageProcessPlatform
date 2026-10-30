/**
 * @file HttpClient.cpp
 * @brief HttpClient实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HttpClient.h"
#include "NetUrl.h"
#include "TextEncoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

namespace Vision {

HttpClient::HttpClient()
    : m_session(0)
    , m_port(0)
    , m_timeoutMs(10000)
    , m_secure(false)
{
}

HttpClient::~HttpClient()
{
    close();
}

CommTransport HttpClient::transport() const
{
    return TransportHttp;
}

bool HttpClient::open(const ParamMap& params, std::string* err)
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
    if (scheme != "http" && scheme != "https") {
        setCommErr(err, "HTTP 只接受 http 或 https");
        return false;
    }
    HINTERNET session = WinHttpOpen(L"VisionPlatform/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        setCommErr(err, "HTTP 会话打开失败");
        return false;
    }
    const int t = timeoutMs < 0 ? 0 : timeoutMs;
    WinHttpSetTimeouts(session, t, t, t, t);
    std::lock_guard<std::mutex> lock(m_mu);
    m_session = session;
    m_scheme = scheme;
    m_host = host;
    m_port = port;
    m_path = path;
    m_url = url;
    m_token = token;
    m_timeoutMs = t;
    m_secure = scheme == "https";
    return true;
}

void HttpClient::close()
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_session) {
        WinHttpCloseHandle(static_cast<HINTERNET>(m_session));
        m_session = 0;
    }
    m_url.clear();
    m_host.clear();
    m_path.clear();
    m_token.clear();
}

bool HttpClient::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_session != 0;
}

std::vector<CommPeer> HttpClient::peers() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    std::vector<CommPeer> out;
    if (m_session) {
        CommPeer peer;
        peer.remote = m_url;
        out.push_back(peer);
    }
    return out;
}

bool HttpClient::write(const std::string&, const CommBytes&, int, std::string* err)
{
    setCommErr(err, "HTTP 通道不按字节写");
    return false;
}

CommBytes HttpClient::read(const std::string&, int, int, std::string* err)
{
    setCommErr(err, "HTTP 通道不按字节读");
    return CommBytes();
}

void HttpClient::drain(const std::string&)
{
}

std::string HttpClient::waitIncoming(int)
{
    return std::string();
}

bool HttpClient::exchange(const std::string& method, const std::string& path, const std::string& contentType,
                          const std::string& body, int* status, std::string* response, std::string* err)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (!m_session) {
        setCommErr(err, "通道未打开");
        return false;
    }
    // 每次请求单独建连接；路径为空时用通道参数里的默认路径
    const std::string usePath = path.empty() ? m_path : path;
    const std::wstring whost = utf8ToWide(m_host);
    const std::wstring wpath = utf8ToWide(usePath.empty() ? std::string("/") : usePath);
    const std::wstring wmethod = utf8ToWide(method.empty() ? std::string("GET") : method);
    HINTERNET connect = WinHttpConnect(static_cast<HINTERNET>(m_session), whost.c_str(),
                                       static_cast<INTERNET_PORT>(m_port), 0);
    if (!connect) {
        setCommErr(err, "HTTP 连接失败");
        return false;
    }
    const DWORD flags = m_secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(connect, wmethod.c_str(), wpath.c_str(), 0, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        WinHttpCloseHandle(connect);
        setCommErr(err, "HTTP 打开请求失败");
        return false;
    }
    if (!m_token.empty()) {
        const std::wstring header = L"Authorization: Bearer " + utf8ToWide(m_token);
        WinHttpAddRequestHeaders(req, header.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    }
    if (!contentType.empty()) {
        const std::wstring header = L"Content-Type: " + utf8ToWide(contentType);
        WinHttpAddRequestHeaders(req, header.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    }
    const DWORD bodyLen = static_cast<DWORD>(body.size());
    const BOOL sent = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                          body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
                                          bodyLen, bodyLen, 0);
    if (!sent || !WinHttpReceiveResponse(req, 0)) {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(connect);
        setCommErr(err, "HTTP 请求失败");
        return false;
    }
    // 只要收到应答即返回 true，HTTP 状态码由调用方判断
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (status) {
        *status = static_cast<int>(statusCode);
    }
    std::string resp;
    while (true) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) {
            break;
        }
        std::string chunk(avail, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(req, &chunk[0], avail, &read)) {
            break;
        }
        chunk.resize(read);
        resp.append(chunk);
    }
    if (response) {
        *response = resp;
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(connect);
    return true;
}

}
