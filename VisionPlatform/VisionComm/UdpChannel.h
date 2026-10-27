/**
 * @file UdpChannel.h
 * @brief UdpChannel头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_UDP_CHANNEL_H
#define VISION_UDP_CHANNEL_H
#include "VisionCommExport.h"

#include "CommWin.h"
#include "ICommChannel.h"
#include <string>

namespace Vision {

/** @class UdpChannel @brief UDP。绑定本机端口，写发往 params 里的对端 */
class VISION_COMM_API UdpChannel : public ICommChannel {
public:
    /** @brief 构造未绑定的套接字 */
    UdpChannel();
    /** @brief 析构时关闭套接字 */
    ~UdpChannel();

    /** @brief 传输种类为 UDP */
    CommTransport transport() const;
    /** @brief 绑定本机端口，并记下对端 */
    bool open(const ParamMap& params, std::string* err);
    /** @brief 关闭套接字 */
    void close();
    /** @brief 套接字是否有效 */
    bool isOpen() const;
    /** @brief 已配置对端时返回其地址 */
    std::vector<CommPeer> peers() const;
    /** @brief 发往打开时记下的对端 */
    bool write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err);
    /** @brief 收一包；来源记成下一次发送目标 */
    CommBytes read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err);
    /** @brief 把接收队列读空 */
    void drain(const std::string& peerId);
    /** @brief UDP 不区分接入端，始终返回空 */
    std::string waitIncoming(int timeoutMs);

private:
    SOCKET m_sock;           ///< 绑定套接字
    sockaddr_in m_remote;    ///< 发送目标
    bool m_hasRemote;        ///< 是否已有对端
    std::string m_remoteText; ///< 对端地址文本
};

}

#endif
