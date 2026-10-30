/**
 * @file RemoteAuth.cpp
 * @brief 验证码、PBKDF2 密码、防爆破
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteAuth.h"
#include "Messages.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <cstring>
#include <sstream>

#pragma comment(lib, "bcrypt.lib")

namespace Vision {

RemoteAuth::RemoteAuth()
    : m_codeAt(0)
    , m_hasPassword(false)
    , m_failCount(0)
    , m_lockUntil(0)
    , m_lockSec(RemoteConst::kAuthLockSec)
{
    std::memset(m_salt, 0, sizeof(m_salt));
    std::memset(m_hash, 0, sizeof(m_hash));
}

uint64_t RemoteAuth::nowMs()
{
    return GetTickCount64();
}

std::string RemoteAuth::toHex(const unsigned char* data, size_t n)
{
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        out[i * 2] = kHex[(data[i] >> 4) & 0xf];
        out[i * 2 + 1] = kHex[data[i] & 0xf];
    }
    return out;
}

bool RemoteAuth::fromHex(const std::string& hex, unsigned char* out, size_t n)
{
    if (hex.size() != n * 2) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        unsigned int v = 0;
        if (sscanf_s(hex.c_str() + i * 2, "%2x", &v) != 1) {
            return false;
        }
        out[i] = static_cast<unsigned char>(v);
    }
    return true;
}

bool RemoteAuth::pbkdf2(const std::string& password, const unsigned char* salt, size_t saltLen,
                        unsigned char* out, size_t outLen)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
        return false;
    }
    const NTSTATUS st = BCryptDeriveKeyPBKDF2(
        alg,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        const_cast<PUCHAR>(salt),
        static_cast<ULONG>(saltLen),
        kPbkdf2Iter,
        out,
        static_cast<ULONG>(outLen),
        0);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0;
}

std::string RemoteAuth::fingerprintOf(const std::string& material)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char dig[32];
    DWORD digLen = sizeof(dig);
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return std::string();
    }
    bool ok = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0
        && BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(material.data())),
                          static_cast<ULONG>(material.size()), 0) == 0
        && BCryptFinishHash(hash, dig, digLen, 0) == 0;
    if (hash) {
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok ? toHex(dig, sizeof(dig)) : std::string();
}

std::string RemoteAuth::refreshCode()
{
    unsigned char rnd[4];
    if (BCryptGenRandom(nullptr, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        rnd[0] = static_cast<unsigned char>(GetTickCount() & 0xff);
        rnd[1] = static_cast<unsigned char>((GetTickCount() >> 8) & 0xff);
        rnd[2] = static_cast<unsigned char>((GetTickCount() >> 16) & 0xff);
        rnd[3] = static_cast<unsigned char>((GetTickCount() >> 24) & 0xff);
    }
    const unsigned int n = (static_cast<unsigned int>(rnd[0]) << 24)
        | (static_cast<unsigned int>(rnd[1]) << 16)
        | (static_cast<unsigned int>(rnd[2]) << 8)
        | static_cast<unsigned int>(rnd[3]);
    char buf[16];
    sprintf_s(buf, "%06u", n % 1000000u);
    m_code = buf;
    m_codeAt = nowMs();
    return m_code;
}

std::string RemoteAuth::currentCode()
{
    if (m_code.empty() || nowMs() - m_codeAt >= static_cast<uint64_t>(kCodeTtlMs)) {
        return refreshCode();
    }
    return m_code;
}

bool RemoteAuth::checkCode(const std::string& token)
{
    std::string t;
    for (size_t i = 0; i < token.size(); ++i) {
        if (token[i] >= '0' && token[i] <= '9') {
            t.push_back(token[i]);
        }
    }
    const std::string expect = currentCode();
    if (t != expect) {
        return false;
    }
    refreshCode();
    return true;
}

std::string RemoteAuth::setPassword(const std::string& password)
{
    if (password.empty()) {
        m_hasPassword = false;
        std::memset(m_salt, 0, sizeof(m_salt));
        std::memset(m_hash, 0, sizeof(m_hash));
        return std::string();
    }
    if (BCryptGenRandom(nullptr, m_salt, sizeof(m_salt), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return std::string();
    }
    if (!pbkdf2(password, m_salt, sizeof(m_salt), m_hash, sizeof(m_hash))) {
        return std::string();
    }
    m_hasPassword = true;
    return toHex(m_salt, sizeof(m_salt)) + ":" + toHex(m_hash, sizeof(m_hash));
}

bool RemoteAuth::loadPasswordHash(const std::string& stored)
{
    m_hasPassword = false;
    const size_t colon = stored.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    if (!fromHex(stored.substr(0, colon), m_salt, sizeof(m_salt))
        || !fromHex(stored.substr(colon + 1), m_hash, sizeof(m_hash))) {
        return false;
    }
    m_hasPassword = true;
    return true;
}

bool RemoteAuth::hasPassword() const
{
    return m_hasPassword;
}

bool RemoteAuth::checkPassword(const std::string& password) const
{
    if (!m_hasPassword) {
        return false;
    }
    unsigned char dig[32];
    if (!pbkdf2(password, m_salt, sizeof(m_salt), dig, sizeof(dig))) {
        return false;
    }
    return std::memcmp(dig, m_hash, sizeof(m_hash)) == 0;
}

bool RemoteAuth::onFail(uint32_t* lockSecLeft)
{
    m_failCount += 1;
    if (m_failCount < RemoteConst::kAuthFailMax) {
        if (lockSecLeft) {
            *lockSecLeft = 0;
        }
        return false;
    }
    m_lockUntil = nowMs() + static_cast<uint64_t>(m_lockSec) * 1000ull;
    if (lockSecLeft) {
        *lockSecLeft = static_cast<uint32_t>(m_lockSec);
    }
    // 再失败翻倍，最长 1 小时
    m_lockSec = m_lockSec * 2;
    if (m_lockSec > 3600) {
        m_lockSec = 3600;
    }
    m_failCount = 0;
    return true;
}

void RemoteAuth::onSuccess()
{
    m_failCount = 0;
    m_lockUntil = 0;
    m_lockSec = RemoteConst::kAuthLockSec;
}

bool RemoteAuth::isLocked(uint32_t* lockSecLeft) const
{
    const uint64_t now = nowMs();
    if (m_lockUntil == 0 || now >= m_lockUntil) {
        if (lockSecLeft) {
            *lockSecLeft = 0;
        }
        return false;
    }
    if (lockSecLeft) {
        *lockSecLeft = static_cast<uint32_t>((m_lockUntil - now + 999) / 1000);
    }
    return true;
}

uint8_t RemoteAuth::triesLeft() const
{
    if (m_failCount >= RemoteConst::kAuthFailMax) {
        return 0;
    }
    return static_cast<uint8_t>(RemoteConst::kAuthFailMax - m_failCount);
}

}
