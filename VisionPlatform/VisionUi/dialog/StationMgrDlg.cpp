/**
 * @file StationMgrDlg.cpp
 * @brief StationMgrDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StationMgrDlg.h"
#include "Solution.h"
#include "QtConv.h"
#include "VpUtil.h"
#include "Session.h"
#include "Theme.h"
#include "UiText.h"
#include <cstdio>
#include <QAbstractItemView>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayoutItem>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace Vision {

// 概要卡片：标签 + 粗体值
static QFrame* kvCell(QWidget* parent, const
QString& key, QLabel** value)
{
    QFrame* f = makeCard(parent);
    QVBoxLayout* v = new QVBoxLayout(f);
    v->setContentsMargins(10, 8, 10, 8);
    QLabel* k = new QLabel(key, f);
    k->setObjectName(QLatin1String("hint"));
    QLabel* val = new QLabel(QLatin1String("-"), f);
    val->setStyleSheet(QLatin1String("font-size:15px; font-weight:bold;"));
    v->addWidget(k);
    v->addWidget(val);
    *value = val;
    return f;
}

// 绑定设备摘要用的彩色标签
static QLabel* deviceChip(QWidget* parent, const QString& text)
{
    QLabel* lab = new QLabel(text, parent);
    lab->setAlignment(Qt::AlignCenter);
    lab->setMinimumWidth(88);
    lab->setStyleSheet(QLatin1String(
        "background:#E6F7FF; color:#1890FF; border:1px solid #91D5FF;"
        " border-radius:4px; padding:10px 12px; font-weight:bold;"));
    return lab;
}

// 通信通道单选按钮，commId 存在属性里
static QPushButton* commChoice(QWidget* parent, const QString& text, const QString& id)
{
    QPushButton* b = new QPushButton(text, parent);
    b->setCheckable(true);
    b->setProperty("commId", id);
    return b;
}

// 工站管理：左侧列表 + 右侧概要/设备/通信/流程
StationMgrDlg::StationMgrDlg(Session* session, Solution* solution, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_sol(solution)
{
    setWindowTitle(T("通用视觉平台 - 工站管理"));
    resize(1080, 640);

    m_list = new QListWidget(this);
    connect(m_list, SIGNAL(itemSelectionChanged()), this, SLOT(onSel()));
    connect(m_list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onOpen()));

    QPushButton* addBtn = new QPushButton(T("添加工站"), this);
    QPushButton* delBtn = new QPushButton(T("删除"), this);
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAdd()));
    connect(delBtn, SIGNAL(clicked()), this, SLOT(onRemove()));
    const bool canEdit = m_session->has(PermEditSolution);
    addBtn->setEnabled(canEdit);
    delBtn->setEnabled(canEdit);

    QFrame* leftCard = makeCard(this);
    leftCard->setMinimumWidth(220);
    QVBoxLayout* left = new QVBoxLayout(leftCard);
    QLabel* listTitle = new QLabel(T("工站列表"), this);
    listTitle->setObjectName(QLatin1String("section"));
    left->addWidget(listTitle);
    left->addWidget(m_list, 1);
    QHBoxLayout* lb = new QHBoxLayout();
    lb->addWidget(addBtn);
    lb->addWidget(delBtn);
    left->addLayout(lb);

    QHBoxLayout* kv = new QHBoxLayout();
    kv->addWidget(kvCell(this, T("名称"), &m_name), 1);
    kv->addWidget(kvCell(this, T("工位编号"), &m_code), 1);
    kv->addWidget(kvCell(this, T("状态"), &m_status), 1);
    kv->addWidget(kvCell(this, T("流程数量"), &m_jobCount), 1);
    kv->addWidget(kvCell(this, T("绑定设备数"), &m_devCount), 1);
    kv->addWidget(kvCell(this, T("执行方式"), &m_arrange), 1);

    QWidget* chipHost = new QWidget(this);
    m_chips = new QHBoxLayout(chipHost);
    m_chips->setContentsMargins(0, 0, 0, 0);
    m_chips->addStretch();

    m_commGroup = new QButtonGroup(this);
    m_commGroup->setExclusive(true);

    QGroupBox* overview = new QGroupBox(T("工站概要"), this);
    QVBoxLayout* ov = new QVBoxLayout(overview);
    ov->addLayout(kv);

    QGroupBox* bound = new QGroupBox(T("绑定设备摘要"), this);
    QVBoxLayout* bv = new QVBoxLayout(bound);
    bv->addWidget(chipHost);

    QGroupBox* comm = new QGroupBox(T("通信"), this);
    QVBoxLayout* cvOuter = new QVBoxLayout(comm);
    QWidget* commHost = new QWidget(comm);
    m_commLay = new QHBoxLayout(commHost);
    m_commLay->setContentsMargins(0, 0, 0, 0);
    rebuildCommChoices();
    cvOuter->addWidget(commHost);

    m_jobs = new QTableWidget(this);
    m_jobs->setColumnCount(4);
    QStringList hh;
    hh << T("名称") << T("触发") << T("使能") << T("顺序");
    m_jobs->setHorizontalHeaderLabels(hh);
    m_jobs->horizontalHeader()->setStretchLastSection(true);
    m_jobs->verticalHeader()->setVisible(false);
    m_jobs->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_jobs->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_jobs->setAlternatingRowColors(true);
    connect(m_jobs, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onJobDouble(int,int)));

    QPushButton* openBtn = new QPushButton(T("进入工站配置"), this);
    QPushButton* jobBtn = new QPushButton(T("打开流程"), this);
    setButtonRole(openBtn, "primary");
    connect(openBtn, SIGNAL(clicked()), this, SLOT(onOpen()));
    connect(jobBtn, SIGNAL(clicked()), this, SLOT(onOpenJob()));

    QGroupBox* jobs = new QGroupBox(T("本工站流程摘要"), this);
    QVBoxLayout* jv = new QVBoxLayout(jobs);
    jv->addWidget(m_jobs);

    QHBoxLayout* actions = new QHBoxLayout();
    actions->addWidget(openBtn);
    actions->addWidget(jobBtn);
    actions->addStretch();

    QDialogButtonBox* box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    box->button(QDialogButtonBox::Ok)->setText(T("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(T("取消"));
    box->button(QDialogButtonBox::Apply)->setText(T("应用"));
    setButtonRole(box->button(QDialogButtonBox::Ok), "primary");
    connect(box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(box->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(onApply()));
    connect(this, SIGNAL(accepted()), this, SLOT(onApply()));

    QWidget* right = new QWidget(this);
    QVBoxLayout* rLay = new QVBoxLayout(right);
    rLay->setContentsMargins(0, 0, 0, 0);
    rLay->addWidget(overview);
    rLay->addWidget(bound);
    rLay->addWidget(comm);
    rLay->addWidget(jobs, 1);
    rLay->addLayout(actions);

    QHBoxLayout* body = new QHBoxLayout();
    body->addWidget(leftCard, 1);
    body->addWidget(right, 3);

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(body, 1);
    lay->addWidget(box);
    refresh();
}

// 刷新工站列表并尽量保持原选中项
void StationMgrDlg::refresh()
{
    const QString keep = currentId();
    m_list->clear();
    for (int i = 0; i < m_sol->stations.size(); ++i) {
        const StationCfg& st = m_sol->stations[i];
        QListWidgetItem* it = new QListWidgetItem(qs(st.stationCode) + QLatin1String("  ") + qs(st.name), m_list);
        it->setData(Qt::UserRole, qs(st.id));
        it->setToolTip(qs(st.stationCode));
    }
    if (!keep.isEmpty()) {
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(Qt::UserRole).toString() == keep) {
                m_list->setCurrentRow(i);
                break;
            }
        }
    } else if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    fillDetail();
}

// 当前选中工站 ID
QString StationMgrDlg::currentId() const
{
    QListWidgetItem* it = m_list->currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString();
}

// 选中变化时刷新右侧详情
void StationMgrDlg::onSel()
{
    fillDetail();
}

// 填充概要、设备芯片、通信选择与流程表
void StationMgrDlg::fillDetail()
{
    // 立即删掉旧芯片；deleteLater 会与新控件叠在同一位置
    QWidget* chipParent = m_chips->parentWidget();
    while (QLayoutItem* it = m_chips->takeAt(0)) {
        if (QWidget* w = it->widget()) {
            delete w;
        }
        if (it) {
            delete it;
        }
    }

    const StationCfg* st = m_sol->findStationConfig(oid(currentId()));
    if (!st) {
        m_name->setText(QLatin1String("-"));
        m_code->setText(QLatin1String("-"));
        m_status->setText(QLatin1String("-"));
        m_jobCount->setText(QLatin1String("0"));
        m_devCount->setText(QLatin1String("0"));
        m_arrange->setText(QLatin1String("-"));
        m_jobs->setRowCount(0);
        m_chips->addStretch();
        selectComm(QString());
        return;
    }

    m_name->setText(qs(st->name));
    m_code->setText(qs(st->stationCode));
    m_status->setText(T("● 已配置"));
    m_status->setStyleSheet(QLatin1String("color:#52C41A; font-size:15px; font-weight:bold;"));
    m_jobCount->setText(QString::number(st->jobs.size()));
    m_devCount->setText(QString::number(st->boundDevices.size()));
    m_arrange->setText(QString::fromUtf8(scheduleTitle(st->jobSchedule).c_str()));

    // 按设备种类统计并生成摘要芯片
    int cam = 0, folderN = 0, imageN = 0, card = 0, plc = 0;
    for (int i = 0; i < st->boundDevices.size(); ++i) {
        const HardwareDevice* d = m_sol->findDevice(st->boundDevices[i].deviceId);
        if (!d) {
            continue;
        }
        if (d->kind == DeviceCamera) {
            ++cam;
        } else if (d->kind == DeviceFolder) {
            ++folderN;
        } else if (d->kind == DeviceImage) {
            ++imageN;
        } else if (d->kind == DeviceControlCard) {
            ++card;
        } else if (d->kind == DevicePlc) {
            ++plc;
        }
    }
    if (cam > 0) {
        m_chips->addWidget(deviceChip(chipParent, T("相机 (") + QString::number(cam) + QLatin1Char(')')));
    }
    if (card > 0) {
        m_chips->addWidget(deviceChip(chipParent, T("控制卡 (") + QString::number(card) + QLatin1Char(')')));
    }
    if (plc > 0) {
        m_chips->addWidget(deviceChip(chipParent, T("PLC (") + QString::number(plc) + QLatin1Char(')')));
    }
    if (folderN > 0) {
        m_chips->addWidget(deviceChip(chipParent, T("文件夹 (") + QString::number(folderN) + QLatin1Char(')')));
    }
    if (imageN > 0) {
        m_chips->addWidget(deviceChip(chipParent, T("图像 (") + QString::number(imageN) + QLatin1Char(')')));
    }
    if (cam + folderN + imageN + card + plc == 0) {
        QLabel* empty = new QLabel(T("未绑定设备"), chipParent);
        empty->setObjectName(QLatin1String("hint"));
        m_chips->addWidget(empty);
    }
    m_chips->addStretch();

    rebuildCommChoices();
    selectComm(qs(st->commChannelId));

    // 本工站流程摘要表
    m_jobs->setRowCount(0);
    for (int i = 0; i < st->jobs.size(); ++i) {
        const JobCfg& j = st->jobs[i];
        const int r = m_jobs->rowCount();
        m_jobs->insertRow(r);
        QTableWidgetItem* name = new QTableWidgetItem(qs(j.name));
        name->setData(Qt::UserRole, qs(j.id));
        m_jobs->setItem(r, 0, name);
        m_jobs->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(triggerTitle(j.trigger).c_str())));
        m_jobs->setItem(r, 2, new QTableWidgetItem(j.enabled ? T("开") : T("关")));
        m_jobs->setItem(r, 3, new QTableWidgetItem(QString::number(j.order)));
    }
}

// 按方案通信台账重建通道按钮
void StationMgrDlg::rebuildCommChoices()
{
    const bool canEdit = m_session->has(PermEditSolution);
    const QString keep = currentCommId();
    // 立即删除；deleteLater 会与新按钮短暂叠在旧几何位置（见绑定设备芯片同款问题）
    QWidget* host = m_commLay->parentWidget();
    if (!host) {
        host = this;
    }
    while (QLayoutItem* it = m_commLay->takeAt(0)) {
        if (QWidget* w = it->widget()) {
            m_commGroup->removeButton(qobject_cast<QAbstractButton*>(w));
            delete w;
        }
        if (it) {
            delete it;
        }
    }
    QList<QAbstractButton*> old = m_commGroup->buttons();
    for (int i = 0; i < old.size(); ++i) {
        m_commGroup->removeButton(old[i]);
        if (old[i]) {
            delete old[i];
            old[i] = nullptr;
        }
    }
    QPushButton* cNone = commChoice(host, T("无"), QString());
    cNone->setEnabled(canEdit);
    m_commGroup->addButton(cNone);
    m_commLay->addWidget(cNone);
    for (size_t i = 0; i < m_sol->commChannels.size(); ++i) {
        const CommChannel& c = m_sol->commChannels[i];
        QPushButton* b = commChoice(host, qs(c.name), qs(c.id));
        b->setEnabled(canEdit);
        m_commGroup->addButton(b);
        m_commLay->addWidget(b);
    }
    m_commLay->addStretch();
    selectComm(keep);
}

// 当前勾选的通信通道 ID
QString StationMgrDlg::currentCommId() const
{
    QAbstractButton* b = m_commGroup->checkedButton();
    return b ? b->property("commId").toString() : QString();
}

// 按通道 ID 勾选通信按钮，找不到则回退第一项
void StationMgrDlg::selectComm(const QString& id)
{
    const QList<QAbstractButton*> bs = m_commGroup->buttons();
    QAbstractButton* fallback = nullptr;
    for (int i = 0; i < bs.size(); ++i) {
        if (!fallback) {
            fallback = bs[i];
        }
        if (bs[i]->property("commId").toString() == id) {
            bs[i]->setChecked(true);
            return;
        }
    }
    if (fallback) {
        fallback->setChecked(true);
    }
}

// 把通信选择写回当前工站
void StationMgrDlg::applyComm()
{
    StationCfg* st = m_sol->findStationConfig(oid(currentId()));
    if (!st || !m_session->has(PermEditSolution)) {
        return;
    }
    const std::string commId = ss(currentCommId());
    if (st->commChannelId == commId) {
        return;
    }
    st->commChannelId = commId;
    m_sol->dirty = true;
}

// 应用按钮：只写回通信通道
void StationMgrDlg::onApply()
{
    applyComm();
}

// 追加一个带默认编号的新工站
void StationMgrDlg::onAdd()
{
    StationCfg st;
    st.id = ObjectId::create();
    st.name = std::string(Translate::tr("新工站"));
    char codeBuf[32];
    sprintf(codeBuf, "ST-%02d", static_cast<int>(m_sol->stations.size()) + 1);
    st.stationCode = std::string(codeBuf);
    m_sol->stations.push_back(st);
    m_sol->dirty = true;
    refresh();
    m_list->setCurrentRow(m_list->count() - 1);
}

// 删除工站及其流程
void StationMgrDlg::onRemove()
{
    const QString id = currentId();
    if (id.isEmpty()) {
        return;
    }
    if (QMessageBox::question(this, T("删除"), T("删除工站将级联其 JobCfg，确认？")) != QMessageBox::Yes) {
        return;
    }
    for (int i = 0; i < m_sol->stations.size(); ++i) {
        if (qs(m_sol->stations[i].id) == id) {
            vpRemoveAt(m_sol->stations, i);
            m_sol->dirty = true;
            break;
        }
    }
    refresh();
}

// 先保存通信再打开工站配置
void StationMgrDlg::onOpen()
{
    applyComm();
    const QString id = currentId();
    if (!id.isEmpty()) {
        emit openStationRequested(id);
    }
}

// 打开选中流程，无选中则打开该工站第一个
void StationMgrDlg::onOpenJob()
{
    applyComm();
    if (m_jobs->currentRow() >= 0 && m_jobs->item(m_jobs->currentRow(), 0)) {
        emit openJobRequested(m_jobs->item(m_jobs->currentRow(), 0)->data(Qt::UserRole).toString());
        return;
    }
    StationCfg* st = m_sol->findStationConfig(oid(currentId()));
    if (st && !st->jobs.empty()) {
        emit openJobRequested(qs(st->jobs[0].id));
    }
}

// 双击流程表行打开该流程
void StationMgrDlg::onJobDouble(int row, int)
{
    if (row < 0 || !m_jobs->item(row, 0)) {
        return;
    }
    applyComm();
    emit openJobRequested(m_jobs->item(row, 0)->data(Qt::UserRole).toString());
}

}

#include "moc_StationMgrDlg.cpp"
