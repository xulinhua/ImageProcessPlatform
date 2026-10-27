# -*- coding: utf-8 -*-
"""Apply GBK-safe patches to source files (project uses GBK, not UTF-8)."""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def read_gbk(path):
    with open(path, 'rb') as f:
        return f.read().decode('gbk')

def write_gbk(path, text):
    with open(path, 'wb') as f:
        f.write(text.encode('gbk'))

def nl(text):
    """Normalize to LF for matching/writing (VS/project mix CRLF and LF)."""
    return text.replace('\r\n', '\n')

def patch_main_cpp():
    path = os.path.join(ROOT, 'VisionApp', 'app', 'main.cpp')
    t = nl(read_gbk(path))
    if 'VisionLog::startup' in t:
        print('skip', path, '(already patched)')
        return
    t = t.replace(
        '#include "RegisterAlgoBase.h"\n#include "RegisterAlgoHalcon.h"\n#include "RegisterGrabFile.h"',
        '#include "RegisterGrabFile.h"\n#include "PluginHost.h"\n#include "VisionLog.h"\n#include "LoginDialog.h"'
    )
    t = t.replace(
        '    Vision::registerAlgoBase();\n    Vision::registerAlgoHalcon();\n    Vision::registerGrabFile();',
        '    Vision::VisionLog::startup();\n    {\n        Vision::PluginHost host;\n        Vision::loadVisionPlugins(&host);\n    }\n    Vision::registerGrabFile();'
    )
    old_login = '''    // \u8c03\u8bd5\u9636\u6bb5\uff1a\u514d\u767b\u5f55\u8fdb\u5165\u7ba1\u7406\u5458\u5e76\u653e\u5f00\u6743\u9650\uff0c\u6062\u590d\u9ed8\u8ba4\u6743\u9650
    bool usersChanged = session.users().ensureBuiltinEmptyPasswords();
    usersChanged = session.users().ensureAdminAccount() || usersChanged;
    if (usersChanged) {
        Vision::VisionConfigMgr::saveUsers(session.users());
    }
    if (!session.enterAdminForDev()) {
        QMessageBox::critical(0,
                              QString::fromLocal8Bit(Vision::Translate::tr("\u7528\u6237\u5e93")),
                              QString::fromLocal8Bit(Vision::Translate::tr("\u65e0\u6cd5\u8fdb\u5165\u7cfb\u7edf\uff1a\u6ca1\u6709\u53ef\u7528\u8d26\u6237\u3002")));
        return 1;
    }
    session.resetDevPermissions();
    Vision::VisionConfigMgr::saveSettings(session.settings());

    Vision::MainWindow w(&session);
    w.show();
    return app.exec();'''
    new_login = '''#ifdef _DEBUG
    bool usersChanged = session.users().ensureBuiltinEmptyPasswords();
    usersChanged = session.users().ensureAdminAccount() || usersChanged;
    if (usersChanged) {
        Vision::VisionConfigMgr::saveUsers(session.users());
    }
    if (!session.enterAdminForDev()) {
        QMessageBox::critical(0,
                              QString::fromLocal8Bit(Vision::Translate::tr("\u7528\u6237\u5e93")),
                              QString::fromLocal8Bit(Vision::Translate::tr("\u65e0\u6cd5\u8fdb\u5165\u7cfb\u7edf\uff1a\u6ca1\u6709\u53ef\u7528\u8d26\u6237\u3002")));
        Vision::VisionLog::shutdown();
        return 1;
    }
    session.resetDevPermissions();
    Vision::VisionConfigMgr::saveSettings(session.settings());
#else
    if (!session.autoLoginOperator()) {
        Vision::LoginDialog loginDlg(&session);
        if (loginDlg.exec() != QDialog::Accepted || !session.isLoggedIn()) {
            Vision::VisionLog::shutdown();
            return 0;
        }
    }
#endif

    Vision::MainWindow w(&session);
    w.show();
    const int rc = app.exec();
    Vision::VisionLog::shutdown();
    return rc;'''
    if old_login not in t:
        # try without comment line match - find by enterAdminForDev block
        start = t.find('    bool usersChanged = session.users().ensureBuiltinEmptyPasswords();')
        end = t.find('    return app.exec();')
        if start < 0 or end < 0:
            raise SystemExit('main.cpp: login block not found')
        t = t[:start] + new_login + t[end + len('    return app.exec();'):]
    else:
        t = t.replace(old_login, new_login)
    write_gbk(path, t)
    print('patched', path)

