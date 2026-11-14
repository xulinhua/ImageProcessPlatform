/**
 * @file Identity.cpp
 * @brief 设备号和自签证书
 */

#include "Identity.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509v3.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace Vision {

namespace {

std::string hexOf(const unsigned char* data, size_t n)
{
    static const char* kHex = "0123456789abcdef";
    std::string out(n * 2, '0');
    for (size_t i = 0; i < n; ++i) {
        out[i * 2] = kHex[(data[i] >> 4) & 0xf];
        out[i * 2 + 1] = kHex[data[i] & 0xf];
    }
    return out;
}

bool writeText(const std::string& path, const std::string& text)
{
    std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << text;
    return file.good();
}

std::string readText(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file) {
        return std::string();
    }
    std::string text;
    file.seekg(0, std::ios::end);
    text.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);
    if (!text.empty()) {
        file.read(&text[0], static_cast<std::streamsize>(text.size()));
    }
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

}

Identity::Identity()
    : m_ctx(0)
    , m_cert(0)
    , m_key(0)
{
}

Identity::~Identity()
{
    if (m_ctx) {
        SSL_CTX_free(m_ctx);
    }
    if (m_cert) {
        X509_free(m_cert);
    }
    if (m_key) {
        EVP_PKEY_free(m_key);
    }
}

const std::string& Identity::deviceId() const
{
    return m_deviceId;
}

std::string Identity::machineMaterial() const
{
    std::string id = readText("/etc/machine-id");
    if (id.size() < 8) {
        id = readText("/var/lib/dbus/machine-id");
    }
    if (id.size() < 8) {
        char name[256];
        if (gethostname(name, sizeof(name)) == 0 && name[0]) {
            id = name;
        }
    }
    if (id.empty()) {
        id = "remote-client";
    }
    return id;
}

std::string Identity::formatDeviceId(const std::string& material) const
{
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < material.size(); ++i) {
        hash ^= static_cast<unsigned char>(material[i]);
        hash *= 1099511628211ULL;
    }
    const unsigned int n = static_cast<unsigned int>(hash % 1000000000ULL);
    char text[16];
    std::snprintf(text, sizeof(text), "%03u %03u %03u", n / 1000000U, (n / 1000U) % 1000U, n % 1000U);
    return text;
}

bool Identity::nineDigitId(const std::string& id) const
{
    if (id.size() != 11) {
        return false;
    }
    for (size_t i = 0; i < id.size(); ++i) {
        if (i == 3 || i == 7) {
            if (id[i] != ' ') {
                return false;
            }
        } else if (id[i] < '0' || id[i] > '9') {
            return false;
        }
    }
    return true;
}

const std::string& Identity::fingerprint() const
{
    return m_fingerprint;
}

SSL_CTX* Identity::context() const
{
    return m_ctx;
}

bool Identity::loadOrCreate(std::string* err)
{
    const char* home = getenv("HOME");
    if (!home || !home[0]) {
        if (err) {
            *err = "没有 HOME";
        }
        return false;
    }
    const std::string dir = std::string(home) + "/.config/RemoteClient";
    mkdir((std::string(home) + "/.config").c_str(), 0700);
    mkdir(dir.c_str(), 0700);

    const std::string next = formatDeviceId(machineMaterial());
    m_deviceId = readText(dir + "/device-id");
    if (m_deviceId != next || !nineDigitId(m_deviceId)) {
        m_deviceId = next;
        if (!writeText(dir + "/device-id", m_deviceId + "\n")) {
            if (err) {
                *err = "写设备号失败";
            }
            return false;
        }
    }

    const std::string certPath = dir + "/cert.pem";
    const std::string keyPath = dir + "/key.pem";
    if (!loadCert(certPath, keyPath, err)) {
        if (!createCert(dir, err)) {
            return false;
        }
    }
    return makeContext(err);
}

bool Identity::loadCert(const std::string& certPath, const std::string& keyPath, std::string* err)
{
    FILE* certFile = fopen(certPath.c_str(), "rb");
    FILE* keyFile = fopen(keyPath.c_str(), "rb");
    if (!certFile || !keyFile) {
        if (certFile) {
            fclose(certFile);
        }
        if (keyFile) {
            fclose(keyFile);
        }
        if (err) {
            err->clear();
        }
        return false;
    }
    m_cert = PEM_read_X509(certFile, 0, 0, 0);
    m_key = PEM_read_PrivateKey(keyFile, 0, 0, 0);
    fclose(certFile);
    fclose(keyFile);
    if (!m_cert || !m_key) {
        if (err) {
            *err = "证书文件打不开";
        }
        return false;
    }
    return true;
}

