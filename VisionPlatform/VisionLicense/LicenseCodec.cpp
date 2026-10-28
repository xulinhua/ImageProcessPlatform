/**
 * @file LicenseCodec.cpp
 * @brief 申请码 VP-A1-，注册码 VP-R1-
 */

#include "LicenseCodec.h"
#include <cstring>

namespace Vision {

namespace {

const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

int alphabetIndex(char c)
{
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    for (int i = 0; kAlphabet[i] != 0; ++i) {
        if (kAlphabet[i] == c) {
            return i;
        }
    }
    return -1;
}

std::string b32(const unsigned char* data, size_t n)
{
    std::string raw;
    unsigned int acc = 0;
    int bits = 0;
    for (size_t i = 0; i < n; ++i) {
        acc = (acc << 8) | data[i];
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            raw.push_back(kAlphabet[(acc >> bits) & 31]);
        }
    }
    if (bits > 0) {
        raw.push_back(kAlphabet[(acc << (5 - bits)) & 31]);
    }
    std::string grouped;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (i > 0 && (i % 5) == 0) {
            grouped.push_back('-');
        }
        grouped.push_back(raw[i]);
    }
    return grouped;
}

bool b32dec(const std::string& text, std::vector<unsigned char>* out)
{
    if (!out) {
        return false;
    }
    out->clear();
    unsigned int acc = 0;
    int bits = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '-' || c == ' ' || c == '\r' || c == '\n' || c == '\t') {
            continue;
        }
        const int v = alphabetIndex(c);
        if (v < 0) {
            return false;
        }
        acc = (acc << 5) | static_cast<unsigned int>(v);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out->push_back(static_cast<unsigned char>((acc >> bits) & 0xFF));
        }
    }
    return true;
}

std::string pack(const std::string& code)
{
    std::string out;
    for (size_t i = 0; i < code.size(); ++i) {
        const char c = code[i];
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
            continue;
        }
        out.push_back(c);
    }
    return out;
}

bool parseSerial(const std::string& text, int* serial)
{
    if (!serial || text.empty()) {
        return false;
    }
    int value = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c < '0' || c > '9') {
            return false;
        }
        const int digit = c - '0';
        if (value > (2147483647 - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    *serial = value;
    return true;
}

std::string fieldAt(const std::string& body, int index)
{
    int n = 0;
    std::string cur;
    for (size_t i = 0; i <= body.size(); ++i) {
        if (i == body.size() || body[i] == '\n') {
            if (n == index) {
                return cur;
            }
            ++n;
            cur.clear();
        } else {
            cur.push_back(body[i]);
        }
    }
    return std::string();
}

}

std::string LicenseCodec::requestFromDigest(const std::string& digest, int serial)
{
    if (digest.empty() || serial < 0) {
        return std::string();
    }
    const std::string body = std::string("VisionPlatform\n") + digest + "\n" + std::to_string(serial) + "\n";
    return std::string("VP-A1-") + b32(reinterpret_cast<const unsigned char*>(body.data()), body.size());
}

bool LicenseCodec::splitRequest(const std::string& code, std::string* digest, int* serial)
{
    const std::string packed = pack(code);
    if (packed.compare(0, 6, "VP-A1-") != 0 || !digest || !serial) {
        return false;
    }
    std::vector<unsigned char> raw;
    if (!b32dec(packed.substr(6), &raw) || raw.empty()) {
        return false;
    }
    const std::string body(raw.begin(), raw.end());
    const std::string product = fieldAt(body, 0);
    const std::string machine = fieldAt(body, 1);
    if (product != "VisionPlatform" || machine.empty()) {
        return false;
    }
    const std::string serialText = fieldAt(body, 2);
    int parsed = 0;
    if (!serialText.empty() && !parseSerial(serialText, &parsed)) {
        return false;
    }
    *digest = machine;
    *serial = parsed;
    return true;
}

std::string LicenseCodec::payload(const std::string& licenseId, const std::string& machine,
                                  bool perpetual, const std::string& until, int serial)
{
    if (serial < 1) {
        return std::string();
    }
    std::string body = std::string("VisionPlatform\n") + licenseId + "\n" + machine + "\n";
    if (perpetual) {
        body += "perpetual\n\n";
    } else {
        body += std::string("until\n") + until + "\n";
    }
    body += std::to_string(serial);
    body += "\n";
    return body;
}

std::string LicenseCodec::registration(const std::string& body, const unsigned char* sig, int sigLen)
{
    if (!sig || sigLen != 64 || body.size() > 0xFFFF) {
        return std::string();
    }
    std::vector<unsigned char> raw(2 + body.size() + 64);
    raw[0] = static_cast<unsigned char>(body.size() & 0xFF);
    raw[1] = static_cast<unsigned char>((body.size() >> 8) & 0xFF);
    memcpy(&raw[2], body.data(), body.size());
    memcpy(&raw[2 + body.size()], sig, 64);
    return std::string("VP-R1-") + b32(&raw[0], raw.size());
}

bool LicenseCodec::splitRegistration(const std::string& code, std::string* body, std::vector<unsigned char>* sig)
{
    const std::string packed = pack(code);
    if (!body || !sig || packed.compare(0, 6, "VP-R1-") != 0) {
        return false;
    }
    std::vector<unsigned char> raw;
    if (!b32dec(packed.substr(6), &raw) || raw.size() < 2 + 64) {
        return false;
    }
    const int len = raw[0] | (static_cast<int>(raw[1]) << 8);
    if (len <= 0 || static_cast<size_t>(2 + len + 64) > raw.size()) {
        return false;
    }
    body->assign(reinterpret_cast<const char*>(&raw[2]), static_cast<size_t>(len));
    sig->assign(raw.begin() + 2 + len, raw.begin() + 2 + len + 64);
    return true;
}

bool LicenseCodec::splitPayload(const std::string& body, std::string* product, std::string* licenseId,
                                std::string* machine, std::string* kind, std::string* until, int* serial)
{
    if (!product || !licenseId || !machine || !kind || !until || !serial) {
        return false;
    }
    *product = fieldAt(body, 0);
    *licenseId = fieldAt(body, 1);
    *machine = fieldAt(body, 2);
    *kind = fieldAt(body, 3);
    *until = fieldAt(body, 4);
    *serial = 0;
    const std::string serialText = fieldAt(body, 5);
    if (!serialText.empty() && !parseSerial(serialText, serial)) {
        return false;
    }
    return *product == "VisionPlatform" && !licenseId->empty() && !machine->empty()
        && (*kind == "perpetual" || *kind == "until");
}

}
