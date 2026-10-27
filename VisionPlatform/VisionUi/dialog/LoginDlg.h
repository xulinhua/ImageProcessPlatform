/**
 * @file LoginDlg.h
 * @brief LoginDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LOGINDIALOG_H
#define VISION_LOGINDIALOG_H
#include "VisionUiExport.h"

#include <QDialog>
#include <QString>
#include <string>

class QLineEdit;
class QLabel;

namespace Vision {

class Session;

/**
 * @class LoginDlg
 * @brief 用户登录。口令可为空
 */
class VISION_UI_API LoginDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造登录对话框，预填上次登录名
     * @param session 会话（不拥有）
     */
    explicit LoginDlg(Session* session, QWidget* parent = 0);
    /**
     * @brief 提权登录，账户须具备所需权限
     * @param neededPerm 权限键
     */
    LoginDlg(Session* session, const std::string& neededPerm, QWidget* parent = 0);
    /** @brief 尝试登录；失败累计次数并显示错误 */
    QString loginName() const;

private slots:
    /** @brief 尝试登录；失败累计次数并显示错误 */
    void onOk();

private:
    /** @brief 搭登录表单；有所需权限时改为提权文案 */
    void buildUi();
    Session* m_session;  ///< 会话（不拥有）
    QLineEdit* m_user;  ///< 登录名
    QLineEdit* m_pwd;   ///< 口令
    QLabel* m_err;      ///< 错误提示
    int m_fails;        ///< 连续失败次数
    std::string m_neededPerm; ///< 提权所需权限，空表示普通登录
};

}

#endif
