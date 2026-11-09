/**
 * @file AgentService.h
 * @brief 被控端 Windows 服务：守护会话代理、转发 Ctrl+Alt+Del
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_AGENT_SERVICE_H
#define VISION_CONTROLLED_AGENT_SERVICE_H

#include <string>

namespace Vision {

/**
 * @class AgentService
 * @brief 安装 / 卸载 / 运行服务。服务在会话 0，拉起用户会话里的 RemoteClient.exe --agent
 */
class AgentService {
public:
    static const wchar_t* kName;            ///< 服务名
    static const wchar_t* kDisplay;         ///< 显示名
    static const wchar_t* kPipe;            ///< CAD 命名管道

    /** @brief 安装服务。需要管理员。失败写 err */
    static bool install(std::string* err);
    /** @brief 卸掉服务 */
    static bool uninstall(std::string* err);
    /** @brief 阻塞运行 ServiceMain（供 --service 调用） */
    static int run();
    /** @brief 通过命名管道请求服务发 SAS（代理进程调用） */
    static bool requestCad();
};

}

#endif
