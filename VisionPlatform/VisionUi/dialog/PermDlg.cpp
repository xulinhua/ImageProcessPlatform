/**
 * @file PermDlg.cpp
 * @brief PermDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PermDlg.h"
#include "AuditLog.h"
#include "PermMgr.h"
#include <stdint.h>
#include "QtConv.h"
#include "Session.h"
#include "VisionCfgMgr.h"
#include "Theme.h"
#include "UiText.h"
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace Vision {

// 控件权限：页面分组 + 三角色勾选
PermDlg::PermDlg(Session* session, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
{
    setWindowTitle(T("控件权限"));
    setModal(true);
    resize(780, 560);

    QLabel* hint = new QLabel(
        T("按页面展开后勾选该页下的控件。管理员的「用户管理」「系统设置」「日志」不可关闭，以免把自己锁在外面。"),
        this);
    hint->setWordWrap(true);
    hint->setObjectName(QLatin1String("hint"));

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    QStringList h;
    h << T("页面 / 控件") << T("操作员") << T("工程师") << T("管理员");
    m_tree->setHeaderLabels(h);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setItemsExpandable(true);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QPushButton* reset = new QPushButton(T("恢复默认"), this);
    connect(reset, SIGNAL(clicked()), this, SLOT(onReset()));
    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Save)->setText(T("保存"));
    setButtonRole(box->button(QDialogButtonBox::Save), "primary");
    connect(box, SIGNAL(accepted()), this, SLOT(onSave()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    QHBoxLayout* bot = new QHBoxLayout();
    bot->addWidget(reset);
    bot->addStretch();
    bot->addWidget(box);

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addWidget(hint);
    lay->addWidget(m_tree, 1);
    lay->addLayout(bot);

    loadTree(false);
}

// 落盘并记操作日志
void PermDlg::loadTree(bool useDefaults)
{
    m_tree->clear();
    quint32 masks[3];
    if (useDefaults) {
        masks[0] = defaultPerms(RoleOperator);
        masks[1] = defaultPerms(RoleEngineer);
        masks[2] = defaultPerms(RoleAdmin);
    } else {
        uint32_t dbMask = 0;
        for (int c = 0; c < 3; ++c) {
            const UserRole role = static_cast<UserRole>(c);
            if (PermMgr::instance().loadRoleMask(role, &dbMask, 0)) {
                masks[c] = dbMask;
            } else {
                masks[c] = m_session->settings().permMask(role);
            }
        }
    }
    for (int p = 0; p < permPageCount(); ++p) {
        const PermPageDesc page = permPageAt(p);
        QTreeWidgetItem* parent = new QTreeWidgetItem(m_tree);
        parent->setText(0, QString::fromUtf8(page.title.c_str()));
        parent->setToolTip(0, QString::fromUtf8(page.hint.c_str()));
        parent->setFirstColumnSpanned(true);
        QFont f = parent->font(0);
        f.setBold(true);
        parent->setFont(0, f);
        parent->setFlags(Qt::ItemIsEnabled);
        for (int i = 0; i < page.itemCount; ++i) {
            const Perm perm = page.items[i];
            QTreeWidgetItem* row = new QTreeWidgetItem(parent);
            row->setText(0, QString::fromUtf8(permissionTitle(perm).c_str()));
            row->setToolTip(0, QString::fromUtf8(permissionHint(perm).c_str()));
            row->setData(0, Qt::UserRole, static_cast<int>(perm));
            row->setFlags(Qt::ItemIsEnabled);
            for (int c = 0; c < 3; ++c) {
                const UserRole role = static_cast<UserRole>(c);
                const bool on = (masks[c] & static_cast<quint32>(perm)) != 0;
                const bool locked = (lockedPerms(role) & static_cast<quint32>(perm)) != 0;
                row->setCheckState(c + 1, on ? Qt::Checked : Qt::Unchecked);
                if (locked) {
                    row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
                    row->setToolTip(c + 1, T("此项不可关闭"));
                } else {
                    row->setFlags(row->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                }
            }
        }
        parent->setExpanded(true);
    }
    m_tree->resizeColumnToContents(0);
}

// 落盘并记操作日志
quint32 PermDlg::maskFromColumn(int col) const
{
    quint32 m = 0;
    for (int p = 0; p < m_tree->topLevelItemCount(); ++p) {
        QTreeWidgetItem* parent = m_tree->topLevelItem(p);
        if (!parent) {
            continue;
        }
        for (int i = 0; i < parent->childCount(); ++i) {
            QTreeWidgetItem* row = parent->child(i);
            if (!row) {
                continue;
            }
            if (row->checkState(col) == Qt::Checked) {
                m |= static_cast<quint32>(row->data(0, Qt::UserRole).toInt());
            }
        }
    }
    return m;
}

// 落盘并记操作日志
void PermDlg::onReset()
{
    loadTree(true);
}

// 落盘并记操作日志
void PermDlg::onSave()
{
    m_session->settings().setPermMask(RoleOperator, maskFromColumn(1));
    m_session->settings().setPermMask(RoleEngineer, maskFromColumn(2));
    m_session->settings().setPermMask(RoleAdmin, maskFromColumn(3));
    if (!VisionCfgMgr::saveSettings(m_session->settings())) {
        QMessageBox::warning(this, T("控件权限"), T("保存失败。"));
        return;
    }
    auditLog("perm_save", m_session->user().loginName, std::string());
    m_session->reloadPerms();
    m_session->notifyChanged();
    accept();
}

}

#include "moc_PermDlg.cpp"
