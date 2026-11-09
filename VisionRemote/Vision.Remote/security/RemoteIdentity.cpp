/**
 * @file RemoteIdentity.cpp
 * @brief 自签证书生成与 DPAPI 存取
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteIdentity.h"
#include "RemotePaths.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>
#include <bcrypt.h>

#include <vector>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "shell32.lib")

namespace Vision {

namespace {

std::wstring widen(const std::string& s)
{
    if (s.empty()) {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), 0, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &out[0], n);
    return out;
}

std::string toHex(const unsigned char* data, size_t n)
{
    static const char* kHex = "0123456789abcdef";
    std::string out(n * 2, '0');
    for (size_t i = 0; i < n; ++i) {
        out[i * 2] = kHex[(data[i] >> 4) & 0xf];
        out[i * 2 + 1] = kHex[data[i] & 0xf];
    }
    return out;
}

bool writeAll(const std::wstring& path, const unsigned char* data, size_t n)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, data, static_cast<DWORD>(n), &written, 0);
    CloseHandle(file);
    return ok && written == n;
}

bool readAll(const std::wstring& path, std::vector<unsigned char>* out)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    const DWORD size = GetFileSize(file, 0);
    if (size == INVALID_FILE_SIZE || size == 0) {
        CloseHandle(file);
        return false;
    }
    out->resize(size);
    DWORD read = 0;
    const BOOL ok = ReadFile(file, out->data(), size, &read, 0);
    CloseHandle(file);
    return ok && read == size;
}

}

RemoteIdentity::RemoteIdentity()
    : m_store(nullptr)
    , m_cert(nullptr)
{
}

RemoteIdentity::~RemoteIdentity()
{
    clear();
}

void RemoteIdentity::clear()
{
    if (m_cert) {
        CertFreeCertificateContext(m_cert);
        m_cert = nullptr;
    }
    if (m_store) {
        CertCloseStore(static_cast<HCERTSTORE>(m_store), 0);
        m_store = nullptr;
    }
    m_fingerprint.clear();
}

bool RemoteIdentity::ready() const
{
    return m_cert != nullptr;
}

const std::string& RemoteIdentity::fingerprint() const
{
    return m_fingerprint;
}

PCCERT_CONTEXT RemoteIdentity::certificate() const
{
    return m_cert;
}

std::wstring RemoteIdentity::storePath(const std::string& role)
{
    RemotePaths::ensureLayout();
    std::wstring path = RemotePaths::configDirW();
    path += L"\\";
    path += widen(role.empty() ? "device" : role);
    path += L".pfx.dpapi";
    return path;
}

bool RemoteIdentity::fingerprintEqual(const std::string& a, const std::string& b)
{
    std::string x;
    std::string y;
    for (size_t i = 0; i < a.size(); ++i) {
        const char c = a[i];
        if (c == ' ' || c == ':' || c == '-') {
            continue;
        }
        x.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));
    }
    for (size_t i = 0; i < b.size(); ++i) {
        const char c = b[i];
        if (c == ' ' || c == ':' || c == '-') {
            continue;
        }
        y.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));
    }
    return x == y && !x.empty();
}

std::string RemoteIdentity::fingerprintOfCert(PCCERT_CONTEXT cert)
{
    if (!cert) {
        return std::string();
    }
    unsigned char dig[32];
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return std::string();
    }
    bool ok = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0
        && BCryptHashData(hash, cert->pbCertEncoded, cert->cbCertEncoded, 0) == 0
        && BCryptFinishHash(hash, dig, sizeof(dig), 0) == 0;
    if (hash) {
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok ? toHex(dig, sizeof(dig)) : std::string();
}

bool RemoteIdentity::ensure(const std::string& role, const std::string& displayName, std::string* err)
{
    clear();
    const std::wstring path = storePath(role);
    std::string local;
    if (load(path, &local)) {
        return true;
    }
    // 旧文件常有「有证书无私钥」（不可导出密钥），删掉重建
    if (!local.empty() && local.find("私钥") != std::string::npos) {
        DeleteFileW(path.c_str());
    }
    if (!create(path, displayName.empty() ? role : displayName, err)) {
        if (err && err->empty()) {
            *err = local.empty() ? "身份创建失败" : local;
        } else if (err && !local.empty()) {
            *err = local + "; " + *err;
        }
        return false;
    }
    return true;
}

bool RemoteIdentity::load(const std::wstring& path, std::string* err)
{
    std::vector<unsigned char> sealed;
    if (!readAll(path, &sealed)) {
        if (err) {
            *err = "身份文件不存在";
        }
        return false;
    }
    DATA_BLOB in;
    in.pbData = sealed.data();
    in.cbData = static_cast<DWORD>(sealed.size());
    DATA_BLOB out = {};
    if (!CryptUnprotectData(&in, 0, 0, 0, 0, 0, &out)) {
        if (err) {
            *err = "身份解密失败";
        }
        return false;
    }
    CRYPT_DATA_BLOB pfx;
    pfx.pbData = out.pbData;
    pfx.cbData = out.cbData;
    // 服务端 Schannel 入站需要可定位的持久密钥；NO_PERSIST 常导致 SEC_E_NO_CREDENTIALS
    HCERTSTORE store = PFXImportCertStore(&pfx, L"", CRYPT_USER_KEYSET);
    if (!store) {
        store = PFXImportCertStore(&pfx, L"", 0);
    }
    if (!store) {
        store = PFXImportCertStore(&pfx, L"", PKCS12_NO_PERSIST_KEY);
    }
    LocalFree(out.pbData);
    if (!store) {
        if (err) {
            *err = "身份导入失败";
        }
        return false;
    }
    PCCERT_CONTEXT cert = CertEnumCertificatesInStore(store, nullptr);
    if (!cert) {
        CertCloseStore(store, 0);
        if (err) {
            *err = "身份中没有证书";
        }
        return false;
    }
    clear();
    m_store = static_cast<void*>(store);
    m_cert = cert;
    // 只探测私钥是否可用。证书还要交给 Schannel，绝不能在这里 Free / Release
    DWORD keySpec = 0;
    BOOL callerFree = FALSE;
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE key = 0;
    if (!CryptAcquireCertificatePrivateKey(m_cert, CRYPT_ACQUIRE_CACHE_FLAG | CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG,
            nullptr, &key, &keySpec, &callerFree)) {
        clear();
        if (err) {
            *err = "身份缺少私钥";
        }
        return false;
    }
    m_fingerprint = fingerprintOfCert(m_cert);
    return m_cert != nullptr && !m_fingerprint.empty();
}

bool RemoteIdentity::create(const std::wstring& path, const std::string& displayName, std::string* err)
{
    const std::wstring cn = L"CN=" + widen(displayName);
    CERT_NAME_BLOB subject = {};
    DWORD cb = 0;
    if (!CertStrToNameW(X509_ASN_ENCODING, cn.c_str(), CERT_X500_NAME_STR, 0, 0, &cb, 0)) {
        if (err) {
            *err = "证书名编码失败";
        }
        return false;
    }
    std::vector<BYTE> name(cb);
    if (!CertStrToNameW(X509_ASN_ENCODING, cn.c_str(), CERT_X500_NAME_STR, 0, name.data(), &cb, 0)) {
        if (err) {
            *err = "证书名编码失败";
        }
        return false;
    }
    subject.pbData = name.data();
    subject.cbData = cb;

    CRYPT_KEY_PROV_INFO provInfo = {};
    wchar_t container[80];
    swprintf_s(container, L"Vision.Remote.%u.%u", GetCurrentProcessId(), GetTickCount());
    provInfo.pwszContainerName = container;
    provInfo.pwszProvName = const_cast<LPWSTR>(MS_ENH_RSA_AES_PROV_W);
    provInfo.dwProvType = PROV_RSA_AES;
    provInfo.dwFlags = 0;
    provInfo.dwKeySpec = AT_KEYEXCHANGE;

    // 必须先建可导出密钥容器，否则 PFX 里只有证书没有私钥
    HCRYPTPROV prov = 0;
    CryptAcquireContextW(&prov, container, provInfo.pwszProvName, provInfo.dwProvType, CRYPT_DELETEKEYSET);
    if (!CryptAcquireContextW(&prov, container, provInfo.pwszProvName, provInfo.dwProvType, CRYPT_NEWKEYSET)) {
        if (err) {
            char buf[64];
            sprintf_s(buf, "创建密钥容器失败 (%lu)", GetLastError());
            *err = buf;
        }
        return false;
    }
    HCRYPTKEY key = 0;
    if (!CryptGenKey(prov, AT_KEYEXCHANGE, CRYPT_EXPORTABLE, &key)) {
        CryptReleaseContext(prov, 0);
        CryptAcquireContextW(&prov, container, provInfo.pwszProvName, provInfo.dwProvType, CRYPT_DELETEKEYSET);
        if (err) {
            char buf[64];
            sprintf_s(buf, "生成可导出密钥失败 (%lu)", GetLastError());
            *err = buf;
        }
        return false;
    }
    CryptDestroyKey(key);
    CryptReleaseContext(prov, 0);

    // 服务器/客户端双向认证 EKU，否则 Schannel 入站 AcquireCredentialsHandle 常返回 SEC_E_NO_CREDENTIALS
    LPSTR ekuOids[] = {
        const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH),
        const_cast<LPSTR>(szOID_PKIX_KP_CLIENT_AUTH)
    };
    CERT_ENHKEY_USAGE eku = {};
    eku.cUsageIdentifier = 2;
    eku.rgpszUsageIdentifier = ekuOids;
    DWORD ekuCb = 0;
    CryptEncodeObjectEx(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &eku, 0, 0, 0, &ekuCb);
    std::vector<BYTE> ekuBuf(ekuCb ? ekuCb : 1);
    if (ekuCb == 0 || !CryptEncodeObjectEx(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &eku, 0, 0, ekuBuf.data(), &ekuCb)) {
        CryptAcquireContextW(&prov, container, provInfo.pwszProvName, provInfo.dwProvType, CRYPT_DELETEKEYSET);
        if (err) {
            *err = "编码 EKU 失败";
        }
        return false;
    }
    CERT_EXTENSION ekuExt = {};
    ekuExt.pszObjId = const_cast<LPSTR>(szOID_ENHANCED_KEY_USAGE);
    ekuExt.fCritical = FALSE;
    ekuExt.Value.cbData = ekuCb;
    ekuExt.Value.pbData = ekuBuf.data();
    CERT_EXTENSIONS exts = {};
    exts.cExtension = 1;
    exts.rgExtension = &ekuExt;

    SYSTEMTIME start = {};
    SYSTEMTIME end = {};
    GetSystemTime(&start);
    end = start;
    end.wYear = static_cast<WORD>(end.wYear + 10);

    PCCERT_CONTEXT cert = CertCreateSelfSignCertificate(0, &subject, 0, &provInfo, 0, &start, &end, &exts);
    if (!cert) {
        CryptAcquireContextW(&prov, container, provInfo.pwszProvName, provInfo.dwProvType, CRYPT_DELETEKEYSET);
        if (err) {
            char buf[64];
            sprintf_s(buf, "生成自签证书失败 (%lu)", GetLastError());
            *err = buf;
        }
        return false;
    }

    HCERTSTORE mem = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, 0);
    if (!mem || !CertAddCertificateContextToStore(mem, cert, CERT_STORE_ADD_ALWAYS, 0)) {
        CertFreeCertificateContext(cert);
        if (mem) {
            CertCloseStore(mem, 0);
        }
        if (err) {
            *err = "证书入库失败";
        }
        return false;
    }
    CertFreeCertificateContext(cert);

    CRYPT_DATA_BLOB pfx = {};
    PFXExportCertStoreEx(mem, &pfx, L"", 0, EXPORT_PRIVATE_KEYS);
    std::vector<BYTE> pfxBuf(pfx.cbData ? pfx.cbData : 1);
    pfx.pbData = pfxBuf.data();
    if (!PFXExportCertStoreEx(mem, &pfx, L"", 0, EXPORT_PRIVATE_KEYS)) {
        CertCloseStore(mem, 0);
        if (err) {
            *err = "导出 PFX 失败";
        }
        return false;
    }
    CertCloseStore(mem, 0);

    DATA_BLOB plain;
    plain.pbData = pfx.pbData;
    plain.cbData = pfx.cbData;
    DATA_BLOB sealed = {};
    if (!CryptProtectData(&plain, L"Vision.Remote", 0, 0, 0, 0, &sealed)) {
        if (err) {
            *err = "身份加密失败";
        }
        return false;
    }
    const bool saved = writeAll(path, sealed.pbData, sealed.cbData);
    LocalFree(sealed.pbData);
    if (!saved) {
        if (err) {
            *err = "身份写入失败";
        }
        return false;
    }
    return load(path, err);
}

}
