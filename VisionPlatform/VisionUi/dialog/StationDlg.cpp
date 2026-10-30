/**
 * @file StationDlg.cpp
 * @brief StationDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StationDlg.h"
#include "Solution.h"
#include "QtConv.h"
#include "RecipeDraft.h"
#include "VpUtil.h"
#include "Session.h"
#include "Theme.h"
#include "UiText.h"
#include <cstdio>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Vision {

namespace {

// 表格至少显示 rows 行，超出部分滚动
void setTableMinVisibleRows(QTableWidget* table, int rows)
{
    if (!table || rows <= 0) {
        return;
    }
    table->verticalHeader()->setVisible(false);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    if (table->verticalHeader()->defaultSectionSize() < 20) {
        table->verticalHeader()->setDefaultSectionSize(26);
    }
    const int rowH = table->verticalHeader()->defaultSectionSize();
    const int headH = table->horizontalHeader()->height() > 0
        ? table->horizontalHeader()->height() : 28;
    table->setMinimumHeight(headH + rowH * rows + 2);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

} // namespace

// 工站配置：基本信息、绑定设备与 JobCfg 表
StationDlg::StationDlg(Session* session,
Solution* solution, const QString& stationId, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_sol(solution)
    , m_stationId(stationId)
{
    setWindowTitle(T("工站配置"));
    resize(860, 560);
    setMinimumSize(640, 480);
    enableDialogResize(this);
    const bool canEdit = m_session->has(PermEditSolution);

    // 基本信息表单
    m_name = new QLineEdit(this);
    m_enabled = new QCheckBox(T("启用工站"), this);
    m_code = new QLineEdit(this);
    m_schedule = new QComboBox(this);
    m_schedule->addItem(T("串行"), ScheduleSequence);
    m_schedule->addItem(T("并行"), ScheduleParallel);
    m_timeout = new QSpinBox(this);
    m_timeout->setRange(1, 600000);
    m_timeout->setSuffix(QLatin1String(" ms"));
    m_devices = new QListWidget(this);
    m_devices->setMaximumHeight(110);

    QFormLayout* form = new QFormLayout();
    form->addRow(T("工站名称"), m_name);
    form->addRow(T("使能"), m_enabled);
    form->addRow(T("工位编号"), m_code);
    form->addRow(T("流程执行方式"), m_schedule);
    form->addRow(T("默认超时"), m_timeout);
    form->addRow(T("绑定设备"), m_devices);

    // 流程表与增删、打开、切换使能按钮；打开流程不受编辑权限限制
    m_jobs = new QTableWidget(this);
    m_jobs->setColumnCount(5);
    QStringList h;
    h << T("顺序") << T("名称") << T("超时") << T("使能") << T("工具数");
    m_jobs->setHorizontalHeaderLabels(h);
    m_jobs->horizontalHeader()->setStretchLastSection(true);
    m_jobs->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_jobs->setEditTriggers(QAbstractItemView::NoEditTriggers);
    setTableMinVisibleRows(m_jobs, 3);

    QPushButton* addJ = new QPushButton(T("新建流程"), this);
    QPushButton* delJ = new QPushButton(T("删除"), this);
    QPushButton* openJ = new QPushButton(T("打开流程配置"), this);
    QPushButton* enJ = new QPushButton(T("切换使能"), this);
    connect(addJ, SIGNAL(clicked()), this, SLOT(onAddJob()));
    connect(delJ, SIGNAL(clicked()), this, SLOT(onDelJob()));
    connect(openJ, SIGNAL(clicked()), this, SLOT(onOpenJob()));
    connect(enJ, SIGNAL(clicked()), this, SLOT(onToggleJobEnabled()));
    connect(m_jobs, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onOpenJob()));
    addJ->setEnabled(canEdit);
    delJ->setEnabled(canEdit);
    enJ->setEnabled(canEdit);

    QHBoxLayout* jb = new QHBoxLayout();
    jb->addWidget(addJ);
    jb->addWidget(openJ);
    jb->addWidget(enJ);
    jb->addWidget(delJ);
    jb->addStretch();

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, SIGNAL(accepted()), this, SLOT(onSave()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(form);
    lay->addWidget(new QLabel(T("本工站流程"), this));
    lay->addLayout(jb);
    lay->addWidget(m_jobs, 1);
    lay->addWidget(new QLabel(T("工站内多条流程可串行或并行；单条流程内并行请用「并行组」。"), this));
    lay->addWidget(box);

    m_name->setReadOnly(!canEdit);
    m_enabled->setEnabled(canEdit);
    m_code->setReadOnly(!canEdit);
    m_schedule->setEnabled(canEdit);
    m_timeout->setEnabled(canEdit);
    m_devices->setEnabled(canEdit);

    // 在 draft 上编辑，另存打开时快照用于判断是否改过
    StationCfg* formal = m_sol->findStationConfig(oid(stationId));
    if (formal) {
        m_draft.loadFrom(*formal);
        m_formalSnapshot.loadFrom(*formal);
    }
    connect(m_name, SIGNAL(textEdited(QString)), this, SLOT(onEdited()));
    connect(m_enabled, SIGNAL(toggled(bool)), this, SLOT(onEdited()));
    connect(m_code, SIGNAL(textEdited(QString)), this, SLOT(onEdited()));
    connect(m_schedule, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
    connect(m_timeout, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
    connect(m_devices, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(onEdited()));
    load();
}

// 从 draft 填充表单
void StationDlg::load()
{
    m_name->setText(qs(m_draft.name));
    m_enabled->setChecked(m_draft.enabled);
    m_code->setText(qs(m_draft.stationCode));
    m_schedule->setCurrentIndex(m_draft.jobSchedule == ScheduleParallel ? 1 : 0);
    m_timeout->setValue(m_draft.timeoutMs);
    m_devices->clear();
    for (int i = 0; i < m_sol->inventory.size(); ++i) {
        const HardwareDevice& d = m_sol->inventory[i];
        QListWidgetItem* it = new QListWidgetItem(qs(d.name), m_devices);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setData(Qt::UserRole, qs(d.id));
        bool on = false;
        for (int b = 0; b < static_cast<int>(m_draft.boundDevices.size()); ++b) {
            if (m_draft.boundDevices[static_cast<size_t>(b)].deviceId == d.id
                && m_draft.boundDevices[static_cast<size_t>(b)].enabled) {
                on = true;
            }
        }
        it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    }
    m_jobs->setRowCount(0);
    for (int i = 0; i < static_cast<int>(m_draft.jobs.size()); ++i) {
        const JobCfg& j = m_draft.jobs[static_cast<size_t>(i)];
        const int r = m_jobs->rowCount();
        m_jobs->insertRow(r);
        m_jobs->setItem(r, 0, new QTableWidgetItem(QString::number(j.order)));
        QTableWidgetItem* name = new QTableWidgetItem(qs(j.name));
        name->setData(Qt::UserRole, qs(j.id));
        m_jobs->setItem(r, 1, name);
        m_jobs->setItem(r, 2, new QTableWidgetItem(QString::number(j.timeoutMs)));
        m_jobs->setItem(r, 3, new QTableWidgetItem(j.enabled ? T("是") : T("否")));
        m_jobs->setItem(r, 4, new QTableWidgetItem(QString::number(j.tools.size())));
    }
}

// 表单写回 draft
void StationDlg::flushUiToDraft()
{
    if (!m_session->has(PermEditSolution)) {
        return;
    }
    m_draft.enabled = m_enabled->isChecked();
    m_draft.name = ss(m_name->text().trimmed());
    m_draft.stationCode = ss(m_code->text().trimmed());
    m_draft.jobSchedule = static_cast<JobSchedule>(m_schedule->currentData().toInt());
    m_draft.timeoutMs = m_timeout->value();
    m_draft.boundDevices.clear();
    for (int i = 0; i < m_devices->count(); ++i) {
        QListWidgetItem* it = m_devices->item(i);
        if (it->checkState() == Qt::Checked) {
            DeviceRef r;
            r.deviceId = oid(it->data(Qt::UserRole).toString());
            m_draft.boundDevices.push_back(r);
        }
    }
}

// 更新脏标记
void StationDlg::markDraftDirty()
{
    flushUiToDraft();
    // 工站 draft 无 Registry，靠 askCloseAction 本地比较
}

// 表单变化
void StationDlg::onEdited()
{
    markDraftDirty();
}

// 关窗前三选一：取消=0，不保存=1，已保存=2
int StationDlg::askCloseAction()
{
    flushUiToDraft();
    if (m_draft.equals(m_formalSnapshot)) {
        return 1;
    }
    const int q = askUnsavedCloseQuestion(this);
    if (q == 0) {
        return 0;
    }
    if (q == 2) {
        if (!trySave()) {
            return 0;
        }
        return 2;
    }
    return 1;
}

// 写回 formal，不关窗
bool StationDlg::trySave()
{
    if (!m_session->has(PermEditSolution)) {
        return true;
    }
    flushUiToDraft();
    if (m_draft.equals(m_formalSnapshot)) {
        return true;
    }
    StationCfg st;
    st.id = oid(m_stationId);
    m_draft.applyTo(&st);
    if (!m_sol->commitStation(st)) {
        QMessageBox::warning(this, T("保存"), T("保存失败。"));
        return false;
    }
    m_formalSnapshot = m_draft;
    m_sol->dirty = true;
    return true;
}

// 取消
void StationDlg::reject()
{
    const int action = askCloseAction();
    if (action == 0) {
        return;
    }
    if (action == 2) {
        QDialog::accept();
        return;
    }
    QDialog::reject();
}

// 关闭前确认
void StationDlg::closeEvent(QCloseEvent* event)
{
    const int action = askCloseAction();
    if (action == 0) {
        event->ignore();
        return;
    }
    event->accept();
    if (action == 2) {
        QDialog::accept();
    } else {
        QDialog::reject();
    }
}

// 当前选中的流程行号
int StationDlg::currentJobRow() const
{
    return m_jobs->currentRow();
}

// 保存工站 draft
void StationDlg::onSave()
{
    if (!trySave()) {
        return;
    }
    accept();
}

// 新建空流程，不带工具
void StationDlg::onAddJob()
{
    if (!m_session->has(PermEditSolution)) {
        return;
    }
    flushUiToDraft();
    JobCfg j;
    j.id = ObjectId::create();
    j.order = static_cast<int>(m_draft.jobs.size()) + 1;
    char jobBuf[32];
    sprintf(jobBuf, "JobCfg%d", j.order);
    j.name = std::string(jobBuf);
    j.root.id = ObjectId::create();
    j.execMode = ExecActive;
    j.root.kind = GroupSequence;

    m_draft.jobs.push_back(j);
    markDraftDirty();
    load();
}

// 删除当前行流程（draft）
void StationDlg::onDelJob()
{
    if (!m_session->has(PermEditSolution)) {
        return;
    }
    const int r = currentJobRow();
    if (r < 0 || r >= static_cast<int>(m_draft.jobs.size())) {
        return;
    }
    flushUiToDraft();
    vpRemoveAt(m_draft.jobs, r);
    markDraftDirty();
    load();
}

// 打开流程配置：先把工站 draft 提交到 formal（与旧版「打开前写回」一致）

// 切换当前选中流程的使能
void StationDlg::onToggleJobEnabled()
{
    if (!m_session->has(PermEditSolution)) {
        return;
    }
    const int r = currentJobRow();
    if (r < 0 || r >= static_cast<int>(m_draft.jobs.size())) {
        return;
    }
    flushUiToDraft();
    m_draft.jobs[static_cast<size_t>(r)].enabled = !m_draft.jobs[static_cast<size_t>(r)].enabled;
    markDraftDirty();
    load();
    m_jobs->setCurrentCell(r, 0);
}
// 先提交工站草稿，再请求打开该流程
void StationDlg::onOpenJob()
{
    const int r = currentJobRow();
    if (r < 0 || !m_jobs->item(r, 1)) {
        return;
    }
    flushUiToDraft();
    if (!m_draft.equals(m_formalSnapshot)) {
        StationCfg st;
        st.id = oid(m_stationId);
        m_draft.applyTo(&st);
        if (m_sol->commitStation(st)) {
            m_formalSnapshot = m_draft;
            m_sol->dirty = true;
        }
    }
    emit openJobRequested(m_jobs->item(r, 1)->data(Qt::UserRole).toString());
}

}

#include "moc_StationDlg.cpp"
