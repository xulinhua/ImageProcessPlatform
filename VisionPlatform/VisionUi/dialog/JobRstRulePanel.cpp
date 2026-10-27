/**
 * @file JobRstRulePanel.cpp
 * @brief JobRstRulePanel实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "JobRstRulePanel.h"
#include "QtConv.h"
#include "Translate.h"
#include "UiText.h"
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <string>

namespace Vision {

enum {
    ColName = 0,
    ColBindTool = 1,
    ColTermStart = 2
};

static const int kRoleRuleId = Qt::UserRole;

// 输出结果配置面板
JobRstRulePanel::JobRstRulePanel(QWidget* parent)
    : QWidget(parent)
    , m_editJob(nullptr)
    , m_editable(true)
{
    QLabel* title = new QLabel(T("逻辑判断矩阵"), this);
    title->setObjectName(QLatin1String("section"));

    QLabel* hint = new QLabel(
        T("每格单击循环：忽略 → 要求 OK → 要求 NG。未运行工具自动忽略。同行多项为 AND。"), this);
    hint->setObjectName(QLatin1String("hint"));
    hint->setWordWrap(true);

    m_table = new QTableWidget(this);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setMinimumHeight(360);

    m_addRule = new QPushButton(T("添加逻辑判断"), this);
    m_removeRule = new QPushButton(T("删除选中行"), this);
    connect(m_addRule, SIGNAL(clicked()), this, SLOT(onAddRule()));
    connect(m_removeRule, SIGNAL(clicked()), this, SLOT(onRemoveRule()));

    QHBoxLayout* btns = new QHBoxLayout();
    btns->addWidget(m_addRule);
    btns->addWidget(m_removeRule);
    btns->addStretch(1);

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(title);
    lay->addWidget(hint);
    lay->addWidget(m_table, 1);
    lay->addLayout(btns);
}

// 绑定编辑 JobCfg
void JobRstRulePanel::setJob(JobCfg* job)
{
    m_editJob = job;
}

// 设置三态格外观
void JobRstRulePanel::applyTermVisual(QCheckBox* cb, int state)
{
    if (!cb) {
        return;
    }
    cb->setProperty("termState", state);
    cb->blockSignals(true);
    switch (state) {
    case RstTermRequireOk:
        cb->setTristate(false);
        cb->setCheckState(Qt::Checked);
        cb->setText(QString());
        cb->setToolTip(T("要求 OK（单击切换）"));
        break;
    case RstTermRequireNg:
        cb->setTristate(true);
        cb->setCheckState(Qt::PartiallyChecked);
        cb->setText(T("反"));
        cb->setToolTip(T("要求 NG（单击切换）"));
        break;
    default:
        cb->setTristate(false);
        cb->setCheckState(Qt::Unchecked);
        cb->setText(QString());
        cb->setToolTip(T("忽略（单击切换）"));
        break;
    }
    cb->blockSignals(false);
}

// 解析列对应的来源
bool JobRstRulePanel::columnSource(int col, int ruleRow, int* kind, ObjectId* sourceId) const
{
    if (!m_editJob || col < ColTermStart || !kind || !sourceId) {
        return false;
    }
    const std::vector<ObjectId> tools = m_editJob->okNgToolIds();
    const int toolCount = static_cast<int>(tools.size());
    const int ruleCol = col - ColTermStart - toolCount;
    if (col < ColTermStart + toolCount) {
        *kind = RstTermTool;
        *sourceId = tools[static_cast<size_t>(col - ColTermStart)];
        return true;
    }
    if (ruleCol >= 0 && ruleCol < ruleRow) {
        *kind = RstTermRule;
        *sourceId = m_editJob->resultRules[static_cast<size_t>(ruleCol)].id;
        return true;
    }
    return false;
}

// 填充「流程结果选择」下拉
void JobRstRulePanel::populateFromRuleCombo(QComboBox* combo, const JobCfg* job)
{
    if (!combo) {
        return;
    }
    combo->blockSignals(true);
    combo->clear();
    combo->addItem(T("（末级工具，默认）"), QString());
    if (job) {
        for (size_t i = 0; i < job->resultRules.size(); ++i) {
            const RstRule& r = job->resultRules[static_cast<size_t>(i)];
            combo->addItem(qs(r.name), qs(r.id));
        }
        const QString fromId = qs(job->resultFromRuleId);
        const int idx = combo->findData(fromId);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    combo->blockSignals(false);
}

// 从下拉写回 resultFromRuleId
void JobRstRulePanel::applyFromRuleCombo(QComboBox* combo, JobCfg* job)
{
    if (!combo || !job) {
        return;
    }
    const QString fromData = combo->currentData().toString();
    job->resultFromRuleId = fromData.isEmpty() ? ObjectId() : oid(fromData);
}

// 从 JobCfg 刷新
void JobRstRulePanel::loadFromJob(const JobCfg* job)
{
    if (job) {
        m_editJob = const_cast<JobCfg*>(job);
    }
    rebuildTable();
}

// 写回 JobCfg
void JobRstRulePanel::flushToJob(JobCfg* job)
{
    if (!job) {
        return;
    }
    job->resultRules.clear();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* nameItem = m_table->item(row, ColName);
        if (!nameItem) {
            continue;
        }
        const QString rid = nameItem->data(kRoleRuleId).toString();
        if (rid.isEmpty()) {
            continue;
        }
        RstRule rule;
        rule.id = oid(rid);
        QLineEdit* nameEdit = qobject_cast<QLineEdit*>(m_table->cellWidget(row, ColName));
        if (nameEdit) {
            rule.name = ss(nameEdit->text().trimmed());
        }
        QComboBox* bind = qobject_cast<QComboBox*>(m_table->cellWidget(row, ColBindTool));
        if (bind && bind->currentIndex() > 0) {
            rule.toolId = oid(bind->currentData().toString());
        }
        const int cols = m_table->columnCount();
        for (int col = ColTermStart; col < cols; ++col) {
            QCheckBox* cb = qobject_cast<QCheckBox*>(m_table->cellWidget(row, col));
            if (!cb || !cb->isEnabled()) {
                continue;
            }
            const int state = cb->property("termState").toInt();
            if (state == RstTermIgnore) {
                continue;
            }
            int kind = RstTermTool;
            ObjectId sourceId;
            if (!columnSource(col, row, &kind, &sourceId)) {
                continue;
            }
            RstRuleTerm term;
            term.kind = kind;
            term.sourceId = sourceId;
            term.state = state;
            rule.terms.push_back(term);
        }
        if (rule.name.empty()) {
            rule.name = std::string(Translate::tr("逻辑判断"));
        }
        job->resultRules.push_back(rule);
    }
    job->normalizeRstRules();
}

// 设置是否可编辑
void JobRstRulePanel::setEditable(bool editable)
{
    m_editable = editable;
    m_addRule->setEnabled(editable);
    m_removeRule->setEnabled(editable);
    m_table->setEnabled(editable);
}

// 重建矩阵
void JobRstRulePanel::rebuildTable()
{
    m_table->clear();
    if (!m_editJob) {
        m_table->setRowCount(0);
        m_table->setColumnCount(0);
        return;
    }
    const std::vector<ObjectId> tools = m_editJob->okNgToolIds();
    const int toolCount = static_cast<int>(tools.size());
    const int ruleCount = static_cast<int>(m_editJob->resultRules.size());
    const int maxRuleCols = ruleCount > 0 ? ruleCount - 1 : 0;
    const int colCount = ColTermStart + toolCount + maxRuleCols;

    QStringList headers;
    headers << T("名称") << T("绑定工具");
    for (int i = 0; i < toolCount; ++i) {
        const ToolCfg* t = m_editJob->findTool(tools[static_cast<size_t>(i)]);
        headers << (t ? qs(t->name) : qs(tools[static_cast<size_t>(i)].toString()));
    }
    for (int i = 0; i < maxRuleCols; ++i) {
        headers << qs(m_editJob->resultRules[static_cast<size_t>(i)].name);
    }
    m_table->setColumnCount(colCount);
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setRowCount(ruleCount);

    for (int row = 0; row < ruleCount; ++row) {
        const RstRule& rule = m_editJob->resultRules[static_cast<size_t>(row)];

        QLineEdit* nameEdit = new QLineEdit(qs(rule.name), m_table);
        connect(nameEdit, SIGNAL(textEdited(QString)), this, SLOT(onRuleMetaChanged()));
        QTableWidgetItem* nameItem = new QTableWidgetItem();
        nameItem->setData(kRoleRuleId, qs(rule.id));
        m_table->setItem(row, ColName, nameItem);
        m_table->setCellWidget(row, ColName, nameEdit);

        QComboBox* bind = new QComboBox(m_table);
        bind->addItem(T("（不绑定）"), QString());
        for (size_t ti = 0; ti < tools.size(); ++ti) {
            const ToolCfg* t = m_editJob->findTool(tools[ti]);
            if (!t) {
                continue;
            }
            bind->addItem(qs(t->name), qs(t->id));
        }
        const int bindIdx = bind->findData(qs(rule.toolId));
        bind->setCurrentIndex(bindIdx >= 0 ? bindIdx : 0);
        connect(bind, SIGNAL(currentIndexChanged(int)), this, SLOT(onRuleMetaChanged()));
        m_table->setCellWidget(row, ColBindTool, bind);

        for (int col = ColTermStart; col < colCount; ++col) {
            int kind = RstTermTool;
            ObjectId sourceId;
            const bool valid = columnSource(col, row, &kind, &sourceId);
            QCheckBox* cb = new QCheckBox(m_table);
            const int state = valid
                ? rule.termState(kind, sourceId) : RstTermIgnore;
            applyTermVisual(cb, state);
            cb->setEnabled(valid && m_editable);
            if (valid && m_editable) {
                connect(cb, SIGNAL(clicked()), this, SLOT(onTermClicked()));
            }
            m_table->setCellWidget(row, col, cb);
        }
    }
    m_table->resizeColumnsToContents();
}

// 三态格单击
void JobRstRulePanel::onTermClicked()
{
    QCheckBox* cb = qobject_cast<QCheckBox*>(sender());
    if (!cb) {
        return;
    }
    int state = cb->property("termState").toInt();
    state = (state + 1) % 3;
    applyTermVisual(cb, state);
    emit changed();
}

// 名称或绑定工具变更
void JobRstRulePanel::onRuleMetaChanged()
{
    emit changed();
}

// 添加逻辑判断行
void JobRstRulePanel::onAddRule()
{
    if (!m_editJob || !m_editable) {
        return;
    }
    flushToJob(m_editJob);
    RstRule rule;
    rule.id = ObjectId::create();
    rule.name = Translate::format("逻辑判断{0}", std::to_string(static_cast<int>(m_editJob->resultRules.size()) + 1));
    m_editJob->resultRules.push_back(rule);
    if (m_editJob->resultFromRuleId.isNull()) {
        m_editJob->resultFromRuleId = rule.id;
    }
    loadFromJob(m_editJob);
    emit changed();
}

// 删除选中行
void JobRstRulePanel::onRemoveRule()
{
    if (!m_editJob || !m_editable) {
        return;
    }
    const int row = m_table->currentRow();
    if (row < 0) {
        return;
    }
    flushToJob(m_editJob);
    if (row >= static_cast<int>(m_editJob->resultRules.size())) {
        return;
    }
    const ObjectId removedId = m_editJob->resultRules[static_cast<size_t>(row)].id;
    m_editJob->resultRules.erase(m_editJob->resultRules.begin() + row);
    if (m_editJob->resultFromRuleId == removedId) {
        m_editJob->resultFromRuleId = m_editJob->resultRules.empty()
            ? ObjectId() : m_editJob->resultRules.back().id;
    }
    m_editJob->normalizeRstRules();
    loadFromJob(m_editJob);
    emit changed();
}

}

#include "moc_JobRstRulePanel.cpp"
