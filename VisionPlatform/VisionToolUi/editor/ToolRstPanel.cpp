/**
 * @file ToolRstPanel.cpp
 * @brief ToolRstPanel实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolRstPanel.h"
#include "Job.h"
#include "Solution.h"
#include "ParamKeys.h"
#include "QtConv.h"
#include "UiText.h"
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QSizePolicy>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Vision {

namespace {

enum RunStatusKind {
    RunStatusNone = 0,   ///< 未选中 / 无上下文
    RunStatusPending,    ///< 未运行
    RunStatusOk,         ///< OK
    RunStatusNg          ///< NG
};

// 运行结果标签：仅字体颜色，不改背景
static void applyRunStatusLabel(QLabel* lab, RunStatusKind kind)
{
    if (!lab) {
        return;
    }
    lab->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    switch (kind) {
    case RunStatusOk:
        lab->setStyleSheet(
            QLatin1String("QLabel { color:#2e7d32; font-weight:bold; padding:2px 0; }"));
        break;
    case RunStatusNg:
        lab->setStyleSheet(
            QLatin1String("QLabel { color:#c62828; font-weight:bold; padding:2px 0; }"));
        break;
    case RunStatusPending:
        lab->setStyleSheet(
            QLatin1String("QLabel { color:#546e7a; padding:2px 0; }"));
        break;
    default:
        lab->setStyleSheet(
            QLatin1String("QLabel { color:#757575; padding:2px 0; }"));
        break;
    }
}

// 输出表中 OK/NG 单元格着色
static void applyOkNgTableCell(QTableWidgetItem* it, bool ok)
{
    if (!it) {
        return;
    }
    if (ok) {
        it->setBackground(QBrush(QColor(0xc8, 0xe6, 0xc9)));
        it->setForeground(QBrush(QColor(0x1b, 0x5e, 0x20)));
    } else {
        it->setBackground(QBrush(QColor(0xff, 0xcd, 0xd2)));
        it->setForeground(QBrush(QColor(0xb7, 0x1c, 0x1c)));
    }
    QFont f = it->font();
    f.setBold(true);
    it->setFont(f);
}

} // namespace

// 只读单元格
static QTableWidgetItem* roCell(const QString& text, bool rightNum)
{
    QTableWidgetItem* it = new QTableWidgetItem(text);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    if (rightNum) {
        it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    return it;
}

// 参数值转显示文本
static QString paramText(const ParamValue& v)
{
    if (v.isNull()) {
        return QString();
    }
    return qs(v.toString());
}

// 运行结果只读区
ToolRstPanel::ToolRstPanel(QWidget* parent)
    : QWidget(parent)
    , m_toolLab(nullptr)
    , m_statusLab(nullptr)
    , m_msgLab(nullptr)
    , m_timeLab(nullptr)
    , m_outTable(nullptr)
{
    m_toolLab = new QLabel(this);
    m_toolLab->setWordWrap(true);
    m_toolLab->setObjectName(QLatin1String("section"));

    QGroupBox* runBox = new QGroupBox(T("运行结果"), this);
    QVBoxLayout* runLay = new QVBoxLayout(runBox);
    runLay->setContentsMargins(8, 6, 8, 6);
    runLay->setSpacing(2);
    m_statusLab = new QLabel(runBox);
    m_msgLab = new QLabel(runBox);
    m_msgLab->setWordWrap(true);
    m_msgLab->setObjectName(QLatin1String("hint"));
    m_timeLab = new QLabel(runBox);
    m_timeLab->setObjectName(QLatin1String("hint"));
    m_timeLab->hide();
    runLay->addWidget(m_statusLab);
    runLay->addWidget(m_msgLab);
    runBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QGroupBox* outBox = new QGroupBox(T("输出数据"), this);
    QVBoxLayout* outLay = new QVBoxLayout(outBox);
    outLay->setContentsMargins(8, 6, 8, 6);
    m_outTable = new QTableWidget(outBox);
    m_outTable->setColumnCount(2);
    QStringList hh;
    hh << T("项") << T("值");
    m_outTable->setHorizontalHeaderLabels(hh);
    m_outTable->verticalHeader()->setVisible(false);
    m_outTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_outTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_outTable->setAlternatingRowColors(true);
    m_outTable->horizontalHeader()->setStretchLastSection(true);
    m_outTable->verticalHeader()->setDefaultSectionSize(22);
    m_outTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_outTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    outLay->addWidget(m_outTable, 1);

    QSplitter* split = new QSplitter(Qt::Vertical, this);
    split->addWidget(runBox);
    split->addWidget(outBox);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setCollapsible(0, false);
    split->setCollapsible(1, false);
    split->setChildrenCollapsible(false);
    split->setSizes(QList<int>() << 52 << 320);

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    lay->addWidget(m_toolLab);
    lay->addWidget(split, 1);
}

// 填输出键值表
void ToolRstPanel::fillOutputTable(const ParamMap& values)
{
    if (!m_outTable) {
        return;
    }
    m_outTable->setRowCount(0);
    const std::vector<std::string> keys = values.keys();
    m_outTable->setRowCount(static_cast<int>(keys.size()));
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        const std::string& k = keys[static_cast<size_t>(i)];
        const ParamValue v = values.value(k);
        m_outTable->setItem(i, 0, roCell(qs(k), false));
        m_outTable->setItem(i, 1, roCell(paramText(v), true));
    }
    m_outTable->resizeRowsToContents();
}

// 按工具与流程缓存刷新运行结果与输出
void ToolRstPanel::refresh(Solution* sol, ToolCfg* tool, Job* job, const ToolOut* lastRun,
                              bool afterJobRun)
{
    if (!tool) {
        m_toolLab->setText(T("选中工具"));
        m_statusLab->setText(T("状态：") + T("未选中"));
        applyRunStatusLabel(m_statusLab, RunStatusNone);
        m_msgLab->clear();
        fillOutputTable(ParamMap());
        return;
    }

    m_toolLab->setText(qs(tool->name) + QLatin1String("  (") + qs(tool->toolName) + QLatin1Char(')'));

    const bool useLast = lastRun && lastRun->toolId == tool->id;
    const bool hasCache = job && job->hasCache(tool->id);
    ParamMap values;
    if (useLast) {
        values = lastRun->values;
    } else if (job) {
        values = job->cachedValues(tool->id);
    }

    bool ok = false;
    bool hasStatus = false;
    QString msg;
    int ms = 0;
    if (useLast) {
        hasStatus = true;
        ok = lastRun->ok;
        msg = qs(lastRun->message);
        ms = lastRun->elapsedMs;
    } else if (afterJobRun && job && tool && job->hasToolRun(tool->id)) {
        hasStatus = true;
        bool ranOk = false;
        job->cachedToolOk(tool->id, &ranOk);
        ok = ranOk;
        if (paramHas(values, ValueKey::Ok)) {
            ok = paramGet(values, ValueKey::Ok).toBool(ok);
        } else if (paramHas(values, ValueKey::Pass)) {
            ok = paramGet(values, ValueKey::Pass).toBool(ok);
        }
        msg = T("本流程已运行");
    } else if (hasCache) {
        hasStatus = true;
        if (paramHas(values, ValueKey::Ok)) {
            ok = paramGet(values, ValueKey::Ok).toBool(false);
        } else if (paramHas(values, ValueKey::Pass)) {
            ok = paramGet(values, ValueKey::Pass).toBool(false);
        } else {
            ok = true;
        }
        msg = T("来自上次运行缓存");
    }

    if (!hasStatus) {
        m_statusLab->setText(T("状态：") + T("未运行"));
        applyRunStatusLabel(m_statusLab, RunStatusPending);
        m_msgLab->setText(T("点「运行本工具」或「运行本流程」查看结果。"));
    } else {
        const QString stText = ok ? T("OK") : T("NG");
        QString line = T("状态：") + stText;
        if (ms > 0) {
            line += QLatin1String("    ") + T("耗时：") + QString::number(ms) + T(" ms");
        }
        m_statusLab->setText(line);
        applyRunStatusLabel(m_statusLab, ok ? RunStatusOk : RunStatusNg);
        m_msgLab->setText(msg.isEmpty() ? T("（无说明）") : msg);
    }
    fillOutputTable(values);
    // 单工具视图：若输出含 ok/pass，着色对应行
    if (m_outTable && hasStatus) {
        for (int r = 0; r < m_outTable->rowCount(); ++r) {
            QTableWidgetItem* keyIt = m_outTable->item(r, 0);
            QTableWidgetItem* valIt = m_outTable->item(r, 1);
            if (!keyIt || !valIt) {
                continue;
            }
            const QString key = keyIt->text();
            ValueKey vk;
            if (valueKeyFromName(ss(key), &vk)
                && (vk == ValueKey::Ok || vk == ValueKey::Pass)) {
                applyOkNgTableCell(valIt, valIt->text() == QLatin1String("1")
                                         || valIt->text().compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
                                         || valIt->text() == T("OK"));
            }
        }
    }
}

// 设计态：汇总整 JobCfg 各工具的运行结果与输出
void ToolRstPanel::refreshJob(Solution* sol, const JobCfg* jobConfig, Job* job,
                                 const ToolOut* jobOut)
{
    if (!jobConfig) {
        m_toolLab->setText(T("选中流程"));
        m_statusLab->setText(T("状态：") + T("未选中"));
        applyRunStatusLabel(m_statusLab, RunStatusNone);
        m_msgLab->clear();
        fillOutputTable(ParamMap());
        return;
    }

    m_toolLab->setText(T("流程 ") + qs(jobConfig->name));

    int ranCount = 0;
    int okCount = 0;
    const std::vector<ObjectId> order = jobConfig->execOrder();
    for (size_t i = 0; i < order.size(); ++i) {
        if (job && job->hasToolRun(order[i])) {
            ++ranCount;
            bool tok = false;
            if (job->cachedToolOk(order[i], &tok) && tok) {
                ++okCount;
            }
        }
    }

    bool hasStatus = false;
    bool jobOk = false;
    QString msg;
    int ms = 0;
    if (jobOut) {
        hasStatus = true;
        jobOk = jobOut->ok;
        msg = qs(jobOut->message);
        ms = jobOut->elapsedMs;
    } else if (ranCount > 0) {
        hasStatus = true;
        jobOk = (okCount == ranCount);
        msg = T("来自上次运行缓存");
    }

    if (!hasStatus) {
        m_statusLab->setText(T("状态：") + T("未运行"));
        applyRunStatusLabel(m_statusLab, RunStatusPending);
        m_msgLab->setText(T("点「运行本流程」查看本流程全部工具输出。"));
    } else {
        const QString stText = jobOk ? T("OK") : T("NG");
        QString line = T("状态：") + stText;
        if (ms > 0) {
            line += QLatin1String("    ") + T("耗时：") + QString::number(ms) + T(" ms");
        }
        line += QString::fromUtf8("    (") + QString::number(okCount)
            + QLatin1Char('/') + QString::number(ranCount) + T(" 工具 OK") + QLatin1Char(')');
        m_statusLab->setText(line);
        applyRunStatusLabel(m_statusLab, jobOk ? RunStatusOk : RunStatusNg);
        m_msgLab->setText(msg.isEmpty() ? T("（无说明）") : msg);
    }

    // 输出表：按执行序列出各工具全部输出键值
    if (m_outTable) {
        m_outTable->setRowCount(0);
        int row = 0;
        for (size_t i = 0; i < order.size(); ++i) {
            const ToolCfg* t = jobConfig->findTool(order[i]);
            if (!t || !job || !job->hasToolRun(order[i])) {
                continue;
            }
            bool tok = false;
            job->cachedToolOk(order[i], &tok);
            m_outTable->insertRow(row);
            m_outTable->setItem(row, 0, roCell(qs(t->name) + QLatin1Char(' ') + QString(QChar(0x00B7)) + QLatin1Char(' ') + T("状态"), false));
            QTableWidgetItem* stCell = roCell(tok ? T("OK") : T("NG"), false);
            applyOkNgTableCell(stCell, tok);
            m_outTable->setItem(row, 1, stCell);
            ++row;

            const ParamMap values = job->cachedValues(order[i]);
            const std::vector<std::string> keys = values.keys();
            for (size_t k = 0; k < keys.size(); ++k) {
                const std::string& key = keys[k];
                ValueKey vk;
                if (valueKeyFromName(key, &vk)
                    && (vk == ValueKey::Ok || vk == ValueKey::Pass)) {
                    continue;
                }
                m_outTable->insertRow(row);
                m_outTable->setItem(row, 0,
                                    roCell(qs(t->name) + QLatin1Char(' ') + QString(QChar(0x00B7)) + QLatin1Char(' ') + qs(key), false));
                m_outTable->setItem(row, 1, roCell(paramText(values.value(key)), true));
                ++row;
            }
        }
        m_outTable->resizeRowsToContents();
    }
}

}
