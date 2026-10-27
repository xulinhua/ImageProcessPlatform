/**
 * @file ChangePwdDlg.cpp
 * @brief ChangePwdDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ChangePwdDlg.h"
#include "AuditLog.h"
#include "QtConv.h"
#include "Session.h"
#include "UiText.h"
#include "Theme.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

namespace Vision {

// 改密/管理员重置口令对话框
ChangePwdDlg::ChangePwdDlg(Session* session, const QString& loginName, bool requireOld,
                                           QWidget* parent, bool resetByAdmin)
    : QDialog(parent)
    , m_session(session)
    , m_login(loginName)
    , m_requireOld(requireOld)
    , m_resetByAdmin(resetByAdmin)
    , m_old(nullptr)
{
    setWindowTitle(resetByAdmin ? T("重置口令") : T("修改口令"));
    setModal(true);
    QFormLayout* form = new QFormLayout();
    if (requireOld) {
        m_old = new QLineEdit(this);
        m_old->setEchoMode(QLineEdit::Password);
        form->addRow(T("当前口令"), m_old);
    }
    m_n1 = new QLineEdit(this);
    m_n2 = new QLineEdit(this);
    m_n1->setEchoMode(QLineEdit::Password);
    m_n2->setEchoMode(QLineEdit::Password);
    form->addRow(T("新口令"), m_n1);
    form->addRow(T("确认新口令"), m_n2);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, SIGNAL(accepted()), this, SLOT(onOk()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(form);
    lay->addWidget(box);
}

// 校验新旧口令并写回用户库
void ChangePwdDlg::onOk()
{
    if (m_n1->text() != m_n2->text()) {
        QMessageBox::warning(this, T("口令"), T("两次新口令不一致。"));
        return;
    }
    UserAccount* u = m_session->users().findByLogin(ss(m_login));
    if (!u) {
        reject();
        return;
    }
    if (m_requireOld && !m_session->users().verifyPassword(*u, ss(m_old->text()))) {
        QMessageBox::warning(this, T("口令"), T("当前口令不正确。"));
        return;
    }
    m_session->users().setPassword(u, ss(m_n1->text()));
    m_session->persistUsers();
    if (m_session->isLoggedIn() && m_session->user().loginName == ss(m_login)) {
        m_session->refreshCurrentUser();
    }
    auditLog(m_resetByAdmin ? "user_reset_pwd" : "user_change_pwd",
             ss(m_login), std::string());
    accept();
}

}

#include "moc_ChangePwdDlg.cpp"
