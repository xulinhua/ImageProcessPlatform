/**
 * @file PermDlg.cpp
 * @brief PermDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PermDlg.h"
#include "AuditLog.h"
#include "PermGate.h"
#include "PermMgr.h"
#include "QtConv.h"
#include "Session.h"
#include "Solution.h"
#include "ToolBase.h"
#include "ToolRegistry.h"
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
#include <algorithm>

namespace Vision {

namespace {

const int kKindRole = Qt::UserRole + 1;
const int kLockRole = Qt::UserRole + 2;
const int kKindNode = 1;

bool columnLocked(QTreeWidgetItem* item, int col)
{
    if (!item || col < 1) {
        return false;
    }
    const int bits = item->data(0, kLockRole).toInt();
    return (bits & (1 << (col - 1))) != 0;
}

}

PermDlg::PermDlg(Session* session, Solution* solution, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_solution(solution)
    , m_tree(nullptr)
    , m_guard(false)
    , m_resetNodes(false)
{
    setWindowTitle(T("控件权限"));
    setModal(true);
    resize(860, 640);

    QLabel* hint = new QLabel(
        T("按页面或工具展开后勾选。勾选上层时，下属控件一起变。管理员的「用户管理」「系统设置」「日志」不可关闭。"
          "流程工具按当前设置：统一管控按工具种类，实例管控按已打开方案里的流程分开。"),
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
    connect(m_tree, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(onItemChanged(QTreeWidgetItem*,int)));

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

QTreeWidgetItem* PermDlg::addCheckRow(QTreeWidgetItem* parent, const QString& title, const QString& tip,
                                       Perm perm, const QString& nodeKey)
{
    QTreeWidgetItem* row = new QTreeWidgetItem(parent);
    row->setText(0, title);
    row->setToolTip(0, tip);
    row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    row->setData(0, Qt::UserRole, nodeKey);
    row->setData(0, kKindRole, kKindNode);
    int locks = 0;
    const AppSettings& settings = m_session->settings();
    for (int c = 0; c < 3; ++c) {
        const UserRole role = static_cast<UserRole>(c);
        bool on = true;
        if (m_resetNodes) {
            on = perm != PermNone ? permDefaultOn(role, perm) : true;
        } else if (!nodeKey.isEmpty()) {
            on = settings.nodeGranted(role, ss(nodeKey), 0);
        }
        const bool locked = perm != PermNone && permNodeLocked(role, perm);
        if (locked) {
            on = true;
            locks |= (1 << c);
            row->setToolTip(c + 1, T("此项不可关闭"));
        }
        row->setCheckState(c + 1, on ? Qt::Checked : Qt::Unchecked);
    }
    row->setData(0, kLockRole, locks);
    return row;
}

// 重建权限树；useDefaults 为 true 时按出厂默认勾选并按工具共享模式列出
void PermDlg::loadTree(bool useDefaults)
{
    m_guard = true;
    m_resetNodes = useDefaults;
    m_tree->clear();
    // 功能权限：每页一个父节点，子节点为该页权限；列 1~3 对应三种角色
    for (int p = 0; p < permPageCount(); ++p) {
        const PermPageDesc page = permPageAt(p);
        QTreeWidgetItem* parent = new QTreeWidgetItem(m_tree);
        parent->setText(0, QString::fromUtf8(page.title.c_str()));
        parent->setToolTip(0, QString::fromUtf8(page.hint.c_str()));
        parent->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        parent->setData(0, Qt::UserRole, qs(permPageNodeId(p)));
        parent->setData(0, kKindRole, kKindNode);
        QFont f = parent->font(0);
        f.setBold(true);
        parent->setFont(0, f);
        for (int i = 0; i < page.itemCount; ++i) {
            const Perm perm = page.items[i];
            addCheckRow(parent, QString::fromUtf8(permissionTitle(perm).c_str()),
                        QString::fromUtf8(permissionHint(perm).c_str()),
                        perm, qs(permNodeId(perm)));
        }
        if (parent->childCount() > 0) {
            for (int c = 1; c <= 3; ++c) {
                refreshColumnUp(parent->child(0), c);
            }
        }
        parent->setExpanded(true);
    }

    // 参数权限：共享模式按工具种类列字段；实例模式按工站/流程/工具实例列字段
    const bool instance = !useDefaults && m_session->settings().permScope == PermScopeInstance;
    QTreeWidgetItem* tools = new QTreeWidgetItem(m_tree);
    tools->setText(0, instance ? T("流程实例") : T("流程工具"));
    tools->setToolTip(0, instance
        ? T("不同流程里的同一个工具，参数分开勾选。")
        : T("同一种工具共用一套参数权限。"));
    tools->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    tools->setData(0, Qt::UserRole, instance ? qs(PermGate::instRoot()) : qs(PermGate::toolRoot()));
    tools->setData(0, kKindRole, kKindNode);
    QFont tf = tools->font(0);
    tf.setBold(true);
    tools->setFont(0, tf);
    if (!instance) {
        std::vector<ToolBase*> protos = ToolRegistry::instance().all();
        std::sort(protos.begin(), protos.end(), [](ToolBase* a, ToolBase* b) {
            if (!a || !b) {
                return a != 0;
            }
            return a->displayName() < b->displayName();
        });
        for (size_t i = 0; i < protos.size(); ++i) {
            ToolBase* proto = protos[i];
            if (!proto) {
                continue;
            }
            QTreeWidgetItem* tool = addCheckRow(tools, QString::fromUtf8(proto->displayName().c_str()),
                                                 QString::fromUtf8(proto->toolName().c_str()),
                                                 PermNone, qs(PermGate::toolNode(proto->toolName())));
            const std::vector<ParamField> fields = proto->paramFields();
            for (size_t f = 0; f < fields.size(); ++f) {
                addCheckRow(tool, T(fields[f].title.c_str()), qs(fields[f].key),
                            PermNone, qs(PermGate::toolField(proto->toolName(), fields[f].key)));
            }
            if (tool->childCount() > 0) {
                for (int c = 1; c <= 3; ++c) {
                    refreshColumnUp(tool->child(0), c);
                }
            }
        }
    } else if (m_solution) {
        for (size_t s = 0; s < m_solution->stations.size(); ++s) {
            const StationCfg& st = m_solution->stations[s];
            for (size_t j = 0; j < st.jobs.size(); ++j) {
                const JobCfg& job = st.jobs[j];
                QTreeWidgetItem* flow = addCheckRow(
                    tools, QString::fromUtf8(st.name.c_str()) + QLatin1String(" / ") + QString::fromUtf8(job.name.c_str()),
                    qs(job.id.toString()), PermNone, QString());
                for (size_t t = 0; t < job.tools.size(); ++t) {
                    const ToolCfg& tool = job.tools[t];
                    QTreeWidgetItem* row = addCheckRow(flow, QString::fromUtf8(tool.name.c_str()),
                                                       qs(tool.toolName), PermNone,
                                                       qs(PermGate::instNode(job.id.toString(), tool.id.toString())));
                    ToolBase* proto = ToolRegistry::instance().find(tool.toolName);
                    if (!proto) {
                        continue;
                    }
                    const std::vector<ParamField> fields = proto->paramFields();
                    for (size_t f = 0; f < fields.size(); ++f) {
                        addCheckRow(row, T(fields[f].title.c_str()), qs(fields[f].key), PermNone,
                                    qs(PermGate::instField(job.id.toString(), tool.id.toString(), fields[f].key)));
                    }
                    if (row->childCount() > 0) {
                        for (int c = 1; c <= 3; ++c) {
                            refreshColumnUp(row->child(0), c);
                        }
                    }
                }
                if (flow->childCount() > 0) {
                    for (int c = 1; c <= 3; ++c) {
                        refreshColumnUp(flow->child(0), c);
                    }
                }
            }
        }
    }
    // 有子节点时由子节点推出根勾选，否则直接读根节点授权
    if (tools->childCount() > 0) {
        for (int c = 1; c <= 3; ++c) {
            refreshColumnUp(tools->child(0), c);
        }
    } else {
        for (int c = 1; c <= 3; ++c) {
            const bool on = m_resetNodes ? true : m_session->settings().nodeGranted(
                static_cast<UserRole>(c - 1), ss(tools->data(0, Qt::UserRole).toString()), 0);
            tools->setCheckState(c, on ? Qt::Checked : Qt::Unchecked);
        }
    }
    tools->setExpanded(true);
    m_tree->resizeColumnToContents(0);
    m_guard = false;
}

void PermDlg::setColumnDown(QTreeWidgetItem* item, int col, Qt::CheckState state)
{
    if (!item) {
        return;
    }
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem* child = item->child(i);
        if (!columnLocked(child, col)) {
            child->setCheckState(col, state);
        }
        setColumnDown(child, col, state);
    }
}

void PermDlg::refreshColumnUp(QTreeWidgetItem* item, int col)
{
    if (!item) {
        return;
    }
    QTreeWidgetItem* parent = item->parent();
    if (!parent) {
        return;
    }
    int on = 0;
    int off = 0;
    for (int i = 0; i < parent->childCount(); ++i) {
        if (parent->child(i)->checkState(col) == Qt::Checked) {
            ++on;
        } else {
            ++off;
        }
    }
    if (off == 0) {
        parent->setCheckState(col, Qt::Checked);
    } else if (on == 0) {
        parent->setCheckState(col, Qt::Unchecked);
    } else {
        parent->setCheckState(col, Qt::PartiallyChecked);
    }
    refreshColumnUp(parent, col);
}

void PermDlg::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (m_guard || !item || column < 1) {
        return;
    }
    if (columnLocked(item, column)) {
        m_guard = true;
        item->setCheckState(column, Qt::Checked);
        m_guard = false;
        return;
    }
    m_guard = true;
    if (item->childCount() > 0 && item->checkState(column) != Qt::PartiallyChecked) {
        setColumnDown(item, column, item->checkState(column));
    }
    refreshColumnUp(item, column);
    m_guard = false;
}

void PermDlg::collectNodes(QTreeWidgetItem* item, std::vector<AppSettings::PermNodeGrant>* out) const
{
    if (!item || !out) {
        return;
    }
    if (item->data(0, kKindRole).toInt() == kKindNode) {
        const QString key = item->data(0, Qt::UserRole).toString();
        if (!key.isEmpty()) {
            AppSettings::PermNodeGrant g;
            g.id = ss(key);
            g.op = item->checkState(1) != Qt::Unchecked;
            g.eng = item->checkState(2) != Qt::Unchecked;
            g.admin = item->checkState(3) != Qt::Unchecked;
            out->push_back(g);
        }
    }
    for (int i = 0; i < item->childCount(); ++i) {
        collectNodes(item->child(i), out);
    }
}

void PermDlg::onReset()
{
    m_resetNodes = true;
    loadTree(true);
}

void PermDlg::onSave()
{
    if (m_resetNodes) {
        m_session->settings().clearNodeGrants();
    }
    std::vector<AppSettings::PermNodeGrant> nodes;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        collectNodes(m_tree->topLevelItem(i), &nodes);
    }
    for (size_t i = 0; i < nodes.size(); ++i) {
        m_session->settings().setNodeGrant(nodes[i].id, nodes[i].op, nodes[i].eng, nodes[i].admin);
    }
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
