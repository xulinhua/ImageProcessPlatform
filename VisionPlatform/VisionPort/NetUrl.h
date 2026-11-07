/**
 * @file NetUrl.h
 * @brief 解析 http/https/ws/wss 地址
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_NET_URL_H
#define VISION_NET_URL_H

#include <string>

namespace Vision {

/** @class NetUrl @brief 一次请求用的地址。没有实例状态 */
class NetUrl {
public:
    /**
     * @brief 拆开 scheme://host[:port]/path
     * @param port 未写端口时按协议填 80 或 443
     */
    static bool parse(const std::string& url, std::string* scheme, std::string* host,
                      int* port, std::string* path, std::string* err);
};

}

#endif
