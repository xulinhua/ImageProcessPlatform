/**
 * @file LicenseSign.h
 * @brief 用编进库的公钥验注册码正文。私钥不在本库
 */

#ifndef VISION_LICENSE_SIGN_H
#define VISION_LICENSE_SIGN_H

#include <string>

namespace Vision {

class LicenseSign {
public:
    static bool verify(const std::string& body, const unsigned char* sig, int sigLen);
};

}

#endif
