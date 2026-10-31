/**
 * @file DeviceDialog.h
 * @brief 设备属性：常规、连接、画面（下发共享设置）、统计
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_DEVICE_DIALOG_H
#define VISION_CONTROL_DEVICE_DIALOG_H

#include "ControlSession.h"
#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QRadioButton;
class QWidget;

namespace Vision {

class AuditLog;
class DeviceStore;

/**
 * @class DeviceDialog
 * @brief 设备属性对话框：改名称/分组/备注/标签，查看连接信息，给在线设备下发共享范围，看近 30 天统计
 * @note 状态取打开时的快照，对话框打开期间不刷新；共享范围点「下发」即生效，其余在「确定」时写回
 */
class DeviceDialog : public QDialog {
public:
    /** @brief tile 为空表示设备不在线 */
    DeviceDialog(DeviceStore* store, AuditLog* audit, ControlSession* session, const QString& key,
        const ControlSession::Tile* tile, QWidget* parent = 0);

    /** @brief 确定时写回设备列表 */
    void accept() override;

private:
    /************************************************** 搭建 **************************************************/
    /** @brief 常规页：名称、设备 ID、分组、备注、标签 */
    QWidget* buildGeneral();
    /** @brief 连接页：地址、状态、共享、延迟、最后在线等只读信息 */
    QWidget* buildConnection();
    /** @brief 画面页：桌面（选显示器）或窗口（标题/进程名），离线时全部禁用 */
    QWidget* buildPicture();
    /** @brief 统计页：从审计日志统计近 30 天的会话、时长、操作、被拒、截图 */
    QWidget* buildStats();
    /** @brief 新建一页白底容器，通过 grid 返回其两列网格 */
    QWidget* page(QGridLayout** grid);
    /** @brief 网格加一行「右对齐标签 值控件」 */
    void addRow(QGridLayout* grid, int row, const QString& key, QWidget* value);
    /** @brief 把至多三个控件横排成一行，右侧留伸展 */
    QWidget* line(QWidget* parent, QWidget* a, QWidget* b = 0, QWidget* c = 0);

    /************************************************** 动作 **************************************************/
    /** @brief 按 m_tags 重建标签按钮：点标签删除，「+ 标签」新增 */
    void rebuildTags();
    /** @brief 校验并下发共享配置，记审计；窗口共享时标题和进程名至少填一个 */
    void applyPicture();

    DeviceStore* m_store;           ///< 设备库，不拥有
    AuditLog* m_audit;              ///< 审计日志，不拥有
    ControlSession* m_session;      ///< 收发会话，不拥有
    QString m_key;                  ///< 设备 key
    ControlSession::Tile m_tile;    ///< 打开时的状态快照，不含 JPEG；离线且无快照时为默认值
    bool m_online;                  ///< 打开时设备在线
    QStringList m_tags;             ///< 编辑中的标签，确定时写回

    QLineEdit* m_name;              ///< 名称；与上报名相同或留空时清掉别名
    QComboBox* m_group;             ///< 分组，data 为分组名，空为未分组
    QLineEdit* m_remark;            ///< 备注
    QLineEdit* m_mac;               ///< 网卡 MAC
    QHBoxLayout* m_tagRow;          ///< 标签按钮所在布局
    QWidget* m_tagBox;              ///< 标签区容器
    QRadioButton* m_linkAuto;       ///< 连接：跟全局顺序
    QRadioButton* m_linkLan;        ///< 连接：仅直连
    QRadioButton* m_linkRelay;      ///< 连接：仅中继
    QRadioButton* m_linkReverse;    ///< 连接：反连优先
    QLineEdit* m_password;          ///< 无人值守密码
    QCheckBox* m_rememberPassword;  ///< 记住密码
    QComboBox* m_quality;           ///< 打开会话默认画质
    QComboBox* m_highestPrivilege;  ///< 最高权限：跟上级 / 开 / 关

    QRadioButton* m_desktop;        ///< 共享整个桌面
    QRadioButton* m_window;         ///< 共享指定窗口
    QComboBox* m_monitor;           ///< 显示器序号，桌面共享时可用
    QLineEdit* m_title;             ///< 窗口标题包含的文字
    QLineEdit* m_process;           ///< 进程名
    QLabel* m_pictureHint;          ///< 画面页提示或下发结果
};

}

#endif
