/**
 * @file NetUrl.cpp
 * @brief NetUrl实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "NetUrl.h"
#include "ICommChannel.h"
#include <cstdlib>

namespace Vision {

bool NetUrl::parse(const std::string& url, std::string* scheme, std::string* host,
                   int* port, std::string* path, std::string* err)
{
    if (!scheme || !host || !port || !path) {
        setCommErr(err, "地址参数为空");
        return false;
    }
    const size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos || schemeEnd == 0) {
        setCommErr(err, "地址缺少协议");
        return false;
    }
    *scheme = url.substr(0, schemeEnd);
    const std::string rest = url.substr(schemeEnd + 3);
    const size_t slash = rest.find('/');
    const std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    *path = slash == std::string::npos ? std::string("/") : rest.substr(slash);
    if (hostport.empty()) {
        setCommErr(err, "地址缺少主机");
        return false;
    }
    const size_t colon = hostport.rfind(':');
    if (colon != std::string::npos && hostport.find(':') == colon) {
        *host = hostport.substr(0, colon);
        *port = std::atoi(hostport.c_str() + colon + 1);
        if (*port <= 0 || *port > 65535) {
            setCommErr(err, "端口无效");
            return false;
        }
    } else {
        *host = hostport;
        const bool secure = *scheme == "https" || *scheme == "wss";
        *port = secure ? 443 : 80;
    }
    if (host->empty()) {
        setCommErr(err, "地址缺少主机");
        return false;
    }
    return true;
}

}
