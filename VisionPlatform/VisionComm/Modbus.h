/**
 * @file Modbus.h
 * @brief Modbus头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_MODBUS_H
#define VISION_MODBUS_H
#include "VisionCommExport.h"

#include "ICommChannel.h"
#include <atomic>
#include <map>
#include <mutex>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

namespace Vision {

/** @brief Modbus 数据区 */
enum ModbusTable {
    ModbusCoil = 0,      ///< 线圈，读写
    ModbusDiscrete = 1,  ///< 离散输入，主站只读
    ModbusHolding = 2,   ///< 保持寄存器，读写
    ModbusInput = 3      ///< 输入寄存器，主站只读
};

/** @brief 主站一次事务。tcp 为真时走 MBAP，否则 RTU。tid 由调用方递增 */
VISION_COMM_API bool modbusMasterReadBits(ICommChannel* ch, const std::string& peerId, bool tcp, uint16_t tid,
                          int slave, int fc, int address, int count,
                          std::vector<unsigned char>* bits, std::string* err);
/** @brief 主站读保持寄存器或输入寄存器 */
VISION_COMM_API bool modbusMasterReadRegs(ICommChannel* ch, const std::string& peerId, bool tcp, uint16_t tid,
                          int slave, int fc, int address, int count,
                          std::vector<uint16_t>* regs, std::string* err);
/** @brief 主站写线圈；一个走功能码 5，多个走 15 */
VISION_COMM_API bool modbusMasterWriteBits(ICommChannel* ch, const std::string& peerId, bool tcp, uint16_t tid,
                           int slave, int address, const std::vector<unsigned char>& bits, std::string* err);
/** @brief 主站写寄存器；一个走功能码 6，多个走 16 */
VISION_COMM_API bool modbusMasterWriteRegs(ICommChannel* ch, const std::string& peerId, bool tcp, uint16_t tid,
                           int slave, int address, const std::vector<uint16_t>& regs, std::string* err);

/**
 * @class ModbusSlave
 * @brief 从站。后台收请求并应答，映像可被本机读写
 */
class VISION_COMM_API ModbusSlave {
public:
    /** @brief 分配四块映像 */
    ModbusSlave();
    /** @brief 析构时停止应答 */
    ~ModbusSlave();

    /** @brief 挂到通道上并开始应答 */
    void start(ICommChannel* channel, int slaveId);
    /** @brief 停止应答并卸下通道 */
    void stop();

    /** @brief 读本机位映像 */
    bool readBits(ModbusTable table, int address, int count, std::vector<unsigned char>* bits, std::string* err);
    /** @brief 读本机寄存器映像 */
    bool readRegs(ModbusTable table, int address, int count, std::vector<uint16_t>* regs, std::string* err);
    /** @brief 写本机位映像 */
    bool writeBits(ModbusTable table, int address, const std::vector<unsigned char>& bits, std::string* err);
    /** @brief 写本机寄存器映像 */
    bool writeRegs(ModbusTable table, int address, const std::vector<uint16_t>& regs, std::string* err);

private:
    /** @brief 收请求并回包，直到 stop */
    void loop();
    /** @brief 按功能码改映像并组应答；广播不回包 */
    CommBytes reply(const CommBytes& unitAndPdu);
    /** @brief 拆掉 MBAP 后处理，再套回事务号 */
    CommBytes onTcp(const CommBytes& frame);
    /** @brief 校验 CRC 后处理，再补上 CRC */
    CommBytes onRtu(const CommBytes& frame);

    ICommChannel* m_ch; ///< 所挂通道，不拥有
    int m_slaveId; ///< 本机从站号
    std::atomic<bool> m_run; ///< 应答线程是否继续
    std::thread m_thread; ///< 收请求并应答
    std::mutex m_mu; ///< 保护映像
    std::vector<unsigned char> m_coils; ///< 线圈映像
    std::vector<unsigned char> m_discrete; ///< 离散输入映像
    std::vector<uint16_t> m_holding; ///< 保持寄存器映像
    std::vector<uint16_t> m_input; ///< 输入寄存器映像
    std::map<std::string, CommBytes> m_rx; ///< TCP 粘包缓冲，按对端
    static const int kMapSize = 1024; ///< 映像长度
};

}

#endif
