/**
 * @file CommMgr.h
 * @brief CommMgr头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMM_HUB_H
#define VISION_COMM_HUB_H
#include "VisionCommExport.h"

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
class VISION_COMM_API CommMgr {
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
                        int address, int count, std::vector<unsigned char>* bits, std::string* err);
    /** @brief 读保持寄存器或输入寄存器 */
    bool modbusReadRegs(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                        int address, int count, std::vector<uint16_t>* regs, std::string* err);
    /** @brief 写线圈。从站写本机映像 */
    bool modbusWriteBits(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                         int address, const std::vector<unsigned char>& bits, std::string* err);
    /** @brief 写寄存器。从站写本机映像 */
    bool modbusWriteRegs(const ObjectId& id, const std::string& peerId, int slave, ModbusTable table,
                         int address, const std::vector<uint16_t>& regs, std::string* err);

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
