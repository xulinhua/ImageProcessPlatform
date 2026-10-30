/**
 * @file UserMgrDlg.cpp
 * @brief UserMgrDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "UserMgrDlg.h"
#include "FactoryAccount.h"
#include "AuditLog.h"
#include "ChangePwdDlg.h"
#include "QtConv.h"
#include "Session.h"
#include "Theme.h"
#include "UiText.h"
#include "UserEditDlg.h"
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Vision {

// 用户列表：添加、编辑、删除、重置口令、启用/禁用
UserMgrDlg::UserMgrDlg(Session* session, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
{
    setWindowTitle(T("用户管理"));
    resize(820, 480);

    QLabel* hint = new QLabel(T("本地账户，与方案文件无关。管理员可添加、编辑、禁用、删除用户并重置口令。"), this);
    hint->setWordWrap(true);
    hint->setObjectName(QLatin1String("hint"));

    // 用户表，双击编辑
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    QStringList headers;
    headers << T("登录名") << T("显示名") << T("角色") << T("启用") << T("上次登录");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    connect(m_table, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onEdit()));

    QPushButton* addBtn = new QPushButton(T("添加"), this);
    QPushButton* editBtn = new QPushButton(T("编辑"), this);
    QPushButton* delBtn = new QPushButton(T("删除"), this);
    QPushButton* resetBtn = new QPushButton(T("重置口令"), this);
    QPushButton* enBtn = new QPushButton(T("启用/禁用"), this);
    setButtonRole(addBtn, "primary");
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAdd()));
    connect(editBtn, SIGNAL(clicked()), this, SLOT(onEdit()));
    connect(delBtn, SIGNAL(clicked()), this, SLOT(onDelete()));
    connect(resetBtn, SIGNAL(clicked()), this, SLOT(onResetPwd()));
    connect(enBtn, SIGNAL(clicked()), this, SLOT(onToggleEnabled()));

    // 无用户管理权限时只能查看
    const bool can = m_session->has(PermManageUsers);
    addBtn->setEnabled(can);
    editBtn->setEnabled(can);
    delBtn->setEnabled(can);
    resetBtn->setEnabled(can);
    enBtn->setEnabled(can);

    QHBoxLayout* opLay = new QHBoxLayout();
    opLay->addWidget(addBtn);
    opLay->addWidget(editBtn);
    opLay->addWidget(delBtn);
    opLay->addWidget(resetBtn);
    opLay->addWidget(enBtn);
    opLay->addStretch();

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addWidget(hint);
    lay->addWidget(m_table, 1);
    lay->addLayout(opLay);

    refresh();
}

// 当前选中行的登录名
QString UserMgrDlg::selectedLogin() const
{
    const int r = m_table->currentRow();
    if (r < 0 || !m_table->item(r, 0)) {
        return QString();
    }
    return m_table->item(r, 0)->text();
}

// 刷新用户表并尽量保持原选中行
void UserMgrDlg::refresh()
{
    const QString keep = selectedLogin();
    m_table->setRowCount(0);
    const AuthMgr& us = m_session->users();
    int keepRow = -1;
    for (int i = 0; i < us.count(); ++i) {
        const UserAccount& u = us.at(i);
        if (u.role == RoleFactory || FactoryAccount::ownsName(u.loginName)) {
            continue;
        }
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(qs(u.loginName)));
        m_table->setItem(r, 1, new QTableWidgetItem(qs(u.displayName)));
        m_table->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(roleTitle(u.role).c_str())));
        m_table->setItem(r, 3, new QTableWidgetItem(u.enabled ? T("是") : T("否")));
        m_table->setItem(r, 4, new QTableWidgetItem(
                             u.lastLoginAt.empty() ? QLatin1String("-") : qs(u.lastLoginAt)));
        if (u.loginName == ss(keep)) {
            keepRow = r;
        }
    }
    if (keepRow >= 0) {
        m_table->selectRow(keepRow);
    }
}

// 打开添加用户对话框
void UserMgrDlg::onAdd()
{
    UserEditDlg dlg(m_session, QString(), this);
    if (dlg.exec() == QDialog::Accepted) {
        refresh();
    }
}

// 编辑选中用户
void UserMgrDlg::onEdit()
{
    const QString login = selectedLogin();
    if (login.isEmpty()) {
        return;
    }
    UserEditDlg dlg(m_session, login, this);
    if (dlg.exec() == QDialog::Accepted) {
        refresh();
    }
}

// 删除用户：禁止删自己和末个启用管理员
void UserMgrDlg::onDelete()
{
    const QString login = selectedLogin();
    UserAccount* u = m_session->users().findByLogin(ss(login));
    if (!u) {
        return;
    }
    if (u->id == m_session->user().id) {
        QMessageBox::warning(this, T("用户"), T("不能删除当前登录账户。"));
        return;
    }
    if (u->role == RoleAdmin && m_session->users().countRole(RoleAdmin, true) <= 1 && u->enabled) {
        QMessageBox::warning(this, T("用户"), T("不能删除最后一个启用的管理员。"));
        return;
    }
    if (QMessageBox::question(this, T("删除"), T("删除用户 ") + login + T(" ？")) != QMessageBox::Yes) {
        return;
    }
    m_session->users().removeById(u->id);
    m_session->persistUsers();
    auditLog("user_delete", m_session->user().loginName, ss(login));
    refresh();
}

// 管理员重置选中用户口令
void UserMgrDlg::onResetPwd()
{
    const QString login = selectedLogin();
    if (login.isEmpty()) {
        return;
    }
    ChangePwdDlg dlg(m_session, login, false, this, true);
    if (dlg.exec() == QDialog::Accepted) {
        refresh();
    }
}

// 切换启用状态，规则同删除保护
void UserMgrDlg::onToggleEnabled()
{
    const QString login = selectedLogin();
    UserAccount* u = m_session->users().findByLogin(ss(login));
    if (!u) {
        return;
    }
    if (u->id == m_session->user().id) {
        QMessageBox::warning(this, T("用户"), T("不能禁用当前登录账户。"));
        return;
    }
    if (u->enabled && u->role == RoleAdmin && m_session->users().countRole(RoleAdmin, true) <= 1) {
        QMessageBox::warning(this, T("用户"), T("不能禁用最后一个启用的管理员。"));
        return;
    }
    u->enabled = !u->enabled;
    m_session->persistUsers();
    refresh();
}

}

#include "moc_UserMgrDlg.cpp"
