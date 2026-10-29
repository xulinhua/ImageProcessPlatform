/**
 * @file FactoryAccount.cpp
 * @brief FactoryAccount实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FactoryAccount.h"
#include "VpUtil.h"

namespace Vision {

namespace {

const char kLogin[] = "";
const char kPassword[] = "";

}

const char* FactoryAccount::loginName()
{
    return kLogin;
}

const char* FactoryAccount::password()
{
    return kPassword;
}

bool FactoryAccount::configured()
{
    return kLogin[0] != '\0' && kPassword[0] != '\0';
}

bool FactoryAccount::ownsName(const std::string& name)
{
    if (!configured()) {
        return false;
    }
    return vpLower(name) == vpLower(kLogin);
}

bool FactoryAccount::matches(const std::string& name, const std::string& password)
{
    return ownsName(name) && password == kPassword;
}

UserAccount FactoryAccount::account()
{
    UserAccount u;
    u.id = "factory";
    u.loginName = kLogin;
    u.displayName = roleTitle(RoleFactory);
    u.role = RoleFactory;
    u.enabled = true;
    return u;
}

}
