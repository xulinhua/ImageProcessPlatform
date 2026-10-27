/**
 * @file ParaAdaptDlg.cpp
 * @brief ParaAdaptDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ParaAdaptDlg.h"
#include "AlgLibKind.h"
#include "PluginHost.h"
#include "Solution.h"
#include "ToolBase.h"
#include "TextEncoding.h"
#include "ToolParaBase.h"
#include "VisionVersion.h"
#include <QAbstractItemView>
#include <QColor>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <map>
#include <string>
#include <vector>

namespace Vision {

namespace {
QString u(const char* s) { return QString::fromLocal8Bit(s); }

QString showVer(const std::string& s)
{
    return QString::fromUtf8(s.empty() ? "0.0.0.0" : s.c_str());
}

void paintRow(QTableWidgetItem* g, QTableWidgetItem* k, QTableWidgetItem* b, QTableWidgetItem* a, int mark)
{
    QColor color(232, 245, 233);
    if (mark == 1) {
        color = QColor(255, 235, 238);
    } else if (mark == 2) {
        color = QColor(255, 243, 205);
    }
    g->setBackground(color);
    k->setBackground(color);
    b->setBackground(color);
    a->setBackground(color);
}

void addValueRow(QTableWidget* table, const char* group, const QString& key,
                 const QString& before, const QString& after, bool inMap, bool dirty)
{
    const int row = table->rowCount();
    table->insertRow(row);
    QTableWidgetItem* g = new QTableWidgetItem(u(group));
    QTableWidgetItem* k = new QTableWidgetItem(key);
    QTableWidgetItem* b = new QTableWidgetItem(before);
    QTableWidgetItem* a = new QTableWidgetItem(after);
    table->setItem(row, 0, g);
    table->setItem(row, 1, k);
    table->setItem(row, 2, b);
    table->setItem(row, 3, a);
    int mark = 0;
    if (before != after) {
        mark = 1;
    } else if (!inMap) {
        mark = 2;
    }
    if (dirty && inMap && before == after) {
        mark = 2;
    }
    paintRow(g, k, b, a, mark);
}

void addMapRows(QTableWidget* table, const char* group, const ParamMap& before, const ParamMap& after, bool dirty)
{
    std::map<std::string, int> keys;
    const std::vector<std::string> bk = before.keys();
    const std::vector<std::string> ak = after.keys();
    for (size_t i = 0; i < bk.size(); ++i) {
        keys[bk[i]] = 1;
    }
    for (size_t i = 0; i < ak.size(); ++i) {
        keys[ak[i]] = 1;
    }
    for (std::map<std::string, int>::const_iterator it = keys.begin(); it != keys.end(); ++it) {
        const bool inMap = before.contains(it->first);
        const QString bv = inMap ? QString::fromUtf8(before.value(it->first).toString().c_str()) : QString::fromLatin1("-");
        const QString av = after.contains(it->first)
            ? QString::fromUtf8(after.value(it->first).toString().c_str()) : QString::fromLatin1("-");
        addValueRow(table, group, QString::fromUtf8(it->first.c_str()), bv, av, inMap, dirty && inMap);
    }
}

void addJudgeRows(QTableWidget* table, const std::vector<JudgeParam>& before, const std::vector<JudgeParam>& after)
{
    const size_t n = before.size() > after.size() ? before.size() : after.size();
    for (size_t i = 0; i < n; ++i) {
        const int row = table->rowCount();
        table->insertRow(row);
        QString btext;
        QString atext;
        if (i < before.size()) {
            btext = QString::fromUtf8(before[i].name.c_str())
                + QLatin1String(" [") + QString::number(before[i].minVal)
                + QLatin1String(",") + QString::number(before[i].maxVal) + QLatin1String("]");
        }
        if (i < after.size()) {
            atext = QString::fromUtf8(after[i].name.c_str())
                + QLatin1String(" [") + QString::number(after[i].minVal)
                + QLatin1String(",") + QString::number(after[i].maxVal) + QLatin1String("]");
        }
        QTableWidgetItem* g = new QTableWidgetItem(u("判断"));
        QTableWidgetItem* k = new QTableWidgetItem(QString::number(int(i + 1)));
        QTableWidgetItem* b = new QTableWidgetItem(btext);
        QTableWidgetItem* a = new QTableWidgetItem(atext);
        table->setItem(row, 0, g);
        table->setItem(row, 1, k);
        table->setItem(row, 2, b);
        table->setItem(row, 3, a);
        paintRow(g, k, b, a, btext == atext ? 0 : 1);
    }
}
}

class ParaCompareDlg : public QDialog {
public:
    ParaCompareDlg(const QString& station, const QString& job, const QString& toolName,
                   const ToolCfg& tool, bool dirty, QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle(u("参数对比"));
        resize(860, 560);
        const std::string saved = tool.param.paraVersion.empty() ? std::string("0.0.0.0") : tool.param.paraVersion;
        std::string current = "1.0.0.0";
        std::string toolNow;
        std::string algNow;
        ToolParam after = tool.param;
        std::string detail;
        QLabel* logLabel = nullptr;
        ToolBase* proto = ToolRegistry::instance().find(tool.info.toolName);
        if (proto) {
            toolNow = proto->version();
            if (proto->toolParam()) {
                current = proto->toolParam()->paraVersion();
            }
            ToolBase* inst = proto->clone();
            inst->adaptParam(after, tool.param.paraVersion, current, &detail);
            inst->destroy();
            const std::string wantLib = algLibKindTitle(tool.param.algLib);
            const std::vector<ToolLibReg> regs = ToolLibMgr::instance().registrations();
            std::vector<VerNote> algNotes;
            for (size_t ri = 0; ri < regs.size(); ++ri) {
                if (regs[ri].kind == "algo" && regs[ri].toolName == tool.info.toolName
                    && (wantLib.empty() || regs[ri].algLib == wantLib)) {
                    algNow = regs[ri].version;
                    algNotes = regs[ri].notes;
                    break;
                }
            }
            const std::string toolLog = verNotesBetween(proto->versionNotes(), tool.info.toolVersion, toolNow);
            const std::string algLog = verNotesBetween(algNotes, tool.param.algVersion, algNow);
            QString logText;
            if (!toolLog.empty()) {
                logText += QString::fromLocal8Bit("工具版本\n");
                logText += QString::fromUtf8(toolLog.c_str());
            }
            if (!algLog.empty()) {
                if (!logText.isEmpty()) {
                    logText += QLatin1String("\n");
                }
                logText += QString::fromLocal8Bit("算法版本\n");
                logText += QString::fromUtf8(algLog.c_str());
            }
            if (!logText.isEmpty()) {
                logLabel = new QLabel(logText, this);
                logLabel->setWordWrap(true);
            }
        }
        QLabel* head = new QLabel(this);
        head->setText(u("工站") + QLatin1String(": ") + station
            + QLatin1String("    ") + u("流程") + QLatin1String(": ") + job
            + QLatin1String("    ") + u("工具") + QLatin1String(": ") + toolName
            + QLatin1String(" (") + QString::fromUtf8(tool.info.toolName.c_str()) + QLatin1String(")"));
        QLabel* ver = new QLabel(this);
        ver->setText(u("参数版本") + QLatin1String(": ")
            + showVer(saved) + QLatin1String("  ->  ") + showVer(current)
            + QLatin1String("    ") + u("工具版本") + QLatin1String(": ")
            + QString::fromUtf8(tool.info.toolVersion.empty() ? "-" : tool.info.toolVersion.c_str())
            + QLatin1String("  ->  ") + QString::fromUtf8(toolNow.empty() ? "-" : toolNow.c_str())
            + QLatin1String("    ") + u("算法版本") + QLatin1String(": ")
            + showVer(tool.param.algVersion) + QLatin1String("  ->  ") + showVer(algNow));
        QTableWidget* table = new QTableWidget(this);
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels(QStringList()
            << u("分组")
            << u("参数名")
            << u("升级前")
            << u("升级后"));
        table->horizontalHeader()->setStretchLastSection(true);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        addValueRow(table, "运行", u("deviceId"),
                    QString::fromUtf8(tool.param.deviceId.toString().c_str()),
                    QString::fromUtf8(after.deviceId.toString().c_str()),
                    !tool.param.deviceId.isNull(), dirty);
        addValueRow(table, "运行", u("algLib"),
                    QString::number(static_cast<int>(tool.param.algLib)),
                    QString::number(static_cast<int>(after.algLib)),
                    true, dirty);
        if (proto) {
            const std::vector<ParamField> fields = proto->paramFields();
            for (size_t fi = 0; fi < fields.size(); ++fi) {
                const std::string& key = fields[fi].key;
                if (key == "deviceId" || tool.param.run.contains(key) || after.run.contains(key)
                    || tool.param.learn.contains(key) || after.learn.contains(key)) {
                    continue;
                }
                const QString title = QString::fromUtf8(Vision::acpToUtf8(fields[fi].title).c_str());
                const QString shown = title.isEmpty() ? QString::fromUtf8(key.c_str())
                    : (title + QLatin1String(" (") + QString::fromUtf8(key.c_str()) + QLatin1String(")"));
                addValueRow(table, "运行", shown,
                            QString::fromLatin1("-"),
                            QString::fromUtf8(fields[fi].def.toString().c_str()),
                            false, dirty);
            }
        }
        addMapRows(table, "学习", tool.param.learn, after.learn, dirty);
        addMapRows(table, "运行", tool.param.run, after.run, dirty);
        addJudgeRows(table, tool.param.judge, after.judge);
        QLabel* legend = new QLabel(this);
        legend->setText(u("绿色相同，红色不同，黄色是已改未保存或尚未写入参数表"));
        table->resizeColumnsToContents();
        QPushButton* closeBtn = new QPushButton(u("关闭"), this);
        connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
        QHBoxLayout* buttons = new QHBoxLayout();
        buttons->addStretch();
        buttons->addWidget(closeBtn);
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->addWidget(head);
        lay->addWidget(ver);
        if (logLabel) {
            lay->addWidget(logLabel);
        }
        lay->addWidget(legend);
        if (!detail.empty()) {
            lay->addWidget(new QLabel(QString::fromUtf8(detail.c_str()), this));
        }
        lay->addWidget(table, 1);
        lay->addLayout(buttons);
    }
};

ParaAdaptDlg::ParaAdaptDlg(Solution* sol, QWidget* parent)
    : QDialog(parent)
    , m_sol(sol)
    , m_table(nullptr)
{
    setWindowTitle(u("参数版本"));
    resize(1180, 560);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels(QStringList()
        << u("工站")
        << u("流程")
        << u("工具")
        << u("注册名")
        << u("参数版本")
        << u("工具版本")
        << u("算法版本")
        << u("状态"));
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(onOpenCompare(QTableWidgetItem*)));
    QPushButton* up = new QPushButton(u("升级到当前"), this);
    QPushButton* down = new QPushButton(u("降级到指定版本"), this);
    QPushButton* see = new QPushButton(u("只看差异"), this);
    connect(up, SIGNAL(clicked()), this, SLOT(onUpgrade()));
    connect(down, SIGNAL(clicked()), this, SLOT(onDowngrade()));
    connect(see, SIGNAL(clicked()), this, SLOT(onPreview()));
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(up);
    buttons->addWidget(down);
    buttons->addWidget(see);
    buttons->addStretch();
    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->addWidget(m_table);
    lay->addLayout(buttons);
    reload();
}

void ParaAdaptDlg::reload()
{
    m_table->setRowCount(0);
    if (!m_sol) {
        return;
    }
    int row = 0;
    for (size_t si = 0; si < m_sol->stations.size(); ++si) {
        StationCfg& st = m_sol->stations[si];
        for (size_t ji = 0; ji < st.jobs.size(); ++ji) {
            JobCfg& job = st.jobs[ji];
            for (size_t ti = 0; ti < job.tools.size(); ++ti) {
                ToolCfg& tool = job.tools[ti];
                m_table->insertRow(row);
                QTableWidgetItem* stationItem = new QTableWidgetItem(QString::fromUtf8(st.name.c_str()));
                stationItem->setData(Qt::UserRole, int(si));
                stationItem->setData(Qt::UserRole + 1, int(ji));
                stationItem->setData(Qt::UserRole + 2, int(ti));
                m_table->setItem(row, 0, stationItem);
                m_table->setItem(row, 1, new QTableWidgetItem(QString::fromUtf8(job.name.c_str())));
                const QString toolTitle = tool.info.name.empty()
                    ? QString::fromUtf8(tool.info.toolName.c_str())
                    : QString::fromUtf8(tool.info.name.c_str());
                m_table->setItem(row, 2, new QTableWidgetItem(toolTitle));
                m_table->setItem(row, 3, new QTableWidgetItem(QString::fromUtf8(tool.info.toolName.c_str())));
                const std::string saved = tool.param.paraVersion.empty() ? std::string("0.0.0.0") : tool.param.paraVersion;
                std::string current = "1.0.0.0";
                std::string toolNow;
                std::string algNow;
                std::string state = "已一致";
                bool need = false;
                ToolBase* proto = ToolRegistry::instance().find(tool.info.toolName);
                if (proto) {
                    toolNow = proto->version();
                    if (proto->toolParam()) {
                        current = proto->toolParam()->paraVersion();
                    }
                    std::vector<std::string> names;
                    if (proto->toolParam()) {
                        proto->toolParam()->preview(saved, current, &names, &need);
                    }
                    const std::string wantLib = algLibKindTitle(tool.param.algLib);
                    const std::vector<ToolLibReg> regs = ToolLibMgr::instance().registrations();
                    for (size_t ri = 0; ri < regs.size(); ++ri) {
                        if (regs[ri].kind == "algo" && regs[ri].toolName == tool.info.toolName
                            && (wantLib.empty() || regs[ri].algLib == wantLib)) {
                            algNow = regs[ri].version;
                            break;
                        }
                    }
                    const int cmp = ToolParaBase::compareVersion(saved, current);
                    if (cmp == 0) {
                        state = "已一致";
                    } else if (cmp > 0) {
                        state = "文件比程序新";
                    } else if (need || names.empty()) {
                        state = "需确认";
                    } else {
                        state = "可自动升级";
                    }
                }
                m_table->setItem(row, 4, new QTableWidgetItem(showVer(saved) + QLatin1String("  ->  ") + showVer(current)));
                const std::string savedTool = tool.info.toolVersion.empty() ? std::string("-") : tool.info.toolVersion;
                m_table->setItem(row, 5, new QTableWidgetItem(QString::fromUtf8(savedTool.c_str()) + QLatin1String("  ->  ") + showVer(toolNow)));
                const std::string savedAlg = tool.param.algVersion.empty() ? std::string("-") : tool.param.algVersion;
                m_table->setItem(row, 6, new QTableWidgetItem(QString::fromUtf8(savedAlg.c_str()) + QLatin1String("  ->  ") + showVer(algNow)));
                m_table->setItem(row, 7, new QTableWidgetItem(u(state.c_str())));
                ++row;
            }
        }
    }
    m_table->resizeColumnsToContents();
}

bool ParaAdaptDlg::applyRow(int row, const std::string& target)
{
    if (!m_sol || row < 0) {
        return false;
    }
    QTableWidgetItem* item = m_table->item(row, 0);
    if (!item) {
        return false;
    }
    const int si = item->data(Qt::UserRole).toInt();
    const int ji = item->data(Qt::UserRole + 1).toInt();
    const int ti = item->data(Qt::UserRole + 2).toInt();
    if (si < 0 || ji < 0 || ti < 0) {
        return false;
    }
    ToolCfg& tool = m_sol->stations[size_t(si)].jobs[size_t(ji)].tools[size_t(ti)];
    ToolBase* proto = ToolRegistry::instance().find(tool.info.toolName);
    if (!proto) {
        return false;
    }
    ToolBase* inst = proto->clone();
    std::string detail;
    std::string goal = target;
    if (goal.empty() && inst->toolParam()) {
        goal = inst->toolParam()->paraVersion();
    }
    const bool ok = inst->adaptParam(tool.param, tool.param.paraVersion, goal, &detail);
    if (ok) {
        tool.info.toolVersion = inst->version();
        m_sol->dirty = true;
    }
    inst->destroy();
    if (!ok) {
        QMessageBox::warning(this, u("适配失败"), QString::fromUtf8(detail.c_str()));
    }
    return ok;
}

void ParaAdaptDlg::onUpgrade()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, u("参数版本"), u("没有选中工具"));
        return;
    }
    if (applyRow(row, std::string())) {
        QMessageBox::information(this, u("参数版本"), u("已写回方案"));
        reload();
    }
}

void ParaAdaptDlg::onDowngrade()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, u("参数版本"), u("没有选中工具"));
        return;
    }
    bool ok = false;
    const QString ver = QInputDialog::getText(this, u("降级到指定版本"), u("目标版本"), QLineEdit::Normal, QString::fromLatin1("1.0.0.0"), &ok);
    if (!ok || ver.trimmed().isEmpty()) {
        return;
    }
    if (applyRow(row, ver.trimmed().toStdString())) {
        QMessageBox::information(this, u("参数版本"), u("已写回方案"));
        reload();
    }
}

void ParaAdaptDlg::onPreview()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, u("参数版本"), u("没有选中工具"));
        return;
    }
    onOpenCompare(m_table->item(row, 0));
}

void ParaAdaptDlg::onOpenCompare(QTableWidgetItem* item)
{
    if (!m_sol || !item) {
        return;
    }
    const int row = item->row();
    QTableWidgetItem* head = m_table->item(row, 0);
    if (!head) {
        return;
    }
    const int si = head->data(Qt::UserRole).toInt();
    const int ji = head->data(Qt::UserRole + 1).toInt();
    const int ti = head->data(Qt::UserRole + 2).toInt();
    ToolCfg& tool = m_sol->stations[size_t(si)].jobs[size_t(ji)].tools[size_t(ti)];
    const QString station = m_table->item(row, 0) ? m_table->item(row, 0)->text() : QString();
    const QString job = m_table->item(row, 1) ? m_table->item(row, 1)->text() : QString();
    const QString toolTitle = m_table->item(row, 2) ? m_table->item(row, 2)->text() : QString();
    ParaCompareDlg dlg(station, job, toolTitle, tool, m_sol->dirty, this);
    dlg.exec();
}

}
#include "moc_ParaAdaptDlg.cpp"
