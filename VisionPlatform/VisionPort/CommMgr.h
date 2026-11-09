/**
 * @file CommMgr.h
 * @brief CommMgr头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PORT_HUB_H
#define VISION_PORT_HUB_H
#include "VisionPortExport.h"

#include "CommTypes.h"
#include "ICommChannel.h"
#include "Modbus.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CommMgr
 * @brief 通信通道管理。按台账创建传输，Modbus 主从挂在通道上
 */
class VISION_PORT_API CommMgr {
public:
    /** @brief 进程内唯一通信管理 */
    static CommMgr& instance();

    /** @brief 本机串口名 */
    std::vector<std::string> listSerialPorts() const;

    /** @brief 按通道配置打开；已打开则先关再开 */
    bool openChannel(const CommChannel& ch, std::string* err);
    /** @brief 关闭通道 */
    void closeChannel(const ObjectId& id);
    /** @brief 是否已打开。服务端表示正在监听 */
    bool isOpen(const ObjectId& id) const;
    /** @brief 当前对端。TCP 服务端为已接入的客户端 */
    std::vector<CommPeer> peers(const ObjectId& id) const;

    /**
     * @brief 写原始字节
     * @param peerId TCP 服务端填写客户端 id，其它通道留空
     */
    bool writeBytes(const ObjectId& id, const std::string& peerId, const CommBytes& data, std::string* err);
    /** @brief 读原始字节 */
    CommBytes readBytes(const ObjectId& id, const std::string& peerId, int maxBytes, int timeoutMs, std::string* err);

    /** @brief 读线圈或离散输入。从站读本机映像，主站发总线请求 */
    bool modbusReadBits(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                        int address, int count, std::vector<unsigned char>* bits, std::string* err,
                        CommBytes* sent = 0, CommBytes* received = 0);
    /** @brief 读保持寄存器或输入寄存器 */
    /**
     * @brief 读保持寄存器或输入寄存器
     * @param sent 可空。主站实际发出的整帧
     * @param received 可空。主站收到的整帧
     */
    bool modbusReadRegs(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                        int address, int count, std::vector<uint16_t>* regs, std::string* err,
                        CommBytes* sent = 0, CommBytes* received = 0);
    /** @brief 写线圈。从站写本机映像 */
    bool modbusWriteBits(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                         int address, const std::vector<unsigned char>& bits, std::string* err,
                         CommBytes* sent = 0, CommBytes* received = 0);
    /** @brief 写寄存器。从站写本机映像 */
    /**
     * @brief 写寄存器。从站写本机映像
     * @param sent 可空。主站实际发出的整帧
     * @param received 可空。主站收到的整帧
     */
    bool modbusWriteRegs(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                         int address, const std::vector<uint16_t>& regs, std::string* err,
                         CommBytes* sent = 0, CommBytes* received = 0);

    /** @brief 关掉一个对端的连接。客户端忽略 peerId，只断当前套接字 */
    void closePeer(const ObjectId& id, const std::string& peerId);
    /** @brief 打开后改串口 DTR、RTS。非串口通道失败 */
    bool setSerialLines(const ObjectId& id, bool dtr, bool rts, std::string* err);
    /** @brief 打开后改 UDP 发送目标。非 UDP 通道失败 */
    bool setUdpRemote(const ObjectId& id, const std::string& host, int port, std::string* err);
    /** @brief 最近一包 UDP 的来源。不是 UDP 或还没收到则为空 */
    std::string udpLastSource(const ObjectId& id) const;
    /** @brief 等到任一端点有数据，返回端点 id。服务端以外为空 */
    std::string waitIncoming(const ObjectId& id, int timeoutMs);

    /**
     * @brief 在已打开的 HTTP 通道上发一次请求
     * @param path 为空则用打开时 url 里的路径
     * @param status 返回 HTTP 状态码
     * @param response 返回正文
     */
    /**
     * @brief 在已打开的 HTTP 通道上发一次请求
     * @param headers 额外请求头。Content-Type 仍走 contentType
     */
    bool httpExchange(const ObjectId& id, const std::string& method, const std::string& path,
                      const std::string& contentType, const std::string& body,
                      int* status, std::string* response, std::string* err,
                      const std::string& headers = std::string());
    /** @brief 在已打开的 WebSocket 通道上发一条完整消息 */
    bool wsSend(const ObjectId& id, const CommBytes& data, bool text, std::string* err);
    /** @brief 收一条完整 WebSocket 消息。超时返回空，err 为读超时 */
    CommBytes wsRecv(const ObjectId& id, int timeoutMs, bool* text, std::string* err);

private:
    /** @brief 只允许 instance 构造 */
    CommMgr();
    /** @brief 关闭全部已开通道 */
    ~CommMgr();
    /** @brief 禁止拷贝 */
    CommMgr(const CommMgr&);
    /** @brief 禁止赋值 */
    CommMgr& operator=(const CommMgr&);

    /** @class Impl @brief 已打开通道的会话表 */
    struct Impl;
    Impl* m_impl; ///< 会话表
};

}

#endif
