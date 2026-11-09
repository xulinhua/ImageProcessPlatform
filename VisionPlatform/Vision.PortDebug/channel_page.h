/**
 * @file channel_page.h
 * @brief 八种通道的参数页
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 参数排成同一行：标签在上，输入在下。打开后锁住，串口的 DTR/RTS 和 UDP 的远程地址除外。
 */

#ifndef HS_COMM_DEBUG_CHANNEL_PAGE_H
#define HS_COMM_DEBUG_CHANNEL_PAGE_H

#include "CommTypes.h"

#include <QWidget>

/** @brief 参数页。收集打开所需的通道配置，并给出状态栏用的摘要 */
class ChannelPage : public QWidget {
public:
    explicit ChannelPage(QWidget* parent = 0) : QWidget(parent) {}
    /** @brief 写入类型和参数。id 由会话填 */
    virtual void fill(Vision::CommChannel* channel) const = 0;
    /** @brief 原始字节流。HTTP、WebSocket、Modbus 为 false */
    virtual bool stream() const { return true; }
    virtual bool webSocket() const { return false; }
    virtual bool http() const { return false; }
    virtual bool modbus() const { return false; }
    virtual bool udp() const { return false; }
    virtual bool server() const { return false; }
    /** @brief 串口和 Modbus RTU 才有 DTR/RTS */
    virtual bool serialLines() const { return false; }
    /** @brief 状态栏中间那段，例如 COM3  9600 8N1 */
    virtual QString summary() const { return QString(); }
    /** @brief 打开后锁住参数。串口线电平和 UDP 目标保持可改 */
    virtual void setLocked(bool) {}
    virtual bool dtr() const { return true; }
    virtual bool rts() const { return true; }
    virtual QString udpHost() const { return QString(); }
    virtual int udpPort() const { return 0; }
};

ChannelPage* createSerialPage(QWidget* parent);
ChannelPage* createTcpClientPage(QWidget* parent);
ChannelPage* createTcpServerPage(QWidget* parent);
ChannelPage* createUdpPage(QWidget* parent);
ChannelPage* createHttpPage(QWidget* parent);
ChannelPage* createWebSocketPage(QWidget* parent);
ChannelPage* createModbusRtuPage(QWidget* parent);
ChannelPage* createModbusTcpPage(QWidget* parent);

#endif
