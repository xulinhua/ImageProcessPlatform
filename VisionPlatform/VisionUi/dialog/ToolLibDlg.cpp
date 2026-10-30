/**
 * @file ToolLibDlg.cpp
 * @brief ToolLibDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolLibDlg.h"
#include "QtConv.h"
#include "UiText.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Vision {

namespace {

const int kNodeRole = Qt::UserRole;
const int kIndexRole = Qt::UserRole + 1;

// 空详情
void clearLabels(QLabel* kind, QLabel* display, QLabel* name, QLabel* alg,
                 QLabel* ver, QLabel* file, QLabel* stamp, QLabel* desc)
{
    kind->clear();
    display->clear();
    name->clear();
    alg->clear();
    ver->clear();
    file->clear();
    stamp->clear();
    desc->clear();
}

}

// 打开工具库管理
ToolLibDlg::ToolLibDlg(QWidget* parent)
    : QDialog(parent)
    , m_tree(nullptr)
    , m_search(nullptr)
    , m_kind(nullptr)
    , m_display(nullptr)
    , m_name(nullptr)
    , m_alg(nullptr)
    , m_ver(nullptr)
    , m_file(nullptr)
    , m_stamp(nullptr)
    , m_desc(nullptr)
{
    setWindowTitle(T("工具库管理"));
    resize(980, 560);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(T("搜索显示名、注册名或 DLL"));
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(5);
    m_tree->setHeaderLabels(QStringList()
        << T("名称") << T("类型") << T("注册名") << T("算法库") << T("版本"));
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    // 右侧只读详情，随树上选中节点刷新
    QWidget* detail = new QWidget(this);
    detail->setMinimumWidth(240);
    QFormLayout* form = new QFormLayout(detail);
    m_kind = new QLabel(detail);
    m_display = new QLabel(detail);
    m_name = new QLabel(detail);
    m_alg = new QLabel(detail);
    m_ver = new QLabel(detail);
    m_file = new QLabel(detail);
    m_stamp = new QLabel(detail);
    m_desc = new QLabel(detail);
    m_desc->setWordWrap(true);
    form->addRow(T("类型"), m_kind);
    form->addRow(T("显示名"), m_display);
    form->addRow(T("注册名"), m_name);
    form->addRow(T("算法库"), m_alg);
    form->addRow(T("版本"), m_ver);
    form->addRow(T("来源"), m_file);
    form->addRow(T("文件时间"), m_stamp);
    form->addRow(T("说明"), m_desc);
    QSplitter* split = new QSplitter(this);
    split->addWidget(m_tree);
    split->addWidget(detail);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 2);
    QPushButton* reloadBtn = new QPushButton(T("重新扫描"), this);
    QPushButton* closeBtn = new QPushButton(T("关闭"), this);
    connect(reloadBtn, SIGNAL(clicked()), this, SLOT(onReload()));
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
    connect(m_tree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(onCurrentChanged()));
    connect(m_search, SIGNAL(textChanged(QString)), this, SLOT(onSearch(QString)));
    QHBoxLayout* btns = new QHBoxLayout();
    btns->addWidget(reloadBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);
    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->addWidget(m_search);
    lay->addWidget(split, 1);
    lay->addLayout(btns);
    reloadLists();
}

// 再扫插件目录
void ToolLibDlg::onReload()
{
    ToolLibMgr::instance().loadAll();
    reloadLists();
}

// 选中节点变化
void ToolLibDlg::onCurrentChanged()
{
    showNode(m_tree->currentItem());
}

// 搜索框变化
void ToolLibDlg::onSearch(const QString&)
{
    applyFilter();
}

// 登记叶子是否命中搜索
bool ToolLibDlg::leafHit(const ToolLibReg& row, const QString& key) const
{
    if (key.isEmpty()) {
        return true;
    }
    if (qs(row.displayName).contains(key, Qt::CaseInsensitive)) {
        return true;
    }
    if (qs(row.toolName).contains(key, Qt::CaseInsensitive)) {
        return true;
    }
    return qs(row.fileName).contains(key, Qt::CaseInsensitive);
}

// 重读登记并重建树表
void ToolLibDlg::reloadLists()
{
    m_regs = ToolLibMgr::instance().registrations();
    m_files = ToolLibMgr::instance().loaded();
    m_tree->clear();
    QTreeWidgetItem* toolRoot = new QTreeWidgetItem(m_tree, QStringList(T("工具插件")));
    toolRoot->setData(0, kNodeRole, QString("group"));
    toolRoot->setData(0, kIndexRole, QString("tool"));
    QTreeWidgetItem* algoRoot = new QTreeWidgetItem(m_tree, QStringList(T("算法插件")));
    algoRoot->setData(0, kNodeRole, QString("group"));
    algoRoot->setData(0, kIndexRole, QString("algo"));
    for (size_t i = 0; i < m_files.size(); ++i) {
        QTreeWidgetItem* parent = (m_files[i].kind == "algo") ? algoRoot : toolRoot;
        QTreeWidgetItem* dll = new QTreeWidgetItem(parent);
        dll->setText(0, qs(m_files[i].fileName));
        dll->setText(1, (m_files[i].kind == "algo") ? T("算法插件") : T("工具插件"));
        dll->setData(0, kNodeRole, QString("dll"));
        dll->setData(0, kIndexRole, static_cast<int>(i));
        for (size_t r = 0; r < m_regs.size(); ++r) {
            if (m_regs[r].fileName != m_files[i].fileName || m_regs[r].kind != m_files[i].kind) {
                continue;
            }
            const ToolLibReg& row = m_regs[r];
            QTreeWidgetItem* leaf = new QTreeWidgetItem(dll);
            leaf->setText(0, qs(row.displayName));
            leaf->setText(1, (row.kind == "algo") ? T("算法") : T("工具"));
            leaf->setText(2, qs(row.toolName));
            leaf->setText(3, qs(row.algLib));
            leaf->setText(4, qs(row.version));
            leaf->setData(0, kNodeRole, QString("leaf"));
            leaf->setData(0, kIndexRole, static_cast<int>(r));
        }
    }
    m_tree->expandToDepth(1);
    applyFilter();
    if (m_tree->topLevelItemCount() > 0) {
        m_tree->setCurrentItem(m_tree->topLevelItem(0));
    }
}

// 按搜索词显隐节点
void ToolLibDlg::applyFilter()
{
    const QString key = m_search->text().trimmed();
    const int roots = m_tree->topLevelItemCount();
    for (int g = 0; g < roots; ++g) {
        QTreeWidgetItem* group = m_tree->topLevelItem(g);
        bool groupShow = false;
        for (int d = 0; d < group->childCount(); ++d) {
            QTreeWidgetItem* dll = group->child(d);
            const int fileIndex = dll->data(0, kIndexRole).toInt();
            const bool dllNameHit = key.isEmpty()
                || (fileIndex >= 0 && fileIndex < static_cast<int>(m_files.size())
                    && qs(m_files[static_cast<size_t>(fileIndex)].fileName).contains(key, Qt::CaseInsensitive));
            bool anyLeaf = false;
            for (int c = 0; c < dll->childCount(); ++c) {
                QTreeWidgetItem* leaf = dll->child(c);
                const int regIndex = leaf->data(0, kIndexRole).toInt();
                bool hit = dllNameHit;
                if (!hit && regIndex >= 0 && regIndex < static_cast<int>(m_regs.size())) {
                    hit = leafHit(m_regs[static_cast<size_t>(regIndex)], key);
                }
                leaf->setHidden(!hit);
                anyLeaf = anyLeaf || hit;
            }
            const bool showDll = anyLeaf || (dllNameHit && key.isEmpty());
            dll->setHidden(!showDll && !anyLeaf);
            if (anyLeaf) {
                groupShow = true;
            }
            if (!key.isEmpty() && anyLeaf) {
                dll->setExpanded(true);
                group->setExpanded(true);
            }
        }
        group->setHidden(!groupShow && !key.isEmpty());
    }
}

// 按节点种类写详情
void ToolLibDlg::showNode(QTreeWidgetItem* item)
{
    clearLabels(m_kind, m_display, m_name, m_alg, m_ver, m_file, m_stamp, m_desc);
    if (!item) {
        return;
    }
    // 分组节点：统计该类 DLL 数与登记条数
    const QString node = item->data(0, kNodeRole).toString();
    if (node == "group") {
        const QString kind = item->data(0, kIndexRole).toString();
        int dlls = 0;
        int leaves = 0;
        for (size_t i = 0; i < m_files.size(); ++i) {
            if (m_files[i].kind == (kind == "algo" ? "algo" : "tool")) {
                ++dlls;
            }
        }
        for (size_t i = 0; i < m_regs.size(); ++i) {
            if (m_regs[i].kind == (kind == "algo" ? "algo" : "tool")) {
                ++leaves;
            }
        }
        m_kind->setText(kind == "algo" ? T("算法插件") : T("工具插件"));
        m_display->setText(T("一组插件"));
        m_desc->setText(QString::number(dlls) + T(" 个 DLL，") + QString::number(leaves) + T(" 条登记"));
        return;
    }
    // DLL 节点：文件名、时间和该 DLL 的登记条数
    if (node == "dll") {
        const int fileIndex = item->data(0, kIndexRole).toInt();
        if (fileIndex < 0 || fileIndex >= static_cast<int>(m_files.size())) {
            return;
        }
        const ToolLibItem& file = m_files[static_cast<size_t>(fileIndex)];
        int leaves = 0;
        for (size_t i = 0; i < m_regs.size(); ++i) {
            if (m_regs[i].fileName == file.fileName && m_regs[i].kind == file.kind) {
                ++leaves;
            }
        }
        m_kind->setText(file.kind == "algo" ? T("算法插件") : T("工具插件"));
        m_file->setText(qs(file.fileName));
        m_stamp->setText(qs(file.stamp));
        m_desc->setText(QString::number(leaves) + T(" 条登记"));
        return;
    }
    // 其余为单条登记
    const int regIndex = item->data(0, kIndexRole).toInt();
    if (regIndex < 0 || regIndex >= static_cast<int>(m_regs.size())) {
        return;
    }
    const ToolLibReg& row = m_regs[static_cast<size_t>(regIndex)];
    m_kind->setText(row.kind == "algo" ? T("算法") : T("工具"));
    m_display->setText(qs(row.displayName));
    m_name->setText(qs(row.toolName));
    m_alg->setText(qs(row.algLib));
    m_ver->setText(qs(row.version));
    m_file->setText(qs(row.fileName));
    m_stamp->setText(qs(row.stamp));
    m_desc->setText(qs(row.description));
}

}
#include "moc_ToolLibDlg.cpp"