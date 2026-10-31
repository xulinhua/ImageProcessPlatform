/**
 * @file AddDeviceDialog.h
 * @brief 添加设备：局域网扫描、按 IP 地址、按设备 ID（中继）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_ADD_DEVICE_DIALOG_H
#define VISION_CONTROL_ADD_DEVICE_DIALOG_H

#include "Messages.h"
#include <QDialog>
#include <functional>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabBar;
class QTableWidget;

namespace Vision {

class DeviceStore;
class ControlConfig;

/**
 * @class AddDeviceDialog
 * @brief 添加设备对话框：从局域网扫描结果勾选，或手填 IP 按桌面共享登记
 * @note 只读设备库，不写入；确认后由调用方取 picks() 和 group() 自行入库
 */
class AddDeviceDialog : public QDialog {
public:
    /** @brief 一台待添加的设备 */
    struct Pick {
        QString key;        ///< 设备库 key（同 DeviceStore::keyFor）
        QString name;       ///< 被控端上报的机器名，手填 IP 时为空
        QString alias;      ///< 手填 IP 时用户填的名称，可空；扫描添加时为空
        QString address;    ///< IP 地址
        QString system;     ///< 系统版本，手填 IP 时为空
        QString deviceId;   ///< 按设备 ID 添加时的 ID
    };

    /**
     * @brief 搭界面，打开后立即扫描一次
     * @param store 设备库，用来标出已添加的，不拥有
     * @param scanner 执行一次局域网扫描并返回结果
     * @param group 默认选中的分组，不存在时选未分组
     */
    AddDeviceDialog(const DeviceStore* store, const ControlConfig* config,
        const std::function<std::vector<HelloMsg>()>& scanner, const QString& group, QWidget* parent = 0);

    /** @brief 当前页签下选中的设备：扫描页为勾选项，IP 页为合法且未登记的那一个 */
    std::vector<Pick> picks() const;
    /** @brief 选中的分组名，空为未分组 */
    QString group() const;

private:
    /************************************************** 搭建 **************************************************/
    /** @brief 扫描页：筛选框、重新扫描、结果表 */
    QWidget* buildScan();
    /** @brief IP 页：名称、IP 地址 */
    QWidget* buildAddress();
    /** @brief 设备 ID 页，占位空白 */
    QWidget* buildId();

    /************************************************** 动作 **************************************************/
    /** @brief 调 scanner 扫描（阻塞，显示等待光标），更新提示并重填表格 */
    void rescan();
    /** @brief 按筛选重填结果表；已添加的行置灰且不可勾选 */
    void fill();
    /** @brief 按选中数更新底部计数与「添加」按钮 */
    void syncFoot();
    /** @brief 发现结果对应的设备库 key */
    static QString keyOf(const HelloMsg& hello);

    const DeviceStore* m_store;                         ///< 设备库，只读，不拥有
    std::function<std::vector<HelloMsg>()> m_scanner;   ///< 扫描函数，可空
    std::vector<HelloMsg> m_found;                      ///< 最近一次扫描结果
    QTabBar* m_tabs;                                    ///< 页签：扫描、IP、设备 ID（禁用）
    QLineEdit* m_filter;                                ///< 扫描结果筛选（名称或 IP）
    QLabel* m_hint;                                     ///< 扫描状态提示
    QTableWidget* m_table;                              ///< 扫描结果表，第 0 列勾选框的 UserRole 存 m_found 下标
    QLineEdit* m_ipName;                                ///< IP 页：名称
    QLineEdit* m_ip;                                    ///< IP 页：IPv4 地址
    QLineEdit* m_deviceId;                              ///< 设备 ID 页
    const ControlConfig* m_config;                      ///< 中继设置，不拥有
    QComboBox* m_group;                                 ///< 加入分组，data 为分组名
    QLabel* m_count;                                    ///< 底部计数或提示
    QPushButton* m_ok;                                  ///< 添加按钮，无可添加项时禁用
};

}

#endif
