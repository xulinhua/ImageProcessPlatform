/**
 * @file LicenseFile.h
 * @brief ProgramData 下的 license.dat。密钥带本机硬件摘要
 */

#ifndef VISION_LICENSE_FILE_H
#define VISION_LICENSE_FILE_H

#include <string>

namespace Vision {

class LicenseFile {
public:
    /** @brief 固定路径。目录建不出来时返回空 */
    std::string path() const;
    bool exists() const;
    /** @brief 加密写入正文和 64 字节签名。摘要变了就解不开 */
    bool write(const std::string& digest, const std::string& body, const unsigned char* sig) const;
    /** @brief 解开。魔数不对 damaged=true。标签不对 damaged=false，表示像换了机器 */
    bool read(const std::string& digest, std::string* body, unsigned char sig[64], bool* damaged) const;
};

}

#endif
