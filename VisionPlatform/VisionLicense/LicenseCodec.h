/**
 * @file LicenseCodec.h
 * @brief 申请码和注册码的字符串格式。签名不在这里
 */

#ifndef VISION_LICENSE_CODEC_H
#define VISION_LICENSE_CODEC_H

#include "VisionLicenseExport.h"
#include <string>
#include <vector>

namespace Vision {

class VISION_LICENSE_API LicenseCodec {
public:
    /** @brief 产品名、硬件摘要和当前序号，前缀 VP-A1-。serial 小于 0 时为空 */
    static std::string requestFromDigest(const std::string& digest, int serial);
    /** @brief 解开申请码。旧申请码没有序号时，序号为 0 */
    static bool splitRequest(const std::string& code, std::string* digest, int* serial);
    /** @brief 待签名的正文。serial 从 1 起，主程序只接受更大的序号 */
    static std::string payload(const std::string& licenseId, const std::string& machine,
                               bool perpetual, const std::string& until, int serial);
    /** @brief 正文加 64 字节签名，前缀 VP-R1- */
    static std::string registration(const std::string& body, const unsigned char* sig, int sigLen);
    /** @brief 解开注册码。签名长度不是 64 则失败 */
    static bool splitRegistration(const std::string& code, std::string* body, std::vector<unsigned char>* sig);
    /** @brief 从正文取出字段。顺序：产品、编号、摘要、种类、到期日、序号。旧正文没有序号时序号为 0 */
    static bool splitPayload(const std::string& body, std::string* product, std::string* licenseId,
                             std::string* machine, std::string* kind, std::string* until, int* serial);
};

}

#endif
