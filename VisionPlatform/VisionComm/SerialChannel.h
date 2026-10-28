/**
 * @file SerialChannel.h
 * @brief SerialChannel头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SERIAL_CHANNEL_H
#define VISION_SERIAL_CHANNEL_H
#include "VisionCommExport.h"

#include "CommWin.h"
#include "ICommChannel.h"
#include <string>

namespace Vision {

/** @class SerialChannel @brief Win32 串口。同一时刻一个对端，peerId 忽略 */
class VISION_COMM_API SerialChannel : public ICommChannel {
public:
    /** @brief 构造未打开的串口 */
    SerialChannel();
    /** @brief 析构时关闭串口 */
    ~SerialChannel();

    /** @brief 传输种类为串口 */
    CommTransport transport() const;
    /** @brief 按端口、波特率等参数打开 */
    bool open(const ParamMap& params, std::string* err);
    /** @brief 关闭句柄 */
    void close();
    /** @brief 句柄是否有效 */
    bool isOpen() const;
    /** @brief 已打开时给一个空 id 的对端 */
    std::vector<CommPeer> peers() const;
    /** @brief 写出整段字节，忽略对端 id */
    bool write(const std::string& peerId, const CommBytes& data, int timeoutMs, std::string* err);
    /** @brief 按超时读一批，忽略对端 id */
    CommBytes read(const std::string& peerId, int maxBytes, int timeoutMs, std::string* err);
    /** @brief 清空接收缓冲 */
    void drain(const std::string& peerId);
    /** @brief 串口不区分接入端，始终返回空 */
    std::string waitIncoming(int timeoutMs);

    /** @brief 本机串口名，如 COM3 */
    static std::vector<std::string> listPorts();

private:
    /** @brief 把这一次读写的超时写到串口 */
    void applyTimeouts(int timeoutMs);
    HANDLE m_handle; ///< INVALID_HANDLE_VALUE 表示未打开
};

}

#endif