def patch_mainwindow():
    path = os.path.join(ROOT, 'VisionUi', 'main', 'MainWindow.cpp')
    t = nl(read_gbk(path))
    if 'LoginDialog dlg(m_session' in t:
        print('skip', path, '(already patched)')
        return
    if '#include "LoginDialog.h"' not in t:
        t = t.replace('#include "PermissionDialog.h"\n', '#include "PermissionDialog.h"\n#include "LoginDialog.h"\n')
    old = '''bool MainWindow::ensurePerm(Permission perm, const QString& deniedText)
{
    if (m_session->has(perm)) {
        return true;
    }
    m_session->enterAdminForDev();
    m_session->resetDevPermissions();
    VisionConfigMgr::saveSettings(m_session->settings());
    if (m_session->has(perm)) {
        updateActions();
        updateTitle();
        return true;
    }
    Q_UNUSED(deniedText);
    return true;
}'''
    new = '''bool MainWindow::ensurePerm(Permission perm, const QString& deniedText)
{
    if (m_session->has(perm)) {
        return true;
    }
#ifdef _DEBUG
    m_session->enterAdminForDev();
    m_session->resetDevPermissions();
    VisionConfigMgr::saveSettings(m_session->settings());
    if (m_session->has(perm)) {
        updateActions();
        updateTitle();
        return true;
    }
#else
    LoginDialog dlg(m_session, std::string(permissionId(perm)), this);
    if (dlg.exec() == QDialog::Accepted && m_session->has(perm)) {
        updateActions();
        updateTitle();
        return true;
    }
#endif
    Q_UNUSED(deniedText);
    return m_session->has(perm);
}'''
    if old not in t:
        raise SystemExit('MainWindow.cpp: ensurePerm block not found')
    t = t.replace(old, new)
    write_gbk(path, t)
    print('patched', path)

def patch_logindialog_h():
    path = os.path.join(ROOT, 'VisionUi', 'dialog', 'LoginDialog.h')
    t = nl(read_gbk(path))
    if 'neededPerm' in t:
        print('skip', path, '(already patched)')
        return
    if '#include <string>' not in t:
        t = t.replace('#include <QString>\n', '#include <QString>\n#include <string>\n')
    t = t.replace(
        '    explicit LoginDialog(Session* session, QWidget* parent = 0);\n',
        '    explicit LoginDialog(Session* session, QWidget* parent = 0);\n'
        '    LoginDialog(Session* session, const std::string& neededPerm, QWidget* parent = 0);\n'
    )
    t = t.replace(
        'private:\n    Session* m_session;',
        'private:\n    void buildUi();\n    Session* m_session;'
    )
    t = t.replace(
        '    int m_fails;        ///< \u8fde\u7eed\u5931\u8d25\u6b21\u6570\n};',
        '    int m_fails;        ///< \u8fde\u7eed\u5931\u8d25\u6b21\u6570\n    std::string m_neededPerm;\n};'
    )
    write_gbk(path, t)
    print('patched', path)

def patch_logindialog_cpp():
    path = os.path.join(ROOT, 'VisionUi', 'dialog', 'LoginDialog.cpp')
    t = '''#include "LoginDialog.h"
#include "QtConv.h"
#include "Session.h"
#include "VisionConfigMgr.h"
#include "Theme.h"
#include "UiText.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Vision {

LoginDialog::LoginDialog(Session* session, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_fails(0)
{
    buildUi();
}

LoginDialog::LoginDialog(Session* session, const std::string& neededPerm, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_fails(0)
    , m_neededPerm(neededPerm)
{
    buildUi();
}

void LoginDialog::buildUi()
{
    setWindowTitle(m_neededPerm.empty() ? T("\u7528\u6237\u767b\u5f55") : T("\u63d0\u6743\u767b\u5f55"));
    setModal(true);
    resize(400, 220);

    QLabel* hint = new QLabel(
        m_neededPerm.empty()
            ? T("\u8bf7\u8f93\u5165\u767b\u5f55\u540d\u4e0e\u53e3\u4ee4\u3002")
            : T("\u5f53\u524d\u64cd\u4f5c\u9700\u8981\u66f4\u9ad8\u6743\u9650\uff0c\u8bf7\u8f93\u5165\u6709\u6743\u9650\u7684\u8d26\u6237\u3002"),
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
    form->addRow(T("\u767b\u5f55\u540d"), m_user);
    form->addRow(T("\u53e3\u4ee4"), m_pwd);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(m_neededPerm.empty() ? T("\u767b\u5f55") : T("\u63d0\u6743"));
    box->button(QDialogButtonBox::Cancel)->setText(T("\u53d6\u6d88"));
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

QString LoginDialog::loginName() const
{
    return m_user->text().trimmed();
}

void LoginDialog::onOk()
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
        m_err->setText(qs(err) + T("\uff08\u5df2\u5931\u8d25 ") + QString::number(m_fails) + T(" \u6b21\uff09"));
        m_pwd->selectAll();
        m_pwd->setFocus();
        return;
    }
    if (m_neededPerm.empty()) {
        m_session->settings().lastLoginName = name;
        VisionConfigMgr::saveSettings(m_session->settings());
    }
    accept();
}

}

#include "moc_LoginDialog.cpp"
'''
    write_gbk(path, t)
    print('patched', path)

