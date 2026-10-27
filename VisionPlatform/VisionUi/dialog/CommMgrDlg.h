/**
 * @file CommMgrDlg.h
 * @brief CommMgrDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMMMANAGERDIALOG_H
#define VISION_COMMMANAGERDIALOG_H
#include "VisionUiExport.h"

#include "CommTypes.h"
#include <QDialog>
#include <QString>

class QListWidget;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QPlainTextEdit;
class QLabel;
class QStackedWidget;
class QWidget;

namespace Vision {

class Session;
struct Solution;

/**
 * @class CommMgrDlg
 * @brief 方案级通信管理：串口、TCP 客户端/服务端、UDP，以及 Modbus 主从测试
 */
class VISION_UI_API CommMgrDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 会话与方案均不拥有 */
    CommMgrDlg(Session* session, Solution* solution, QWidget* parent = 0);

private slots:
    /** @brief 新增原始串口通道 */
    void onAddSerial();
    /** @brief 新增 TCP 客户端通道 */
    void onAddTcpClient();
    /** @brief 新增 TCP 服务端通道 */
    void onAddTcpServer();
    /** @brief 新增 UDP 通道 */
    void onAddUdp();
    /** @brief 新增 Modbus RTU 通道 */
    void onAddModbusRtu();
    /** @brief 新增 Modbus TCP 客户端通道 */
    void onAddModbusTcp();
    /** @brief 确认后删除当前通道并断开 */
    void onRemove();
    /** @brief 选中通道后把参数灌入右侧 */
    void onSel();
    /** @brief 把右侧参数写回当前通道 */
    void onApplyFields();
    /** @brief 保存参数并打开当前通道 */
    void onConnect();
    /** @brief 关闭当前通道 */
    void onDisconnect();
    /** @brief 按原始字节发送并尝试读回 */
    void onSend();
    /** @brief 按当前区与地址读 Modbus */
    void onModbusRead();
    /** @brief 按当前区与地址写 Modbus */
    void onModbusWrite();
    /** @brief 重列本机串口 */
    void onRefreshPorts();
    /** @brief 重列 TCP 服务端已接入的对端 */
    void onRefreshPeers();
    /** @brief 协议切换后更新类型标签与测试区 */
    void onProtocolChanged();

private:
    /** @brief 按方案通道重建左侧列表 */
    void refreshList();
    /**
     * @brief 按类型追加一条通道并选中
     * @param nameKey 名称格式，含 {0}
     */
    void addChannel(CommChannelType type, const char* nameKey, bool modbus);
    /** @brief 当前列表选中的通道 id */
    QString currentId() const;
    /** @brief TCP 服务端当前对端 id，其它类型为空 */
    std::string currentPeerId() const;
    /** @brief 当前选中的通道，没有则为空 */
    CommChannel* currentChannel();
    /** @brief 把通道参数灌进右侧控件 */
    void loadFields(const CommChannel& ch);
    /** @brief 把右侧控件写回通道 */
    void saveFieldsTo(CommChannel* ch);
    /** @brief 按通道类型切换右侧参数页 */
    void syncTypePage(CommChannelType t);
    /** @brief 按通道类型和协议显示对应的参数与测试区 */
    void syncActionPanel();

    Session* m_session; ///< 会话（不拥有）
    Solution* m_sol; ///< 方案（不拥有）
    QListWidget* m_list; ///< 通道列表
    QLineEdit* m_name; ///< 通道名称
    QLabel* m_type;                  ///< 类型，只展示
    QLabel* m_protocolLabel;         ///< 「协议」标签，UDP 时隐藏
    QComboBox* m_protocol; ///< 原始或 Modbus 主从
    QStackedWidget* m_stack; ///< 按通道类型切换的参数页
    QComboBox* m_serialPort; ///< 串口号
    QSpinBox* m_baud; ///< 波特率
    QSpinBox* m_dataBits; ///< 数据位
    QComboBox* m_parity; ///< 校验位
    QSpinBox* m_stopBits; ///< 停止位
    QLineEdit* m_host; ///< TCP 客户端主机
    QSpinBox* m_tcpPort; ///< TCP 客户端端口
    QSpinBox* m_listenPort; ///< TCP 服务端监听端口
    QComboBox* m_peers; ///< 已接入对端
    QSpinBox* m_udpBind; ///< UDP 本地端口
    QLineEdit* m_udpHost; ///< UDP 对端主机
    QSpinBox* m_udpRemote; ///< UDP 对端端口
    QComboBox* m_mbArea; ///< Modbus 寄存器区
    QSpinBox* m_mbSlave; ///< 从站号
    QSpinBox* m_mbAddr; ///< 起始地址
    QSpinBox* m_mbCount; ///< 读写数量
    QSpinBox* m_mbValue; ///< 写入值
    QWidget* m_rawPanel;             ///< 原始字节收发
    QWidget* m_modbusPanel;          ///< Modbus 读写
    QPlainTextEdit* m_io; ///< 收发记录
    QLineEdit* m_sendEdit; ///< 待发送文本
    QLabel* m_status; ///< 连接状态
};

}

#endif
