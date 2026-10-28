/**
 * @file SettingsDlg.h
 * @brief SettingsDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SETTINGSDIALOG_H
#define VISION_SETTINGSDIALOG_H
#include "VisionUiExport.h"

#include <QDialog>

class QListWidget;
class QStackedWidget;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QGroupBox;
class QPushButton;

namespace Vision {

class Session;

/**
 * @class SettingsDlg
 * @brief 本机设置中心：左导航分类 + 右内容页（常规/界面/路径/运行日志）
 */
class VISION_UI_API SettingsDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造设置中心
     * @param session 会话（不拥有）
     */
    explicit SettingsDlg(Session* session, QWidget* parent = 0);

private slots:
    /** @brief 左侧分类切换 */
    void onCategoryChanged(int row);
    /** @brief 应用写盘，不关窗 */
    void onApply();
    /** @brief 应用并关闭 */
    void onOk();
    /** @brief 打开数据相关目录 */
    void onOpenDataDir();
    /** @brief 在资源管理器中打开方案目录 */
    void onOpenSolutionsDir();
    /** @brief 跳转日志管控 */
    void onOpenLogDlg();

private:
    /** @brief 从 Session 填充控件 */
    void loadFromSettings();
    /** @brief 控件 → AppSettings 并写盘、应用主题/语言 */
    bool applySettings(bool interactivePrompt);
    /** @brief 语言热切换后刷新本对话框文案 */
    void retranslateUi();
    /** @brief 按路径打开资源管理器 */
    void openPath(const QString& path);

    Session* m_session;          ///< 会话
    QListWidget* m_nav;          ///< 左导航
    QStackedWidget* m_stack;     ///< 右内容
    QCheckBox* m_autoOp;         ///< 自动登录操作员
    QComboBox* m_startupMode;    ///< 启动默认模式
    QComboBox* m_closeAction;    ///< 关闭主窗口行为
    QComboBox* m_theme;          ///< 主题
    QComboBox* m_lang;           ///< 语言
    QLineEdit* m_dataDir;        ///< 数据目录
    QLineEdit* m_dbFile;         ///< 数据库 vision.db
    QLineEdit* m_solsDir;        ///< 方案目录
    QLineEdit* m_lastSol;        ///< 上次方案
    QLabel* m_titleLabel;        ///< 无边框标题
    QLabel* m_uiHint;            ///< 外观页提示
    QLabel* m_closeHint;         ///< 关闭行为提示
    QLabel* m_pathHint;          ///< 路径页提示
    QLabel* m_runHint;           ///< 运行页提示
    QGroupBox* m_popupBox;       ///< 流程试跑弹窗
    QCheckBox* m_popupJobOk;     ///< OK 时弹窗
    QCheckBox* m_popupJobNg;     ///< NG 时弹窗
    QCheckBox* m_popupJobEx;     ///< 异常时弹窗
    QGroupBox* m_generalBox;     ///< 启动与关闭
    QPushButton* m_btnOk;        ///< 确定
    QPushButton* m_btnCancel;    ///< 取消
    QPushButton* m_btnApply;     ///< 应用
    QPushButton* m_btnOpenLogsDlg; ///< 打开日志管控
    QPushButton* m_btnOpenLogsFolder; ///< 打开日志目录
    bool m_canEdit;              ///< 是否可编辑
};

}

#endif
