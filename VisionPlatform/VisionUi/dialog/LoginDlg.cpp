/**
 * @file LoginDlg.cpp
 * @brief LoginDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LoginDlg.h"
#include "QtConv.h"
#include "Session.h"
#include "VisionCfgMgr.h"
#include "Theme.h"
#include "UiText.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Vision {

// 普通登录框
LoginDlg::LoginDlg(Session* session, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_fails(0)
{
    buildUi();
}

// 提权登录：校验所需权限后再放行
LoginDlg::LoginDlg(Session* session, const std::string& neededPerm, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_fails(0)
    , m_neededPerm(neededPerm)
{
    buildUi();
}

// 搭登录表单；提权时改标题与按钮
void LoginDlg::buildUi()
{
    setWindowTitle(m_neededPerm.empty() ? T("用户登录") : T("提权登录"));
    setModal(true);
    resize(400, 220);

    QLabel* hint = new QLabel(
        m_neededPerm.empty()
            ? T("请输入登录名与口令。")
            : T("当前操作需要更高权限，请输入有权限的账户。"),
        this);
    hint->setWordWrap(true);
    hint->setObjectName(QLatin1String("hint"));

    m_user = new QLineEdit(this);
    m_pwd = new QLineEdit(this);
    m_pwd->setEchoMode(QLineEdit::Password);
    const std::string last = m_session->settings().lastLoginName;
    m_user->setText(last.empty() ? QLatin1String("engineer") : qs(last));

    m_err = new QLabel(this);
    m_err->setWordWrap(true);
    m_err->setStyleSheet(QLatin1String("color:#FF4D4F;"));

    QFormLayout* form = new QFormLayout();
    form->addRow(T("登录名"), m_user);
    form->addRow(T("口令"), m_pwd);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(m_neededPerm.empty() ? T("登录") : T("提权"));
    box->button(QDialogButtonBox::Cancel)->setText(T("取消"));
    setButtonRole(box->button(QDialogButtonBox::Ok), "primary");
    connect(box, SIGNAL(accepted()), this, SLOT(onOk()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addWidget(hint);
    lay->addLayout(form);
    lay->addWidget(m_err);
    lay->addWidget(box);

    if (last.empty()) {
        m_user->setFocus();
    } else {
        m_pwd->setFocus();
    }
}

// 去掉空白后的登录名
QString LoginDlg::loginName() const
{
    return m_user->text().trimmed();
}

// 登录或提权；失败则累计次数
void LoginDlg::onOk()
{
    std::string err;
    const std::string name = ss(m_user->text().trimmed());
    const std::string pwd = ss(m_pwd->text());
    bool ok = false;
    if (!m_neededPerm.empty()) {
        ok = m_session->elevate(name, pwd, m_neededPerm, &err);
    } else {
        ok = m_session->login(name, pwd, &err);
    }
    if (!ok) {
        ++m_fails;
        m_err->setText(qs(err) + T("（已失败 ") + QString::number(m_fails) + T(" 次）"));
        m_pwd->selectAll();
        m_pwd->setFocus();
        return;
    }
    if (m_neededPerm.empty()) {
        m_session->settings().lastLoginName = name;
        VisionCfgMgr::saveSettings(m_session->settings());
    }
    accept();
}

}

#include "moc_LoginDlg.cpp"
