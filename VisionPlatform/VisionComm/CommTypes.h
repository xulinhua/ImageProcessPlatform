/**
 * @file CommTypes.h
 * @brief CommTypes头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMM_TYPES_H
#define VISION_COMM_TYPES_H
#include "VisionCommExport.h"
#include "VisionCoreExport.h"

#include "ObjectId.h"
#include "ParamValue.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @brief 通信传输类型。协议不在这里区分
 * params["protocol"]：raw 或 modbus
 * params["role"]：master 或 slave（仅 modbus）
 * params["slaveId"]：从站号
 * 串口：port、baud、dataBits、parity（none/even/odd）、stopBits
 * TCP 客户端：host、port
 * TCP 服务端：port
 * UDP：port（本机）、host、remotePort
 * HTTP：url、timeoutMs、token
 * WebSocket：url、timeoutMs、token
 */
enum CommChannelType {
    CommChannelTypeUndefined = -1, ///< 未定义
    CommSerial = 0,    ///< 串口
    CommTcpClient = 1, ///< TCP 客户端
    CommTcpServer = 2, ///< TCP 服务端
    CommUdp = 3,       ///< UDP
    CommHttp = 4,      ///< HTTP 客户端
    CommWebSocket = 5, ///< WebSocket 客户端
    CommChannelTypeMax ///< 个数，不是合法值
};

/** @class CommChannel @brief 方案级通信通道台账项 */
struct CommChannel {
    ObjectId id;            ///< 通道 id
    std::string name;       ///< 显示名
    CommChannelType type;   ///< 类型
    bool enabled;           ///< 是否启用
    ParamMap params;        ///< 类型相关参数
    CommChannel()
        : type(CommChannelTypeUndefined)
        , enabled(true)
    {}
};

/** @brief 整数转通道类型；越界回串口 */
VISION_COMM_API CommChannelType commChannelTypeFromInt(int v);
VISIONCORE_API /**
 * @brief 通道类型显示名
 * 声明在这里，实现在 VisionCore：文案走翻译，不链进本库
 */
std::string commChannelTypeTitle(CommChannelType t);

}

#endif