def patch_permission_dialog():
    path = os.path.join(ROOT, 'VisionUi', 'dialog', 'PermissionDialog.cpp')
    t = nl(read_gbk(path))
    if 'AuthDb::instance().loadRoleMask' in t:
        print('skip', path, '(already patched)')
        return
    if 'AuthDb.h' not in t:
        t = t.replace('#include "AuditLog.h"\n', '#include "AuditLog.h"\n#include "AuthDb.h"\n#include <stdint.h>\n')
    old = '''    } else {
        masks[0] = m_session->settings().permMask(RoleOperator);
        masks[1] = m_session->settings().permMask(RoleEngineer);
        masks[2] = m_session->settings().permMask(RoleAdmin);
    }'''
    new = '''    } else {
        uint32_t dbMask = 0;
        for (int c = 0; c < 3; ++c) {
            const UserRole role = static_cast<UserRole>(c);
            if (AuthDb::instance().loadRoleMask(role, &dbMask, 0)) {
                masks[c] = dbMask;
            } else {
                masks[c] = m_session->settings().permMask(role);
            }
        }
    }'''
    if old in t:
        t = t.replace(old, new)
    old2 = '''    if (!VisionConfigMgr::saveSettings(m_session->settings())) {
        QMessageBox::warning(this, T("\u5173\u952e\u6743\u9650"), T("\u4fdd\u5b58\u5931\u8d25\u3002"));
        return;
    }
    auditLog("perm_save", m_session->user().loginName, std::string());'''
    new2 = '''    if (!VisionConfigMgr::saveSettings(m_session->settings())) {
        QMessageBox::warning(this, T("\u5173\u952e\u6743\u9650"), T("\u4fdd\u5b58\u5931\u8d25\u3002"));
        return;
    }
    AuthDb::instance().saveRoleMask(RoleOperator, m_session->settings().permOperator, 0);
    AuthDb::instance().saveRoleMask(RoleEngineer, m_session->settings().permEngineer, 0);
    AuthDb::instance().saveRoleMask(RoleAdmin, m_session->settings().permAdmin, 0);
    auditLog("perm_save", m_session->user().loginName, std::string());'''
    if old2 in t:
        t = t.replace(old2, new2)
    write_gbk(path, t)
    print('patched', path)

def patch_userstore():
    path = os.path.join(ROOT, 'VisionAuth', 'user', 'UserStore.cpp')
    t = nl(read_gbk(path))
    if 'HashKindPbkdf2' not in t:
        print('skip', path, '(no pbkdf2 block)')
        return
    old = '''    u.hashKind = HashKindPbkdf2;
    u.pwdHash = hashPassword(u.salt, password);
#ifndef _DEBUG
    if (password.empty()) {
        u.mustChangePwd = true;
        u.pwdHash = hashPassword(u.salt, "123456");
    }
#endif
    u.enabled = true;
    u.mustChangePwd = false;'''
    new = '''    u.hashKind = HashKindPbkdf2;
    u.pwdHash = hashPassword(u.salt, password);
    u.enabled = true;
    u.mustChangePwd = false;
#ifndef _DEBUG
    if (password.empty()) {
        u.mustChangePwd = true;
        u.pwdHash = hashPassword(u.salt, "123456");
    }
#endif'''
    if old not in t:
        # try original without pbkdf2 block
        old_orig = '''    u.pwdHash = hashPassword(u.salt, password);
    u.enabled = true;
    u.mustChangePwd = false;'''
        if old_orig in t:
            print('UserStore: makeUser already original, skip')
            return
        raise SystemExit('UserStore.cpp: makeUser block not found')
    t = t.replace(old, new)
    write_gbk(path, t)
    print('patched', path)

if __name__ == '__main__':
    patch_main_cpp()
    patch_mainwindow()
    patch_logindialog_h()
    patch_logindialog_cpp()
    patch_permission_dialog()
    patch_userstore()
    print('done')
