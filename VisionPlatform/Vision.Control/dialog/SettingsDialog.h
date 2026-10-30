/**
 * @file SettingsDialog.h
 * @brief 设置：常规、网络、画面、安全、录屏、日志
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_SETTINGS_DIALOG_H
#define VISION_CONTROL_SETTINGS_DIALOG_H

#include <QDialog>
#include <functional>

class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QVBoxLayout;

namespace Vision {

class ControlConfig;
class SwitchButton;

/**
 * @class SettingsDialog
 * @brief 控制端设置对话框：左侧导航六页，编辑 ControlConfig 的字段
 * @note 只在「应用」「确定」时写回并保存配置；未实现的设置项只占位置灰
 */
class SettingsDialog : public QDialog {
public:
    /** @brief applied 在应用或确定后调用，参数为监听地址或端口是否变了 */
    SettingsDialog(ControlConfig* config, bool listening, const QString& auditDir,
        const std::function<void(bool)>& applied, QWidget* parent = 0);

    /** @brief 打开时选中的页 */
    void setPage(int page);

private:
    /************************************************** 页面 **************************************************/
    /** @brief 常规：开机自启、启动隐藏、关闭到托盘、控制端显示名 */
    QWidget* buildGeneral();
    /** @brief 网络：监听地址端口与当前状态、局域网发现；中继、连接顺序、带宽为占位 */
    QWidget* buildNetwork(bool listening);
    /** @brief 画面：默认适应窗口、默认布局、轮巡间隔、截图目录、当前编码说明 */
    QWidget* buildPicture();
    /** @brief 安全：全部为占位说明 */
    QWidget* buildSecurity();
    /** @brief 录屏：全部为占位 */
    QWidget* buildRecord();
    /** @brief 日志：审计保留天数与目录 */
    QWidget* buildLog(const QString& auditDir);

    /************************************************** 布局 **************************************************/
    /** @brief 新建一页白底容器，通过 col 返回其竖排布局 */
    QWidget* page(QVBoxLayout** col);
    /** @brief 在页内加一个带标题的分组，返回两列网格 */
    QGridLayout* group(QVBoxLayout* col, const QString& title);
    /** @brief 网格末尾加一行「标签 值控件」 */
    void addRow(QGridLayout* grid, const QString& key, QWidget* value);
    /** @brief 把至多四个控件横排成一行，右侧留伸展 */
    QWidget* line(QWidget* a, QWidget* b = 0, QWidget* c = 0, QWidget* d = 0);

    /************************************************** 读写 **************************************************/
    /** @brief 把配置填进控件；「恢复默认」时传默认构造的配置 */
    void load(const ControlConfig& config);
    /** @brief 控件值写回配置（地址留空按 0.0.0.0），保存、应用自启，再回调 applied */
    void store();

    ControlConfig* m_config;                ///< 被编辑的配置，不拥有
    std::function<void(bool)> m_applied;    ///< 应用回调，参数为监听地址或端口是否变了，可空
    QListWidget* m_nav;                     ///< 左侧导航，行号即页号

    SwitchButton* m_tray;           ///< 关闭时最小化到托盘
    SwitchButton* m_hidden;         ///< 启动时隐藏到托盘
    SwitchButton* m_autoStart;      ///< 开机自动启动
    QLineEdit* m_host;              ///< 监听地址
    QSpinBox* m_port;               ///< 监听端口 1~65535
    SwitchButton* m_discovery;      ///< 局域网发现
    SwitchButton* m_fit;            ///< 会话窗口默认适应窗口
    QSpinBox* m_rows;               ///< 监控墙默认行数 1~4
    QSpinBox* m_cols;               ///< 监控墙默认列数 1~4
    QSpinBox* m_tour;               ///< 轮巡间隔 3~600 秒
    SwitchButton* m_relayOn;        ///< 启用中继
    QLineEdit* m_relayHost;         ///< 中继地址
    QSpinBox* m_relayPort;          ///< 中继端口
    QLineEdit* m_relayKey;          ///< 中继口令
    QLineEdit* m_shotDir;           ///< 截图保存目录
    SwitchButton* m_autoRecord;     ///< 取得操作时自动录制
    QLineEdit* m_recordDir;         ///< 录屏目录
    QSpinBox* m_recordDays;         ///< 录屏保留天数
    QSpinBox* m_auditDays;          ///< 审计保留 7~3650 天
};

}

#endif
