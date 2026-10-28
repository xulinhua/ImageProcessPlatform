/**
 * @file SoftLicense.cpp
 * @brief 验注册码、写 license.dat、查三处时间记录
 */

#include "SoftLicense.h"
#include "ClockLog.h"
#include "HardwareProbe.h"
#include "LicenseCodec.h"
#include "LicenseFile.h"
#include "LicenseSign.h"
#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

int storedSerial(const std::string& digest)
{
    LicenseFile file;
    if (!file.exists()) {
        return 0;
    }
    std::string body;
    unsigned char sig[64];
    bool damaged = false;
    if (!file.read(digest, &body, sig, &damaged)) {
        return 0;
    }
    std::string product;
    std::string licenseId;
    std::string machine;
    std::string kind;
    std::string until;
    int serial = 0;
    if (!LicenseCodec::splitPayload(body, &product, &licenseId, &machine, &kind, &until, &serial)) {
        return 0;
    }
    return serial;
}

bool pastDue(const std::string& until)
{
    if (until.size() != 10) {
        return true;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    char today[16];
    sprintf_s(today, "%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
    return std::string(today) > until;
}

}

SoftLicense::SoftLicense()
    : m_ready(false)
{}

std::string SoftLicense::machineId() const
{
    return HardwareProbe().digest();
}

std::string SoftLicense::requestCode() const
{
    const std::string digest = machineId();
    if (digest.empty()) {
        return std::string();
    }
    return LicenseCodec::requestFromDigest(digest, storedSerial(digest));
}

LicenseInfo SoftLicense::describe() const
{
    return m_info;
}

LicenseReason SoftLicense::accept(const std::string& registrationCode)
{
    const std::string digest = machineId();
    if (digest.empty()) {
        return LicenseMachine;
    }
    std::string body;
    std::vector<unsigned char> sig;
    if (!LicenseCodec::splitRegistration(registrationCode, &body, &sig)
        || !LicenseSign::verify(body, &sig[0], static_cast<int>(sig.size()))) {
        return LicenseBadSign;
    }
    std::string product;
    std::string licenseId;
    std::string machine;
    std::string kind;
    std::string until;
    int serial = 0;
    if (!LicenseCodec::splitPayload(body, &product, &licenseId, &machine, &kind, &until, &serial)) {
        return LicenseBadSign;
    }
    if (product != "VisionPlatform") {
        return LicenseProduct;
    }
    if (machine != digest) {
        return LicenseMachine;
    }
    if (serial < 1 || serial <= storedSerial(digest)) {
        return LicenseSerial;
    }
    if (kind == "until" && pastDue(until)) {
        return LicenseExpired;
    }
    if (!LicenseFile().write(digest, body, &sig[0])) {
        return LicenseSaveFailed;
    }
    if (!ClockLog().reset()) {
        return LicenseSaveFailed;
    }
    m_digest = digest;
    m_info.licenseId = licenseId;
    m_info.kind = kind == "until" ? LicenseUntil : LicensePerpetual;
    m_info.until = kind == "until" ? until : std::string();
    m_info.serial = serial;
    m_ready = true;
    return LicenseOk;
}

LicenseReason SoftLicense::checkFile(bool writeRecords)
{
    m_ready = false;
    m_digest = machineId();
    if (m_digest.empty()) {
        return LicenseMachine;
    }
    LicenseFile file;
    if (!file.exists()) {
        return LicenseMissing;
    }
    std::string body;
    unsigned char sig[64];
    bool damaged = false;
    if (!file.read(m_digest, &body, sig, &damaged)) {
        return damaged ? LicenseDamaged : LicenseMachine;
    }
    if (!LicenseSign::verify(body, sig, 64)) {
        return LicenseDamaged;
    }
    std::string product;
    std::string licenseId;
    std::string machine;
    std::string kind;
    std::string until;
    int serial = 0;
    if (!LicenseCodec::splitPayload(body, &product, &licenseId, &machine, &kind, &until, &serial)) {
        return LicenseDamaged;
    }
    if (product != "VisionPlatform") {
        return LicenseProduct;
    }
    if (machine != m_digest) {
        return LicenseMachine;
    }
    if (kind == "until" && pastDue(until)) {
        return LicenseExpired;
    }
    long long stamp = 0;
    int found = 0;
    ClockLog clock;
    clock.latest(&stamp, &found);
    if (found == 0) {
        return LicenseRecordsCleared;
    }
    if (clock.now() < stamp) {
        return LicenseClock;
    }
    if (writeRecords && !clock.writeNow()) {
        return LicenseSaveFailed;
    }
    m_info.licenseId = licenseId;
    m_info.kind = kind == "until" ? LicenseUntil : LicensePerpetual;
    m_info.until = kind == "until" ? until : std::string();
    m_info.serial = serial;
    m_ready = true;
    return LicenseOk;
}

LicenseReason SoftLicense::open()
{
    return checkFile(true);
}

LicenseReason SoftLicense::poll()
{
    return checkFile(true);
}

}
