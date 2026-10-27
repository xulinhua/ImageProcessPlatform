/**
 * @file CommNet.h
 * @brief CommNet头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMM_NET_H
#define VISION_COMM_NET_H
#include "VisionCommExport.h"

#include "CommWin.h"
#include "ICommChannel.h"
#include <string>

namespace Vision {

/** @brief 进程内初始化 Winsock，可重复调用 */
VISION_COMM_API void commNetEnsure();

/** @brief 解析 IPv4 主机名或点分地址 */
VISION_COMM_API bool commResolveIpv4(const std::string& host, unsigned long* addr, std::string* err);

/**
 * @brief 等待套接字可读或可写
 * @return 1 就绪，0 超时，-1 失败
 */
VISION_COMM_API int commWaitSocket(SOCKET sock, bool wantRead, int timeoutMs);

/** @brief 发完 data 或失败 */
VISION_COMM_API bool commSendAll(SOCKET sock, const CommBytes& data, int timeoutMs, std::string* err);

/**
 * @brief 读一批
 * @return 1 有数据，0 超时，-1 失败，-2 对端关闭
 */
VISION_COMM_API int commRecvSome(SOCKET sock, int maxBytes, int timeoutMs, CommBytes* out, std::string* err);

/** @brief 点分地址加端口 */
VISION_COMM_API std::string commEndpointText(const sockaddr_in& addr);

}

#endif
