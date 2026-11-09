/**
 * @file LicenseFile.h
 * @brief ProgramData 下的 license.dat。密钥带本机硬件摘要
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LICENSE_FILE_H
#define VISION_LICENSE_FILE_H

#include <string>

namespace Vision {

/**
 * @class LicenseFile
 * @brief 许可证文件读写：VPL1 魔数 + 版本 + AES-GCM 密文（正文长度、正文、64 字节签名）
 * @note 密钥由内置主密钥和硬件摘要派生，换机器后解不开
 */
class LicenseFile {
public:
    /** @brief 固定路径（UTF-8）。目录建不出来时返回空 */
    std::string path() const;
    /** @brief 文件存在且不是目录 */
    bool exists() const;
    /** @brief 加密写入正文和 64 字节签名。摘要变了就解不开 */
    bool write(const std::string& digest, const std::string& body, const unsigned char* sig) const;
    /** @brief 解开。魔数不对 damaged=true。标签不对 damaged=false，表示像换了机器 */
    bool read(const std::string& digest, std::string* body, unsigned char sig[64], bool* damaged) const;
};

}

#endif
