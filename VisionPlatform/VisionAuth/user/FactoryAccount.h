/**
 * @file FactoryAccount.h
 * @brief 厂家账号。用户名和口令写在代码里，不进用户表
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_FACTORY_ACCOUNT_H
#define VISION_FACTORY_ACCOUNT_H
#include "VisionAuthExport.h"

#include "AuthMgr.h"
#include <string>

namespace Vision {

/**
 * @class FactoryAccount
 * @brief 最高权限。界面不能新建、列出或重置。用户名和口令未填写时不能登录
 */
class VISION_AUTH_API FactoryAccount {
public:
    /** @brief 登录名。尚未填写时为空 */
    static const char* loginName();
    /** @brief 口令。尚未填写时为空 */
    static const char* password();
    /** @brief 用户名和口令都已写上 */
    static bool configured();
    /** @brief 登录名是否就是厂家账号。未配置时为假 */
    static bool ownsName(const std::string& loginName);
    /** @brief 用户名和口令都对上 */
    static bool matches(const std::string& loginName, const std::string& password);
    /** @brief 内存中的登录身份，不写入用户库 */
    static UserAccount account();
};

}

#endif