bool Identity::createCert(const std::string& dir, std::string* err)
{
    if (m_cert) {
        X509_free(m_cert);
        m_cert = 0;
    }
    if (m_key) {
        EVP_PKEY_free(m_key);
        m_key = 0;
    }
    EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, 0);
    if (!kctx || EVP_PKEY_keygen_init(kctx) <= 0 || EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 2048) <= 0
        || EVP_PKEY_keygen(kctx, &m_key) <= 0) {
        EVP_PKEY_CTX_free(kctx);
        if (err) {
            *err = "生成密钥失败";
        }
        return false;
    }
    EVP_PKEY_CTX_free(kctx);

    m_cert = X509_new();
    if (!m_cert) {
        if (err) {
            *err = "生成证书失败";
        }
        return false;
    }
    X509_set_version(m_cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(m_cert), 1);
    X509_gmtime_adj(X509_getm_notBefore(m_cert), 0);
    X509_gmtime_adj(X509_getm_notAfter(m_cert), 60L * 60 * 24 * 3650);
    X509_set_pubkey(m_cert, m_key);
    X509_NAME* name = X509_get_subject_name(m_cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("RemoteClient"), -1, -1, 0);
    X509_set_issuer_name(m_cert, name);

    X509V3_CTX extCtx;
    X509V3_set_ctx_nodb(&extCtx);
    X509V3_set_ctx(&extCtx, m_cert, m_cert, 0, 0, 0);
    // 中控台 Schannel 入站要求这两种用途，否则对端拿不到可用证书
    X509_EXTENSION* ku = X509V3_EXT_conf_nid(0, &extCtx, NID_key_usage, "digitalSignature,keyEncipherment");
    X509_EXTENSION* eku = X509V3_EXT_conf_nid(0, &extCtx, NID_ext_key_usage, "serverAuth,clientAuth");
    if (!ku || !eku) {
        if (err) {
            *err = "证书扩展失败";
        }
        return false;
    }
    X509_add_ext(m_cert, ku, -1);
    X509_add_ext(m_cert, eku, -1);
    X509_EXTENSION_free(ku);
    X509_EXTENSION_free(eku);
    if (X509_sign(m_cert, m_key, EVP_sha256()) == 0) {
        if (err) {
            *err = "证书签名失败";
        }
        return false;
    }

    FILE* certFile = fopen((dir + "/cert.pem").c_str(), "wb");
    FILE* keyFile = fopen((dir + "/key.pem").c_str(), "wb");
    if (!certFile || !keyFile) {
        if (certFile) {
            fclose(certFile);
        }
        if (keyFile) {
            fclose(keyFile);
        }
        if (err) {
            *err = "写证书失败";
        }
        return false;
    }
    PEM_write_X509(certFile, m_cert);
    PEM_write_PrivateKey(keyFile, m_key, 0, 0, 0, 0, 0);
    fclose(certFile);
    fclose(keyFile);
    chmod((dir + "/key.pem").c_str(), 0600);
    return true;
}

bool Identity::makeContext(std::string* err)
{
    // 和 Windows RemoteIdentity 一样：对证书 DER 做 SHA-256，再写成小写十六进制
    int len = i2d_X509(m_cert, 0);
    if (len <= 0) {
        if (err) {
            *err = "导出证书失败";
        }
        return false;
    }
    std::vector<unsigned char> der(static_cast<size_t>(len));
    unsigned char* p = der.data();
    i2d_X509(m_cert, &p);
    unsigned char dig[SHA256_DIGEST_LENGTH];
    SHA256(der.data(), der.size(), dig);
    m_fingerprint = hexOf(dig, sizeof(dig));

    m_ctx = SSL_CTX_new(TLS_client_method());
    if (!m_ctx) {
        if (err) {
            *err = "创建 TLS 上下文失败";
        }
        return false;
    }
    SSL_CTX_set_min_proto_version(m_ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(m_ctx, TLS1_2_VERSION);
    SSL_CTX_set_verify(m_ctx, SSL_VERIFY_NONE, 0);
    if (SSL_CTX_use_certificate(m_ctx, m_cert) != 1 || SSL_CTX_use_PrivateKey(m_ctx, m_key) != 1) {
        if (err) {
            *err = "装入证书失败";
        }
        return false;
    }
    return true;
}

}
