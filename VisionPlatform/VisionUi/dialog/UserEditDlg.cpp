/**
 * @file UserEditDlg.cpp
 * @brief UserEditDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "UserEditDlg.h"
#include "AuditLog.h"
#include "QtConv.h"
#include "Session.h"
#include "Theme.h"
#include "UiText.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace Vision {

// 添加或编辑用户：口令可留空
UserEditDlg::UserEditDlg(Session* session, const QString& loginName, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_editLogin(loginName.toLower())
    , m_isNew(loginName.isEmpty())
{
    setWindowTitle(m_isNew ? T("添加用户") : T("编辑用户"));
    setModal(true);
    resize(400, 280);

    m_login = new QLineEdit(this);
    m_display = new QLineEdit(this);
    m_role = new QComboBox(this);
    m_role->addItem(T("操作员"), RoleOperator);
    m_role->addItem(T("工程师"), RoleEngineer);
    m_role->addItem(T("管理员"), RoleAdmin);
    m_pwd = new QLineEdit(this);
    m_pwd2 = new QLineEdit(this);
    m_pwd->setEchoMode(QLineEdit::Password);
    m_pwd2->setEchoMode(QLineEdit::Password);
    m_enabled = new QCheckBox(T("启用此账户"), this);
    m_enabled->setChecked(true);

    if (!m_isNew) {
        const UserAccount* u = m_session->users().findByLogin(ss(m_editLogin));
        if (u) {
            m_login->setText(qs(u->loginName));
            m_display->setText(qs(u->displayName));
            m_role->setCurrentIndex(static_cast<int>(u->role));
            m_enabled->setChecked(u->enabled);
        }
        m_login->setReadOnly(true);
        m_pwd->setPlaceholderText(T("留空则不改口令"));
        m_pwd2->setPlaceholderText(T("留空则不改口令"));
    } else {
        m_pwd->setPlaceholderText(T("留空则无口令"));
        m_pwd2->setPlaceholderText(T("留空则无口令"));
    }

    QFormLayout* form = new QFormLayout();
    form->addRow(T("登录名"), m_login);
    form->addRow(T("显示名"), m_display);
    form->addRow(T("角色"), m_role);
    form->addRow(m_isNew ? T("初始口令") : T("新口令"), m_pwd);
    form->addRow(T("确认口令"), m_pwd2);
    form->addRow(m_enabled);

    QLabel* hint = new QLabel(T("口令可留空。用户可自行修改口令。"), this);
    hint->setWordWrap(true);
    hint->setObjectName(QLatin1String("hint"));

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(T("保存"));
    setButtonRole(box->button(QDialogButtonBox::Ok), "primary");
    connect(box, SIGNAL(accepted()), this, SLOT(onOk()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(form);
    lay->addWidget(hint);
    lay->addWidget(box);
}

// 对话框中的登录名（小写）
QString UserEditDlg::loginName() const
{
    return m_login->text().trimmed().toLower();
}

// 校验并创建/更新用户，禁止禁用自己和取消末个管理员
void UserEditDlg::onOk()
{
    const QString login = m_login->text().trimmed().toLower();
    if (login.isEmpty()) {
        QMessageBox::warning(this, T("用户"), T("登录名不能为空。"));
        return;
    }
    QString display = m_display->text().trimmed();
    if (display.isEmpty()) {
        display = login;
    }
    const UserRole role = static_cast<UserRole>(m_role->currentData().toInt());
    const QString p1 = m_pwd->text();
    const QString p2 = m_pwd2->text();
    if (p1 != p2) {
        QMessageBox::warning(this, T("用户"), T("两次口令不一致。"));
        return;
    }

    if (m_isNew) {
        // 新建用户并落盘
        UserAccount u = AuthMgr::makeUser(ss(login), ss(display), role, ss(p1));
        u.enabled = m_enabled->isChecked();
        if (!m_session->users().addUser(u)) {
            QMessageBox::warning(this, T("用户"), T("登录名已存在。"));
            return;
        }
        m_session->persistUsers();
        auditLog("user_create", m_session->user().loginName, ss(login));
        accept();
        return;
    }

    UserAccount* u = m_session->users().findByLogin(ss(m_editLogin));
    if (!u) {
        reject();
        return;
    }
    if (u->id == m_session->user().id && !m_enabled->isChecked()) {
        QMessageBox::warning(this, T("用户"), T("不能禁用当前登录账户。"));
        return;
    }
    if (u->role == RoleAdmin && role != RoleAdmin
        && m_session->users().countRole(RoleAdmin, true) <= 1 && u->enabled) {
        QMessageBox::warning(this, T("用户"), T("不能取消最后一个启用的管理员。"));
        return;
    }
    u->displayName = ss(display);
    u->role = role;
    u->enabled = m_enabled->isChecked();
    if (!p1.isEmpty()) {
        m_session->users().setPassword(u, ss(p1));
    }
    m_session->persistUsers();
    if (u->id == m_session->user().id) {
        m_session->refreshCurrentUser();
    }
    auditLog("user_edit", m_session->user().loginName, ss(login));
    accept();
}

}

#include "moc_UserEditDlg.cpp"
