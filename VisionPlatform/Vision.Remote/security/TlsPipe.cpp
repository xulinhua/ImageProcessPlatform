/**
 * @file TlsPipe.cpp
 * @brief Schannel TLS 管道
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TlsPipe.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#define SECURITY_WIN32
#include <schannel.h>
#include <security.h>

#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdio>

#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

#ifndef SP_PROT_TLS1_3
#define SP_PROT_TLS1_3 0x00002000
#endif
#ifndef SP_PROT_TLS1_3_SERVER
#define SP_PROT_TLS1_3_SERVER 0x00001000
#endif
#ifndef SP_PROT_TLS1_3_CLIENT
#define SP_PROT_TLS1_3_CLIENT 0x00002000
#endif

#include <shlobj.h>

namespace Vision {

namespace {

void logTls(const char* msg)
{
    wchar_t dir[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA, 0, 0, dir))) {
        return;
    }
    std::wstring path = dir;
    path += L"\\Vision.Remote\\";
    CreateDirectoryW(path.c_str(), 0);
    path += L"tls.log";
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char line[512];
    const int n = sprintf_s(line, "%04u-%02u-%02u %02u:%02u:%02u [%u] %s\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        GetCurrentProcessId(), msg ? msg : "");
    DWORD written = 0;
    if (n > 0) {
        WriteFile(file, line, static_cast<DWORD>(n), &written, 0);
    }
    CloseHandle(file);
}

}



struct TlsPipe::Impl {
    Impl()
        : sock(INVALID_SOCKET)
        , ready(false)
        , asServer(false)
    {
        SecInvalidateHandle(&cred);
        SecInvalidateHandle(&ctx);
    }

    SOCKET sock;
    CredHandle cred;
    CtxtHandle ctx;
    bool ready;
    bool asServer;
    std::string peerFp;
    std::vector<unsigned char> leftover;    ///< 解密后尚未交给上层的明文
    std::vector<unsigned char> extra;       ///< 握手后多余密文
    SecPkgContext_StreamSizes sizes;
};

TlsPipe::TlsPipe()
    : m_impl(new Impl())
{
}

TlsPipe::~TlsPipe()
{
    reset();
    delete m_impl;
}

bool TlsPipe::ready() const
{
    return m_impl->ready;
}

const std::string& TlsPipe::peerFingerprint() const
{
    return m_impl->peerFp;
}

void TlsPipe::reset()
{
    if (SecIsValidHandle(&m_impl->ctx)) {
        DeleteSecurityContext(&m_impl->ctx);
        SecInvalidateHandle(&m_impl->ctx);
    }
    if (SecIsValidHandle(&m_impl->cred)) {
        FreeCredentialsHandle(&m_impl->cred);
        SecInvalidateHandle(&m_impl->cred);
    }
    m_impl->ready = false;
    m_impl->sock = INVALID_SOCKET;
    m_impl->peerFp.clear();
    m_impl->leftover.clear();
    m_impl->extra.clear();
    memset(&m_impl->sizes, 0, sizeof(m_impl->sizes));
}

namespace {

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

bool recvSome(SOCKET sock, std::vector<unsigned char>* buf, int timeoutMs, std::string* err)
{
    DWORD tv = timeoutMs <= 0 ? 5000 : static_cast<DWORD>(timeoutMs);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    char tmp[16 * 1024];
    const int n = recv(sock, tmp, sizeof(tmp), 0);
    if (n == 0) {
        if (err) {
            *err = "对端关闭";
        }
        return false;
    }
    if (n < 0) {
        const int wsa = WSAGetLastError();
        if (wsa == WSAETIMEDOUT || wsa == WSAEWOULDBLOCK) {
            if (err) {
                *err = "超时";
            }
            return false;
        }
        if (err) {
            *err = "接收失败";
        }
        return false;
    }
    buf->insert(buf->end(), tmp, tmp + n);
    return true;
}

}

bool TlsPipe::handshake(uintptr_t sock, bool asServer, const RemoteIdentity& identity,
                        std::string* peerFingerprintOut, std::string* err)
{
    reset();
    if (!identity.ready() || !identity.certificate()) {
        if (err) {
            *err = "本机证书未就绪";
        }
        return false;
    }
    m_impl->sock = static_cast<SOCKET>(sock);
    m_impl->asServer = asServer;

    SCHANNEL_CRED sch = {};
    sch.dwVersion = SCHANNEL_CRED_VERSION;
    PCCERT_CONTEXT certs[1] = { identity.certificate() };
    sch.cCreds = 1;
    sch.paCred = certs;
    // 自签互认：仅 TLS1.2，手动校验对端证书
    sch.grbitEnabledProtocols = SP_PROT_TLS1_2 | SP_PROT_TLS1_2_SERVER | SP_PROT_TLS1_2_CLIENT;
    // 不用 SCH_USE_STRONG_CRYPTO：自签 RSA 在部分机器上会直接 SEC_E_NO_CREDENTIALS
    sch.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;
#ifndef SCH_CRED_NO_SERVERNAME_CHECK
#define SCH_CRED_NO_SERVERNAME_CHECK 0x00000004
#endif
    if (!asServer) {
        sch.dwFlags |= SCH_CRED_NO_SERVERNAME_CHECK;
    }

    TimeStamp life = {};
    SECURITY_STATUS st = AcquireCredentialsHandleW(
        nullptr,
        const_cast<SEC_WCHAR*>(UNISP_NAME_W),
        asServer ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND,
        nullptr,
        &sch,
        nullptr,
        nullptr,
        &m_impl->cred,
        &life);
    if (st != SEC_E_OK) {
        char buf[80];
        sprintf_s(buf, "AcquireCredentialsHandle 失败 (0x%08lX) server=%d", static_cast<unsigned long>(st), asServer ? 1 : 0);
        logTls(buf);
        if (err) {
            *err = buf;
        }
        return false;
    }
    logTls(asServer ? "cred ok server" : "cred ok client");

    // 自签场景先不做强制双向证书认证（MUTUAL_AUTH 易在 Schannel 上直接断连）；
    // 服务端证书仍会下发，客户端可取指纹；控制端指纹走 HelloAck
    const DWORD clientFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY
        | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_USE_SUPPLIED_CREDS;
    const DWORD serverFlags = ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY
        | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM;

    std::vector<unsigned char> inBuf;
    DWORD ctxAttr = 0;
    TimeStamp ctxLife = {};
    bool haveCtx = false;

    for (;;) {
        SecBuffer outBuffers[1] = {};
        outBuffers[0].BufferType = SECBUFFER_TOKEN;
        SecBufferDesc outDesc = {};
        outDesc.ulVersion = SECBUFFER_VERSION;
        outDesc.cBuffers = 1;
        outDesc.pBuffers = outBuffers;

        SecBuffer inBuffers[2] = {};
        SecBufferDesc inDesc = {};
        inDesc.ulVersion = SECBUFFER_VERSION;
        inDesc.cBuffers = 2;
        inDesc.pBuffers = inBuffers;
        const bool haveInput = !inBuf.empty();
        if (haveInput) {
            inBuffers[0].pvBuffer = inBuf.data();
            inBuffers[0].cbBuffer = static_cast<ULONG>(inBuf.size());
            inBuffers[0].BufferType = SECBUFFER_TOKEN;
            inBuffers[1].BufferType = SECBUFFER_EMPTY;
        }

        PCtxtHandle phContext = haveCtx ? &m_impl->ctx : nullptr;
        PCtxtHandle phNewContext = &m_impl->ctx;
        PSecBufferDesc pInput = haveInput ? &inDesc : nullptr;

        if (!asServer) {
            st = InitializeSecurityContextW(
                &m_impl->cred,
                phContext,
                nullptr,
                clientFlags,
                0,
                0,
                pInput,
                0,
                phNewContext,
                &outDesc,
                &ctxAttr,
                &ctxLife);
        } else {
            if (!haveInput) {
                std::string local;
                if (!recvSome(m_impl->sock, &inBuf, 10000, &local)) {
                    if (err) {
                        *err = local.empty() ? "TLS 握手收包失败" : local;
                    }
                    reset();
                    return false;
                }
                inBuffers[0].pvBuffer = inBuf.data();
                inBuffers[0].cbBuffer = static_cast<ULONG>(inBuf.size());
                inBuffers[0].BufferType = SECBUFFER_TOKEN;
                inBuffers[1].BufferType = SECBUFFER_EMPTY;
                pInput = &inDesc;
            }
            st = AcceptSecurityContext(
                &m_impl->cred,
                phContext,
                pInput,
                serverFlags,
                0,
                phNewContext,
                &outDesc,
                &ctxAttr,
                &ctxLife);
        }
        haveCtx = true;

        if (outBuffers[0].cbBuffer != 0 && outBuffers[0].pvBuffer != nullptr) {
            const bool sent = sendAll(m_impl->sock, static_cast<const char*>(outBuffers[0].pvBuffer),
                static_cast<int>(outBuffers[0].cbBuffer));
            FreeContextBuffer(outBuffers[0].pvBuffer);
            outBuffers[0].pvBuffer = nullptr;
            if (!sent) {
                if (err) {
                    *err = "TLS 握手发包失败";
                }
                reset();
                return false;
            }
        }

        // 处理输入缓冲里的 EXTRA
        if (haveInput && inBuffers[1].BufferType == SECBUFFER_EXTRA && inBuffers[1].cbBuffer > 0
            && inBuffers[1].cbBuffer <= inBuf.size()) {
            const size_t keep = inBuffers[1].cbBuffer;
            const size_t drop = inBuf.size() - keep;
            inBuf.erase(inBuf.begin(), inBuf.begin() + static_cast<std::ptrdiff_t>(drop));
        } else if (st != SEC_E_INCOMPLETE_MESSAGE) {
            inBuf.clear();
        }

        if (st == SEC_E_OK) {
            break;
        }
        if (st == SEC_I_CONTINUE_NEEDED || st == SEC_E_INCOMPLETE_MESSAGE
            || st == SEC_I_COMPLETE_AND_CONTINUE || st == SEC_I_COMPLETE_NEEDED) {
            if (st == SEC_I_COMPLETE_NEEDED || st == SEC_I_COMPLETE_AND_CONTINUE) {
                CompleteAuthToken(&m_impl->ctx, &outDesc);
            }
            if (st == SEC_E_INCOMPLETE_MESSAGE || inBuf.empty()) {
                std::string local;
                if (!recvSome(m_impl->sock, &inBuf, 10000, &local)) {
                    if (err) {
                        *err = local.empty() ? "TLS 握手收包失败" : local;
                    }
                    reset();
                    return false;
                }
            }
            continue;
        }
        char buf[96];
        sprintf_s(buf, "TLS 握手失败 (0x%08lX) server=%d", static_cast<unsigned long>(st), asServer ? 1 : 0);
        logTls(buf);
        if (err) {
            *err = buf;
        }
        reset();
        return false;
    }
    logTls(asServer ? "handshake ok server" : "handshake ok client");

    if (QueryContextAttributes(&m_impl->ctx, SECPKG_ATTR_STREAM_SIZES, &m_impl->sizes) != SEC_E_OK
        || m_impl->sizes.cbMaximumMessage == 0) {
        if (err) {
            *err = "查询 TLS 流大小失败";
        }
        reset();
        return false;
    }

    PCCERT_CONTEXT remote = nullptr;
    if (QueryContextAttributes(&m_impl->ctx, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &remote) == SEC_E_OK && remote) {
        m_impl->peerFp = RemoteIdentity::fingerprintOfCert(remote);
        CertFreeCertificateContext(remote);
    }
    if (peerFingerprintOut) {
        *peerFingerprintOut = m_impl->peerFp;
    }
    m_impl->ready = true;
    return true;
}

bool TlsPipe::write(const CommBytes& data, int timeoutMs, std::string* err)
{
    if (!m_impl->ready) {
        if (err) {
            *err = "TLS 未就绪";
        }
        return false;
    }
    DWORD tv = timeoutMs <= 0 ? 3000 : static_cast<DWORD>(timeoutMs);
    setsockopt(m_impl->sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

    size_t off = 0;
    while (off < data.size()) {
        const size_t chunk = (std::min)(data.size() - off, static_cast<size_t>(m_impl->sizes.cbMaximumMessage));
        std::vector<unsigned char> buf(m_impl->sizes.cbHeader + chunk + m_impl->sizes.cbTrailer);
        memcpy(buf.data() + m_impl->sizes.cbHeader, &data[off], chunk);

        SecBuffer buffers[4];
        buffers[0].pvBuffer = buf.data();
        buffers[0].cbBuffer = m_impl->sizes.cbHeader;
        buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
        buffers[1].pvBuffer = buf.data() + m_impl->sizes.cbHeader;
        buffers[1].cbBuffer = static_cast<ULONG>(chunk);
        buffers[1].BufferType = SECBUFFER_DATA;
        buffers[2].pvBuffer = buf.data() + m_impl->sizes.cbHeader + chunk;
        buffers[2].cbBuffer = m_impl->sizes.cbTrailer;
        buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
        buffers[3].pvBuffer = nullptr;
        buffers[3].cbBuffer = 0;
        buffers[3].BufferType = SECBUFFER_EMPTY;

        SecBufferDesc desc;
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers = 4;
        desc.pBuffers = buffers;
        const SECURITY_STATUS st = EncryptMessage(&m_impl->ctx, 0, &desc, 0);
        if (st != SEC_E_OK) {
            if (err) {
                *err = "EncryptMessage 失败";
            }
            return false;
        }
        const int total = static_cast<int>(buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer);
        if (!sendAll(m_impl->sock, reinterpret_cast<const char*>(buf.data()), total)) {
            if (err) {
                *err = "TLS 发送失败";
            }
            return false;
        }
        off += chunk;
    }
    return true;
}

CommBytes TlsPipe::read(int maxBytes, int timeoutMs, std::string* err)
{
    CommBytes out;
    if (!m_impl->ready) {
        return out;
    }
    if (!m_impl->leftover.empty()) {
        const size_t n = (std::min)(m_impl->leftover.size(), static_cast<size_t>(maxBytes > 0 ? maxBytes : 64 * 1024));
        out.assign(m_impl->leftover.begin(), m_impl->leftover.begin() + static_cast<std::ptrdiff_t>(n));
        m_impl->leftover.erase(m_impl->leftover.begin(), m_impl->leftover.begin() + static_cast<std::ptrdiff_t>(n));
        return out;
    }

    std::vector<unsigned char> cipher = m_impl->extra;
    m_impl->extra.clear();
    std::string local;
    if (cipher.empty()) {
        if (!recvSome(m_impl->sock, &cipher, timeoutMs, &local)) {
            if (err && local != "超时") {
                *err = local;
            }
            return out;
        }
    }

    for (;;) {
        SecBuffer buffers[4];
        buffers[0].pvBuffer = cipher.data();
        buffers[0].cbBuffer = static_cast<ULONG>(cipher.size());
        buffers[0].BufferType = SECBUFFER_DATA;
        buffers[1].BufferType = SECBUFFER_EMPTY;
        buffers[2].BufferType = SECBUFFER_EMPTY;
        buffers[3].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc desc;
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers = 4;
        desc.pBuffers = buffers;

        const SECURITY_STATUS st = DecryptMessage(&m_impl->ctx, &desc, 0, nullptr);
        if (st == SEC_E_INCOMPLETE_MESSAGE) {
            if (!recvSome(m_impl->sock, &cipher, timeoutMs <= 0 ? 200 : timeoutMs, &local)) {
                if (err && local != "超时") {
                    *err = local;
                }
                m_impl->extra.swap(cipher);
                return out;
            }
            continue;
        }
        if (st == SEC_I_CONTEXT_EXPIRED) {
            if (err) {
                *err = "TLS 已关闭";
            }
            return out;
        }
        if (st != SEC_E_OK && st != SEC_I_RENEGOTIATE) {
            if (err) {
                *err = "DecryptMessage 失败";
            }
            return out;
        }
        for (int i = 0; i < 4; ++i) {
            if (buffers[i].BufferType == SECBUFFER_DATA && buffers[i].pvBuffer && buffers[i].cbBuffer > 0) {
                const unsigned char* p = static_cast<const unsigned char*>(buffers[i].pvBuffer);
                out.insert(out.end(), p, p + buffers[i].cbBuffer);
            } else if (buffers[i].BufferType == SECBUFFER_EXTRA && buffers[i].pvBuffer && buffers[i].cbBuffer > 0) {
                const unsigned char* p = static_cast<const unsigned char*>(buffers[i].pvBuffer);
                m_impl->extra.assign(p, p + buffers[i].cbBuffer);
            }
        }
        break;
    }
    if (maxBytes > 0 && out.size() > static_cast<size_t>(maxBytes)) {
        m_impl->leftover.assign(out.begin() + maxBytes, out.end());
        out.resize(static_cast<size_t>(maxBytes));
    }
    return out;
}

}
