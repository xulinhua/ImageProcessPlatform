/**
 * @file VisionLicenseTool.h
 * @brief 给申请机和注册机的接口。不提供给 VisionPlatform
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LICENSE_TOOL_H
#define VISION_LICENSE_TOOL_H

#include "VisionLicense.h"
#include <string>
#include <vector>

namespace Vision {

/** @brief 许可种类，对应载荷里的 kind 字段 */
enum LicenseKind {
    LicensePerpetual = 0,   ///< 永久
    LicenseUntil            ///< 限期，到期日见 LicenseInfo::until
};

/**
 * @class LicenseInfo
 * @brief 解开后的许可摘要，给申请机显示
 */
struct LicenseInfo {
    LicenseKind kind;       ///< 永久或限期
    std::string licenseId;  ///< 签发方给的许可编号
    std::string until;      ///< 到期日 YYYY-MM-DD。永久注册时为空
    int serial;             ///< 签发序号。还没有许可证时是 0
    /** @brief 默认永久、序号 0 */
    LicenseInfo() : kind(LicensePerpetual), serial(0) {}
};

/**
 * @class LicenseTool
 * @brief 客户机：申请码、验注册码并写入许可证
 */
class LicenseTool {
public:
    /** @brief 尚未建立许可 */
    LicenseTool();
    /** @brief 释放内部实现 */
    ~LicenseTool();

    /** @brief 按当前硬件现算申请码。硬件不够时为空 */
    std::string requestCode() const;
    /** @brief 验注册码。序号更大才覆盖许可证文件，并重写三处时间记录 */
    LicenseReason accept(const std::string& registrationCode);
    /** @brief 校验已经落地的许可证文件 */
    LicenseReason open();
    /** @brief 最近一次通过时的许可信息 */
    LicenseInfo describe() const;
    /** @brief 给界面的一句原因 */
    std::string reasonText(LicenseReason reason) const;

private:
    LicenseTool(const LicenseTool&);
    LicenseTool& operator=(const LicenseTool&);
    class Impl;
    Impl* m;
};

/**
 * @class LicenseCodec
 * @brief 申请码、注册码与签名正文的编解码。签名不在这里
 */
class LicenseCodec {
public:
    /** @brief 产品名、硬件摘要和当前序号，前缀 VP-A1-。serial 小于 0 时为空 */
    static std::string requestFromDigest(const std::string& digest, int serial);
    /** @brief 解开申请码。旧申请码没有序号时，序号为 0 */
    static bool splitRequest(const std::string& code, std::string* digest, int* serial);
    /** @brief 待签名的正文。serial 和 keyId 都从 1 起。最后一行是钥匙编号 */
    static std::string payload(const std::string& licenseId, const std::string& machine,
                               bool perpetual, const std::string& until, int serial, int keyId);
    /** @brief 正文加 64 字节签名，前缀 VP-R1- */
    static std::string registration(const std::string& body, const unsigned char* sig, int sigLen);
    /** @brief 解开注册码。签名长度不是 64 则失败 */
    static bool splitRegistration(const std::string& code, std::string* body, std::vector<unsigned char>* sig);
    /** @brief 从正文取出字段。旧正文没有序号时序号为 0 */
    static bool splitPayload(const std::string& body, std::string* product, std::string* licenseId,
                             std::string* machine, std::string* kind, std::string* until, int* serial);
};

}

#endif
