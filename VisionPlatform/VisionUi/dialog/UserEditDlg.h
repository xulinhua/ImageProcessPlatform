/**
 * @file UserEditDlg.h
 * @brief UserEditDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_USEREDITDIALOG_H
#define VISION_USEREDITDIALOG_H
#include "VisionUiExport.h"

#include "Perm.h"
#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QCheckBox;

namespace Vision {

class Session;

/**
 * @class UserEditDlg
 * @brief 添加或编辑本地用户（登录名、显示名、角色、口令、启用）
 */
class VISION_UI_API UserEditDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造用户编辑对话框
     * @param loginName 为空则添加新用户；非空则编辑该登录名
     */
    UserEditDlg(Session* session, const QString& loginName, QWidget* parent = 0);

    /** @brief 表单中的登录名（去空白并转小写） */
    QString loginName() const;

private slots:
    /** @brief 校验并写入 AuthMgr；口令可留空 */
    void onOk();

private:
    Session* m_session;      ///< 会话（不拥有）
    QString m_editLogin;    ///< 编辑目标登录名（新建时为空）
    bool m_isNew;           ///< 是否添加新用户
    QLineEdit* m_login;    ///< 登录名
    QLineEdit* m_display;   ///< 显示名
    QComboBox* m_role;     ///< 角色
    QLineEdit* m_pwd;     ///< 口令
    QLineEdit* m_pwd2;    ///< 确认口令
    QCheckBox* m_enabled;  ///< 是否启用
};

}

#endif
