/**
 * @file channel_page.h
 * @brief 八种通道的参数页
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 主窗口只向当前页要一份 CommChannel。换类型时换页，不改接收区。
 */

#ifndef HS_COMM_DEBUG_CHANNEL_PAGE_H
#define HS_COMM_DEBUG_CHANNEL_PAGE_H

#include "CommTypes.h"

#include <QWidget>

/** @brief 参数页。收集打开所需的通道配置 */
class ChannelPage : public QWidget {
public:
    explicit ChannelPage(QWidget* parent = 0) : QWidget(parent) {}
    /** @brief 写入类型和参数。id 由会话填 */
    virtual void fill(Vision::CommChannel* channel) const = 0;
    /** @brief 原始字节流。HTTP 和 WebSocket 为 false */
    virtual bool stream() const { return true; }
    virtual bool webSocket() const { return false; }
    virtual bool http() const { return false; }
    virtual bool modbus() const { return false; }
    virtual void refreshPorts() {}
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
