/**
 * @file ChangePwdDlg.h
 * @brief ChangePwdDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CHANGEPASSWORDDIALOG_H
#define VISION_CHANGEPASSWORDDIALOG_H
#include "VisionUiExport.h"

#include <QDialog>
#include <QString>

class QLineEdit;

namespace Vision {

class Session;

/**
 * @class ChangePwdDlg
 * @brief 修改口令或管理员重置口令。口令可为空
 */
class VISION_UI_API ChangePwdDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造改密对话框
     * @param loginName 目标登录名
     * @param requireOld true 须校验当前口令（自助改密）
     * @param resetByAdmin true 为管理员重置
     */
    ChangePwdDlg(Session* session, const QString& loginName, bool requireOld,
                         QWidget* parent = 0, bool resetByAdmin = false);

private slots:
    /** @brief 校验两次新口令一致后写入并保存 */
    void onOk();

private:
    Session* m_session;     ///< 会话（不拥有）
    QString m_login;        ///< 目标登录名
    bool m_requireOld;     ///< 是否要求输入当前口令
    bool m_resetByAdmin;    ///< 是否管理员重置
    QLineEdit* m_old;     ///< 当前口令；不要求时为 0
    QLineEdit* m_n1;      ///< 新口令
    QLineEdit* m_n2;      ///< 确认新口令
};

}

#endif
