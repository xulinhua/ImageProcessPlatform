/**
 * @file UserMgrDlg.h
 * @brief UserMgrDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_USERMANAGERDIALOG_H
#define VISION_USERMANAGERDIALOG_H
#include "VisionUiExport.h"

#include <QDialog>

class QTableWidget;

namespace Vision {

class Session;

/**
 * @class UserMgrDlg
 * @brief 本地用户管理：添加、编辑、删除、重置口令、启用/禁用。与方案文件无关
 */
class VISION_UI_API UserMgrDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造用户管理对话框
     * @param session 会话（不拥有），提供 AuthMgr 与权限
     */
    explicit UserMgrDlg(Session* session, QWidget* parent = 0);

private slots:
    /** @brief 从 AuthMgr 重填用户表，尽量保持当前选中行 */
    void refresh();
    /** @brief 弹出 UserEditDlg 添加用户 */
    void onAdd();
    /** @brief 编辑当前选中用户 */
    void onEdit();
    /** @brief 删除选中用户（不可删当前登录/末个启用管理员） */
    void onDelete();
    /** @brief 管理员重置选中用户口令 */
    void onResetPwd();
    /** @brief 切换选中用户启用态 */
    void onToggleEnabled();

private:
    /**
     * @brief 当前选中行的登录名
     * @return 无选中时为空
     */
    QString selectedLogin() const;

    Session* m_session;      ///< 会话（不拥有）
    QTableWidget* m_table;  ///< 用户列表
};

}

#endif
