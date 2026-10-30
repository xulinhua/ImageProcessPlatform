/**
 * @file LogDlg.cpp
 * @brief LogDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LogDlg.h"
#include "AuditLog.h"
#include "Perm.h"
#include "QtConv.h"
#include "Session.h"
#include "Theme.h"
#include "UiText.h"
#include <cstdio>
#include <cstdlib>
#include <QAbstractItemView>
#include <QDate>
#include <QDateEdit>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Vision {

namespace {

// 建一张只读日志表
QTableWidget* makeLogTable(QWidget* parent)
{
    QTableWidget* t = new QTableWidget(parent);
    t->setColumnCount(7);
    QStringList h;
    h << T("时间") << T("用户") << T("角色") << T("动作") << T("对象") << T("结果") << T("说明");
    t->setHorizontalHeaderLabels(h);
    t->horizontalHeader()->setStretchLastSection(true);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    return t;
}

// 查询结果列：失败、拒绝，其余视为成功
QString resultText(const std::string& r)
{
    if (r == "fail") {
        return QString::fromUtf8(Translate::tr("失败").c_str());
    }
    if (r == "denied") {
        return QString::fromUtf8(Translate::tr("拒绝").c_str());
    }
    return QString::fromUtf8(Translate::tr("成功").c_str());
}

} // namespace

// 三个页：操作、调试、管理
LogDlg::LogDlg(Session* session, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
{
    setWindowTitle(T("日志管控"));
    setModal(true);
    resize(900, 560);

    // 日期过滤，默认近 7 天
    m_from = new QDateEdit(this);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QLatin1String("yyyy-MM-dd"));
    m_from->setDate(QDate::currentDate().addDays(-7));
    m_to = new QDateEdit(this);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QLatin1String("yyyy-MM-dd"));
    m_to->setDate(QDate::currentDate());

    QPushButton* refresh = new QPushButton(T("查询"), this);
    setButtonRole(refresh, "primary");
    connect(refresh, SIGNAL(clicked()), this, SLOT(onRefresh()));

    QHBoxLayout* filter = new QHBoxLayout();
    filter->addWidget(new QLabel(T("从"), this));
    filter->addWidget(m_from);
    filter->addWidget(new QLabel(T("到"), this));
    filter->addWidget(m_to);
    filter->addWidget(refresh);
    filter->addStretch();

    // 页签：操作日志表、调试日志表、按日期删除的管理页
    m_opTable = makeLogTable(this);
    m_dbgTable = makeLogTable(this);

    m_files = new QListWidget(this);
    m_btnDelete = new QPushButton(T("删除勾选的日期"), this);
    connect(m_btnDelete, SIGNAL(clicked()), this, SLOT(onDelete()));
    m_fileHint = new QLabel(this);
    m_fileHint->setWordWrap(true);
    m_fileHint->setObjectName(QLatin1String("hint"));

    QWidget* manage = new QWidget(this);
    QVBoxLayout* mLay = new QVBoxLayout(manage);
    mLay->addWidget(m_fileHint);
    mLay->addWidget(m_files, 1);
    mLay->addWidget(m_btnDelete, 0, Qt::AlignLeft);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_opTable, T("操作日志"));
    m_tabs->addTab(m_dbgTable, T("调试日志"));
    m_tabs->addTab(manage, T("日志管理"));
    connect(m_tabs, SIGNAL(currentChanged(int)), this, SLOT(onTabChanged(int)));

    QPushButton* closeBtn = new QPushButton(T("关闭"), this);
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(filter);
    lay->addWidget(m_tabs, 1);
    QHBoxLayout* bot = new QHBoxLayout();
    bot->addStretch();
    bot->addWidget(closeBtn);
    lay->addLayout(bot);

    onRefresh();
}

// 有删除权限才允许勾选删除
bool LogDlg::canManage() const
{
    return m_session && m_session->has(PermManageLogs);
}

// 查询当前日期范围
void LogDlg::onRefresh()
{
    fillTable(m_opTable, LogKindOp);
    fillTable(m_dbgTable, LogKindDebug);
    fillFiles();
}

// 页签切换槽；查库仍由刷新按钮负责
void LogDlg::onTabChanged(int)
{
}

// 填一张表
void LogDlg::fillTable(QTableWidget* table, LogKind kind)
{
    if (!table) {
        return;
    }
    LogQuery q;
    q.kind = kind;
    q.dayFrom = ss(m_from->date().toString(QLatin1String("yyyy-MM-dd")));
    q.dayTo = ss(m_to->date().toString(QLatin1String("yyyy-MM-dd")));
    q.maxRows = 500;
    const std::vector<LogRecord> rows = LogMgr::query(q);
    table->setRowCount(static_cast<int>(rows.size()));
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const LogRecord& r = rows[static_cast<size_t>(i)];
        table->setItem(i, 0, new QTableWidgetItem(qs(r.timeIso)));
        table->setItem(i, 1, new QTableWidgetItem(qs(r.user)));
        table->setItem(i, 2, new QTableWidgetItem(r.role.empty()
            ? QString(QLatin1String("-"))
            : QString::fromUtf8(roleTitle(roleFromInt(atoi(r.role.c_str()))).c_str())));
        table->setItem(i, 3, new QTableWidgetItem(QString::fromUtf8(LogMgr::actionTitle(r.action).c_str())));
        table->setItem(i, 4, new QTableWidgetItem(qs(r.target)));
        table->setItem(i, 5, new QTableWidgetItem(resultText(r.result)));
        table->setItem(i, 6, new QTableWidgetItem(qs(r.detail)));
    }
    table->resizeColumnsToContents();
}

// 文件列表：无管理权限则只读
void LogDlg::fillFiles()
{
    m_files->clear();
    const bool manage = canManage();
    m_btnDelete->setEnabled(manage);
    m_fileHint->setText(manage
                        ? T("勾选日期后删除。删除的是数据库中该日记录，不可恢复。")
                        : T("当前角色只能查看，不能删除日志。"));
    const std::vector<LogFileInfo> files = LogMgr::listFiles();
    for (size_t i = 0; i < files.size(); ++i) {
        const LogFileInfo& fi = files[i];
        const QString kind = (fi.kind == LogKindDebug) ? T("调试")
            : (fi.kind == LogKindRun ? T("运行") : T("操作"));
        QListWidgetItem* it = new QListWidgetItem(kind + QLatin1String("  ") + qs(fi.fileName), m_files);
        it->setData(Qt::UserRole, qs(fi.path));
        if (manage) {
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            it->setCheckState(Qt::Unchecked);
        } else {
            it->setFlags(Qt::ItemIsEnabled);
        }
    }
}

// 删除勾选文件并记操作日志
void LogDlg::onDelete()
{
    if (!canManage()) {
        QMessageBox::warning(this, T("日志管控"), T("没有删除日志的权限。"));
        return;
    }
    std::vector<std::string> paths;
    for (int i = 0; i < m_files->count(); ++i) {
        QListWidgetItem* it = m_files->item(i);
        if (it && it->checkState() == Qt::Checked) {
            paths.push_back(ss(it->data(Qt::UserRole).toString()));
        }
    }
    if (paths.empty()) {
        QMessageBox::information(this, T("日志管控"), T("请先勾选要删除的日期。"));
        return;
    }
    if (QMessageBox::question(this, T("日志管控"),
                              T("确定删除勾选日期的日志？此操作不可恢复。"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    const int n = LogMgr::deleteFiles(paths);
    char buf[64];
    sprintf_s(buf, "%d", n);
    auditLog("log_delete", m_session->user().loginName, std::string(buf));
    onRefresh();
}

}

#include "moc_LogDlg.cpp"
