/**
 * @file RecheckWorkspace.cpp
 * @brief RecheckWorkspace实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RecheckWorkspace.h"
#include "AppPaths.h"
#include "FileGrabber.h"
#include "Job.h"
#include "QtConv.h"
#include "Session.h"
#include "Station.h"
#include "Theme.h"
#include "ThreadPool.h"
#include "UiText.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QFile>
#include <cstdio>
#include "StageTimer.h"

namespace Vision {

namespace {


// JSON 字符串转义
QString jsonEsc(const QString& s)
{
    QString o;
    o.reserve(s.size() + 8);
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        switch (c.unicode()) {
        case '\\':
            o += QLatin1String("\\\\");
            break;
        case '"':
            o += QLatin1String("\\\"");
            break;
        case '\n':
            o += QLatin1String("\\n");
            break;
        case '\r':
            o += QLatin1String("\\r");
            break;
        case '\t':
            o += QLatin1String("\\t");
            break;
        default:
            o += c;
            break;
        }
    }
    return o;
}

} // namespace

// 复检工作区：配置条 + 结果表 + 详情图
RecheckWorkspace::RecheckWorkspace(Session* session, Solution* solution, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_sol(solution)
    , m_station(nullptr)
    , m_queueIndex(0)
    , m_running(false)
    , m_paused(false)
    , m_stopRequested(false)
    , m_stepBusy(false)
    , m_canRun(true)
{
    // 配置区：目录、工站、流程，运行按钮、进度、统计、过滤与导出；bindTrKey 让切换语言时重译
    m_folder = new QLineEdit(this);
    m_folder->setPlaceholderText(T("图像文件夹"));
    m_folder->setProperty("vpTr", QByteArray("图像文件夹"));
    QPushButton* browse = new QPushButton(T("浏览…"), this);
    bindTrKey(browse, "浏览…");
    connect(browse, SIGNAL(clicked()), this, SLOT(onBrowseFolder()));

    m_stationCombo = new QComboBox(this);
    m_jobCombo = new QComboBox(this);
    connect(m_stationCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onStationChanged()));

    m_fileCount = new QLabel(T("已扫描 0 张"), this);
    bindTrKey(m_fileCount, "已扫描 0 张");
    m_fileCount->setObjectName(QLatin1String("hint"));

    m_btnStart = new QPushButton(T("开始复检"), this);
    bindTrKey(m_btnStart, "开始复检");
    setButtonRole(m_btnStart, "primary");
    connect(m_btnStart, SIGNAL(clicked()), this, SLOT(onStart()));
    m_btnPause = new QPushButton(T("暂停"), this);
    bindTrKey(m_btnPause, "暂停");
    m_btnPause->setEnabled(false);
    connect(m_btnPause, SIGNAL(clicked()), this, SLOT(onPauseResume()));
    m_btnStop = new QPushButton(T("停止"), this);
    bindTrKey(m_btnStop, "停止");
    m_btnStop->setEnabled(false);
    connect(m_btnStop, SIGNAL(clicked()), this, SLOT(onStop()));

    m_progress = new QProgressBar(this);
    m_progress->setMinimum(0);
    m_progress->setMaximum(100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setFormat(QLatin1String("%v/%m"));

    m_stats = new QLabel(T("OK 0  NG 0  失败 0"), this);
    bindTrKey(m_stats, "OK 0  NG 0  失败 0");
    m_filter = new QComboBox(this);
    m_filter->addItem(T("全部"), 0);
    m_filter->addItem(T("问题"), 1);
    m_filter->addItem(T("仅 NG"), 2);
    m_filter->addItem(T("仅失败"), 3);
    connect(m_filter, SIGNAL(currentIndexChanged(int)), this, SLOT(onFilterChanged()));
    m_btnExport = new QPushButton(T("导出 CSV…"), this);
    bindTrKey(m_btnExport, "导出 CSV…");
    connect(m_btnExport, SIGNAL(clicked()), this, SLOT(onExportCsv()));
    m_btnReport = new QPushButton(T("保存报告…"), this);
    bindTrKey(m_btnReport, "保存报告…");
    connect(m_btnReport, SIGNAL(clicked()), this, SLOT(onSaveReport()));
    m_btnDebug = new QPushButton(T("打开设计调试"), this);
    bindTrKey(m_btnDebug, "打开设计调试");
    connect(m_btnDebug, SIGNAL(clicked()), this, SLOT(onOpenDesignDebug()));

    QHBoxLayout* cfg1 = new QHBoxLayout();
    cfg1->addWidget(new QLabel(T("图像目录"), this));
    cfg1->addWidget(m_folder, 1);
    cfg1->addWidget(browse);
    cfg1->addWidget(new QLabel(T("工站"), this));
    cfg1->addWidget(m_stationCombo);
    cfg1->addWidget(new QLabel(T("流程"), this));
    cfg1->addWidget(m_jobCombo);

    QHBoxLayout* cfg2 = new QHBoxLayout();
    cfg2->addWidget(m_btnStart);
    cfg2->addWidget(m_btnPause);
    cfg2->addWidget(m_btnStop);
    cfg2->addWidget(m_progress, 1);
    cfg2->addWidget(m_stats);
    cfg2->addWidget(m_fileCount);
    cfg2->addWidget(new QLabel(T("过滤"), this));
    cfg2->addWidget(m_filter);
    cfg2->addWidget(m_btnExport);
    cfg2->addWidget(m_btnReport);
    cfg2->addWidget(m_btnDebug);

    QFrame* cfgCard = makeCard(this);
    QVBoxLayout* cfgLay = new QVBoxLayout(cfgCard);
    cfgLay->setContentsMargins(8, 8, 8, 8);
    cfgLay->setSpacing(8);
    cfgLay->addLayout(cfg1);
    cfgLay->addLayout(cfg2);

    // 左：每张图一行的结果表；右：选中行的结果图与各工具明细
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    QStringList headers;
    headers << T("#") << T("文件名") << T("判定") << T("耗时ms") << T("摘要");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    connect(m_table, SIGNAL(itemSelectionChanged()), this, SLOT(onRowSelected()));

    m_detailTitle = new QLabel(T("详情（选中结果行）"), this);
    bindTrKey(m_detailTitle, "详情（选中结果行）");
    m_detailTitle->setObjectName(QLatin1String("section"));
    m_view = new HsCtrl::HsImageWindow(this);
    m_view->setWindowMode(HsCtrl::WindowRuntime);
    m_view->setOverlayToolbarVisible(true);
    m_view->setDrawTool(HsCtrl::ToolBrowse);

    m_toolTable = new QTableWidget(this);
    m_toolTable->setColumnCount(4);
    QStringList th;
    th << T("工具") << T("判定") << T("耗时ms") << T("说明");
    m_toolTable->setHorizontalHeaderLabels(th);
    m_toolTable->horizontalHeader()->setStretchLastSection(true);
    m_toolTable->verticalHeader()->setVisible(false);
    m_toolTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_toolTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_toolTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_toolTable->setMaximumHeight(160);
    connect(m_toolTable, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(onToolDouble(int, int)));

    QWidget* detail = new QWidget(this);
    QVBoxLayout* detailLay = new QVBoxLayout(detail);
    detailLay->setContentsMargins(0, 0, 0, 0);
    detailLay->addWidget(m_detailTitle);
    detailLay->addWidget(m_view, 1);
    detailLay->addWidget(m_toolTable);

    QSplitter* split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(m_table);
    split->addWidget(detail);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);
    root->addWidget(cfgCard);
    root->addWidget(split, 1);

    reloadCombos();
    updateRunButtons();
}

// 刷新工站/流程下拉
void RecheckWorkspace::reloadCombos()
{
    const QString keepSt = m_stationCombo->currentData().toString();
    m_stationCombo->blockSignals(true);
    m_stationCombo->clear();
    if (m_sol) {
        for (size_t i = 0; i < m_sol->stations.size(); ++i) {
            const StationCfg& st = m_sol->stations[i];
            m_stationCombo->addItem(qs(st.stationCode) + QLatin1String(" ") + qs(st.name),
                                    qs(st.id));
        }
    }
    int idx = m_stationCombo->findData(keepSt);
    if (idx < 0) {
        idx = 0;
    }
    if (m_stationCombo->count() > 0) {
        m_stationCombo->setCurrentIndex(idx);
    }
    m_stationCombo->blockSignals(false);
    onStationChanged();
}

// 绑定工站运行实例
void RecheckWorkspace::setStation(Station* station)
{
    m_station = station;
}

// 运行权限
void RecheckWorkspace::setRunPerms(bool canRun)
{
    m_canRun = canRun;
    updateRunButtons();
}

// 当前工站配置
StationCfg* RecheckWorkspace::currentStationConfig() const
{
    if (!m_sol) {
        return 0;
    }
    return m_sol->findStationConfig(oid(m_stationCombo->currentData().toString()));
}

// 当前流程配置
JobCfg* RecheckWorkspace::currentJobConfig() const
{
    if (!m_sol) {
        return 0;
    }
    return m_sol->findJobConfig(oid(m_jobCombo->currentData().toString()));
}

// 工站变化 → 刷新流程列表
void RecheckWorkspace::onStationChanged()
{
    const QString keepJob = m_jobCombo->currentData().toString();
    m_jobCombo->clear();
    StationCfg* st = currentStationConfig();
    if (!st) {
        return;
    }
    for (size_t i = 0; i < st->jobs.size(); ++i) {
        const JobCfg& j = st->jobs[i];
        m_jobCombo->addItem(qs(j.name), qs(j.id));
    }
    int idx = m_jobCombo->findData(keepJob);
    if (idx < 0) {
        idx = 0;
    }
    if (m_jobCombo->count() > 0) {
        m_jobCombo->setCurrentIndex(idx);
    }
}

// 浏览文件夹并预扫描张数
void RecheckWorkspace::onBrowseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, T("选择图像文件夹"), m_folder->text());
    if (dir.isEmpty()) {
        return;
    }
    m_folder->setText(dir);
    const std::vector<std::string> files = FileGrabber::imageFiles(ss(dir));
    m_fileCount->setText(Tf("已扫描 {0} 张", QString::number(static_cast<int>(files.size()))));
}

// 草稿：相机文件工具强制指向单张图
bool RecheckWorkspace::applyImageToDraft(JobCfg* draft, const std::string& imagePath, QString* err) const
{
    if (!draft) {
        if (err) {
            *err = T("没有流程。");
        }
        return false;
    }
    int hit = 0;
    for (size_t i = 0; i < draft->tools.size(); ++i) {
        ToolCfg& t = draft->tools[i];
        const bool isFileCam = (t.toolName == "camera.file");
        const bool isCam = isFileCam || (t.type == ToolType::Acquire);
        if (!isCam) {
            continue;
        }
        // 强制单文件输入，不改方案 formal
        t.params.insert("path", imagePath);
        t.params.insert("sourceMode", std::string("file"));
        ++hit;
    }
    if (hit == 0) {
        if (err) {
            *err = T("该流程没有可用的图像采集工具（需要 camera.file）。");
        }
        return false;
    }
    return true;
}

// 从 Job 运行态收集各工具行
void RecheckWorkspace::collectToolRows(Job* job, const JobCfg* formal, Item* item) const
{
    if (!item) {
        return;
    }
    item->tools.clear();
    if (!job || !formal) {
        return;
    }
    for (size_t i = 0; i < formal->tools.size(); ++i) {
        const ToolCfg& t = formal->tools[i];
        ToolRow row;
        row.toolId = qs(t.id);
        row.name = t.name.empty() ? qs(t.toolName) : qs(t.name);
        row.ran = job->hasToolRun(t.id);
        row.elapsedMs = job->cachedElapsedMs(t.id);
        bool tok = false;
        if (row.ran && job->cachedToolOk(t.id, &tok)) {
            row.ok = tok;
        }
        if (!row.ran) {
            row.summary = T("未执行");
        } else {
            row.summary = row.ok ? T("OK") : T("NG");
            const ParamMap vals = job->cachedValues(t.id);
            // 若有 message 类字符串参数则附上
            const std::string msg = vals.value("message").toString();
            if (!msg.empty()) {
                row.summary += QLatin1String("  ") + qs(msg);
            }
        }
        item->tools.push_back(row);
    }
}

// 判定文案
QString RecheckWorkspace::verdictText(Verdict v)
{
    if (v == VerdictOk) {
        return T("OK");
    }
    if (v == VerdictNg) {
        return T("NG");
    }
    return T("失败");
}

// 开始批跑
void RecheckWorkspace::onStart()
{
    if (m_running) {
        return;
    }
    if (!m_canRun) {
        QMessageBox::warning(this, T("权限"), T("当前用户不能调试运行。"));
        return;
    }
    StationCfg* stConfig = currentStationConfig();
    JobCfg* jobConfig = currentJobConfig();
    if (!stConfig || !jobConfig) {
        QMessageBox::information(this, T("复检"), T("请先选择工站与流程。"));
        return;
    }
    const QString folder = m_folder->text().trimmed();
    if (folder.isEmpty()) {
        QMessageBox::information(this, T("复检"), T("请选择图像文件夹。"));
        return;
    }
    m_queue = FileGrabber::imageFiles(ss(folder));
    if (m_queue.empty()) {
        QMessageBox::information(this, T("复检"), T("文件夹中没有支持的图像文件。"));
        return;
    }

    // 先让主窗绑定工站实例
    emit prepareStation(qs(stConfig->id));
    if (!m_station || !m_station->findJob(jobConfig->id)) {
        QMessageBox::warning(this, T("复检"), T("找不到该流程运行实例。"));
        return;
    }

    // 预检：草稿能否注入 path
    JobCfg probe = *jobConfig;
    QString err;
    if (!applyImageToDraft(&probe, m_queue.front(), &err)) {
        QMessageBox::warning(this, T("复检"), err);
        return;
    }

    m_sessionStationId = qs(stConfig->id);
    m_sessionJobId = qs(jobConfig->id);
    m_lastReportPath.clear();
    m_items.clear();
    m_items.reserve(static_cast<int>(m_queue.size()));
    m_queueIndex = 0;
    m_stopRequested = false;
    m_paused = false;
    m_stepBusy = false;
    m_running = true;
    m_progress->setMaximum(static_cast<int>(m_queue.size()));
    m_progress->setValue(0);
    m_fileCount->setText(Tf("已扫描 {0} 张", QString::number(static_cast<int>(m_queue.size()))));
    updateRunButtons();
    refillTable();
    updateProgressLabels();
    QTimer::singleShot(0, this, SLOT(onStepNext()));
}

// 请求停止（当前张跑完后停下）
void RecheckWorkspace::onStop()
{
    m_stopRequested = true;
    if (m_paused) {
        m_paused = false;
        updateRunButtons();
        QTimer::singleShot(0, this, SLOT(onStepNext()));
    }
}

// 退出软件：立刻结束批跑状态（池由主窗 drain）
void RecheckWorkspace::abortBatch()
{
    m_stopRequested = true;
    m_paused = false;
    m_running = false;
    m_stepBusy = false;
    updateRunButtons();
}

// 暂停 / 继续
void RecheckWorkspace::onPauseResume()
{
    if (!m_running) {
        return;
    }
    m_paused = !m_paused;
    updateRunButtons();
    if (!m_paused) {
        QTimer::singleShot(0, this, SLOT(onStepNext()));
    }
}

// 跑下一张：投递线程池，不堵 UI
void RecheckWorkspace::onStepNext()
{
    if (!m_running || m_stepBusy) {
        return;
    }
    if (m_paused && !m_stopRequested) {
        return;
    }
    // 停止或队列跑完：收尾并按需自动存报告
    if (m_stopRequested || m_queueIndex >= static_cast<int>(m_queue.size())) {
        m_running = false;
        m_paused = false;
        m_stopRequested = false;
        m_stepBusy = false;
        updateRunButtons();
        updateProgressLabels();
        autoSaveReportIfNeeded();
        return;
    }

    JobCfg* formal = currentJobConfig();
    if (!formal || !m_station) {
        m_running = false;
        m_paused = false;
        m_stepBusy = false;
        updateRunButtons();
        QMessageBox::warning(this, T("复检"), T("流程或工站实例已失效。"));
        return;
    }
    Job* job = m_station->findJob(formal->id);
    if (!job) {
        m_running = false;
        m_paused = false;
        m_stepBusy = false;
        updateRunButtons();
        QMessageBox::warning(this, T("复检"), T("找不到该流程运行实例。"));
        return;
    }
    // 流程正被别处运行时 50ms 后再试
    if (job->isRunning()) {
        QTimer::singleShot(50, this, SLOT(onStepNext()));
        return;
    }

    // 复制配方并把取图改成这张图；改不了直接记为失败
    const std::string path = m_queue[static_cast<size_t>(m_queueIndex)];
    ++m_queueIndex;
    JobCfg* draft = new JobCfg(*formal);
    QString err;
    if (!applyImageToDraft(draft, path, &err)) {
        Item* item = new Item();
        item->path = qs(path);
        item->fileName = QFileInfo(item->path).fileName();
        item->ran = true;
        item->verdict = VerdictFail;
        item->message = err;
        item->elapsedMs = 0;
        onStepFinished(item);
        if (draft) {
            delete draft;
            draft = nullptr;
        }
        return;
    }

    // 线程池里用配方副本跑一遍，结果按 OK/NG 记录（运行出错也记 NG），排队回 onStepFinished
    m_stepBusy = true;
    Solution* sol = m_sol;
    ThreadPool::shared().enqueue([this, job, sol, draft, path]() {
        Item* item = new Item();
        item->path = qs(path);
        item->fileName = QFileInfo(item->path).fileName();
        StageTimer watch;
        watch.begin("recheck");
        const ToolOut out = job->run(sol, draft);
        watch.end("recheck");
        const double elapsed = watch.ms("recheck");
        item->ran = true;
        item->verdict = out.ok ? VerdictOk : VerdictNg;
        item->message = qs(out.message);
        item->elapsedMs = elapsed < 0 ? 0 : static_cast<int>(elapsed);
        item->source = out.source;
        item->image = out.image;
        item->graphics = qg(out.objects);
        collectToolRows(job, draft, item);
        JobCfg* held = draft;
        if (held) {
            delete held;
            held = nullptr;
        }
        QMetaObject::invokeMethod(this, "onStepFinished", Qt::QueuedConnection,
                                  Q_ARG(void*, item));
    });
}

// 单张结果回 UI
void RecheckWorkspace::onStepFinished(void* itemPtr)
{
    Item* item = static_cast<Item*>(itemPtr);
    m_stepBusy = false;
    if (!item) {
        return;
    }
    if (!m_running && m_stopRequested) {
        if (item) {
            delete item;
            item = nullptr;
        }
        m_stopRequested = false;
        m_paused = false;
        updateRunButtons();
        updateProgressLabels();
        return;
    }
    m_items.push_back(*item);
    if (item) {
        delete item;
        item = nullptr;
    }
    m_progress->setValue(m_queueIndex);
    refillTable();
    updateProgressLabels();
    const Item& last = m_items.back();
    const int filter = m_filter->currentData().toInt();
    const bool wantSelect = (filter == 0)
                            || (filter == 1 && last.verdict != VerdictOk)
                            || (filter == 2 && last.verdict == VerdictNg)
                            || (filter == 3 && last.verdict == VerdictFail);
    if (wantSelect) {
        for (int r = m_table->rowCount() - 1; r >= 0; --r) {
            if (itemIndexFromRow(r) == m_items.size() - 1) {
                m_table->selectRow(r);
                break;
            }
        }
    }
    if (!m_paused) {
        QTimer::singleShot(0, this, SLOT(onStepNext()));
    }
}

// 刷新进度与 OK/NG/失败
void RecheckWorkspace::updateProgressLabels()
{
    int ok = 0;
    int ng = 0;
    int fail = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items[i].ran) {
            continue;
        }
        if (m_items[i].verdict == VerdictOk) {
            ++ok;
        } else if (m_items[i].verdict == VerdictNg) {
            ++ng;
        } else {
            ++fail;
        }
    }
    m_stats->setText(T("OK ") + QString::number(ok) + T("  NG ") + QString::number(ng)
                     + T("  失败 ") + QString::number(fail));
    if (m_running) {
        m_progress->setFormat(QString::number(m_queueIndex) + QLatin1Char('/')
                              + QString::number(m_progress->maximum()));
    }
}

// 更新开始/暂停/停止按钮
void RecheckWorkspace::updateRunButtons()
{
    m_btnStart->setEnabled(m_canRun && !m_running);
    m_btnPause->setEnabled(m_running);
    m_btnPause->setText(m_paused ? T("继续") : T("暂停"));
    m_btnStop->setEnabled(m_running);
    m_btnExport->setEnabled(m_canRun && !m_items.isEmpty());
    m_btnReport->setEnabled(m_canRun && !m_items.isEmpty());
    m_btnDebug->setEnabled(m_canRun && currentItem() != 0);
}

// 重填表格
void RecheckWorkspace::refillTable()
{
    const int filter = m_filter->currentData().toInt();
    const QString selPath = (m_table->currentRow() >= 0)
                            ? m_table->item(m_table->currentRow(), 1)
                                  ? m_table->item(m_table->currentRow(), 1)->data(Qt::UserRole).toString()
                                  : QString()
                            : QString();
    m_table->setRowCount(0);
    int showIndex = 0;
    int restoreRow = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        const Item& it = m_items[i];
        if (filter == 1 && it.ran && it.verdict == VerdictOk) {
            continue;
        }
        if (filter == 2 && !(it.ran && it.verdict == VerdictNg)) {
            continue;
        }
        if (filter == 3 && !(it.ran && it.verdict == VerdictFail)) {
            continue;
        }
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        QTableWidgetItem* c0 = new QTableWidgetItem(QString::number(++showIndex));
        c0->setData(Qt::UserRole, i);
        m_table->setItem(r, 0, c0);
        QTableWidgetItem* c1 = new QTableWidgetItem(it.fileName);
        c1->setData(Qt::UserRole, it.path);
        m_table->setItem(r, 1, c1);
        QTableWidgetItem* c2 = new QTableWidgetItem(it.ran ? verdictText(it.verdict) : T("-"));
        if (it.ran) {
            if (it.verdict == VerdictOk) {
                c2->setForeground(QColor(QLatin1String("#52C41A")));
            } else if (it.verdict == VerdictNg) {
                c2->setForeground(QColor(QLatin1String("#F5222D")));
            } else {
                c2->setForeground(QColor(QLatin1String("#FA8C16")));
            }
        }
        m_table->setItem(r, 2, c2);
        m_table->setItem(r, 3, new QTableWidgetItem(it.ran ? QString::number(it.elapsedMs) : QLatin1String("-")));
        m_table->setItem(r, 4, new QTableWidgetItem(it.message));
        if (!selPath.isEmpty() && it.path == selPath) {
            restoreRow = r;
        }
    }
    if (restoreRow >= 0) {
        m_table->selectRow(restoreRow);
    }
    updateRunButtons();
}

// 表格行 → items 下标
int RecheckWorkspace::itemIndexFromRow(int row) const
{
    if (row < 0 || row >= m_table->rowCount()) {
        return -1;
    }
    QTableWidgetItem* it = m_table->item(row, 0);
    return it ? it->data(Qt::UserRole).toInt() : -1;
}

// 当前选中 Item
const RecheckWorkspace::Item* RecheckWorkspace::currentItem() const
{
    const int idx = itemIndexFromRow(m_table->currentRow());
    if (idx < 0 || idx >= m_items.size()) {
        return 0;
    }
    return &m_items[idx];
}

// 选中行 → 详情
void RecheckWorkspace::onRowSelected()
{
    const Item* item = currentItem();
    showDetail(item);
    updateRunButtons();
}

// 过滤条件变化
void RecheckWorkspace::onFilterChanged()
{
    refillTable();
}

// 显示详情
void RecheckWorkspace::showDetail(const Item* item)
{
    if (!item || !item->ran) {
        m_detailTitle->setText(T("详情（选中结果行）"));
        m_view->clearImage();
        m_view->setOverlay(std::vector<HsCommData::Graphic>(), 0);
        m_toolTable->setRowCount(0);
        return;
    }
    // 结果图为空时退回源图
    m_detailTitle->setText(item->fileName + QLatin1String("  ")
                           + verdictText(item->verdict) + QLatin1String("  ")
                           + QString::number(item->elapsedMs) + QLatin1String("ms"));
    HsCommData::HsImage show = item->image;
    if (show.isNull()) {
        show = item->source;
    }
    applyFrame(m_view, show);
    m_view->fitToWindow();
    m_view->setOverlay(toStd(item->graphics), m_view->currentFrameId());

    // 工具明细：没有逐工具结果时只给一行汇总
    m_toolTable->setRowCount(0);
    if (item->tools.isEmpty()) {
        const int r = m_toolTable->rowCount();
        m_toolTable->insertRow(r);
        m_toolTable->setItem(r, 0, new QTableWidgetItem(T("汇总")));
        m_toolTable->setItem(r, 1, new QTableWidgetItem(verdictText(item->verdict)));
        m_toolTable->setItem(r, 2, new QTableWidgetItem(QString::number(item->elapsedMs)));
        m_toolTable->setItem(r, 3, new QTableWidgetItem(item->message));
        return;
    }
    for (int i = 0; i < item->tools.size(); ++i) {
        const ToolRow& tr = item->tools[i];
        const int r = m_toolTable->rowCount();
        m_toolTable->insertRow(r);
        QTableWidgetItem* name = new QTableWidgetItem(tr.name);
        name->setData(Qt::UserRole, tr.toolId);
        m_toolTable->setItem(r, 0, name);
        QString v = T("未执行");
        if (tr.ran) {
            v = tr.ok ? T("OK") : T("NG");
        }
        QTableWidgetItem* vc = new QTableWidgetItem(v);
        if (tr.ran) {
            vc->setForeground(tr.ok ? QColor(QLatin1String("#52C41A"))
                                    : QColor(QLatin1String("#F5222D")));
        }
        m_toolTable->setItem(r, 1, vc);
        QString msText = QLatin1String("-");
        if (tr.ran && tr.elapsedMs >= 0) {
            msText = QString::number(tr.elapsedMs);
        }
        m_toolTable->setItem(r, 2, new QTableWidgetItem(msText));
        m_toolTable->setItem(r, 3, new QTableWidgetItem(tr.summary));
    }
}

// 导出 CSV
void RecheckWorkspace::onExportCsv()
{
    if (m_items.isEmpty()) {
        QMessageBox::information(this, T("导出"), T("没有可导出的结果。"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, T("导出 CSV"),
                                                      QLatin1String("recheck.csv"),
                                                      T("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, T("导出"), T("无法写入文件。"));
        return;
    }
    QTextStream out(&f);
    out << QByteArray("\xEF\xBB\xBF");
    out << T("序号") << ',' << T("文件名") << ',' << T("路径") << ','
        << T("判定") << ',' << T("耗时ms") << ',' << T("摘要") << '\n';
    for (int i = 0; i < m_items.size(); ++i) {
        const Item& it = m_items[i];
        QString msg = it.message;
        msg.replace(QLatin1Char('"'), QLatin1String("\"\""));
        out << (i + 1) << ','
            << '"' << it.fileName << '"' << ','
            << '"' << it.path << '"' << ','
            << (it.ran ? verdictText(it.verdict) : QLatin1String("-")) << ','
            << it.elapsedMs << ','
            << '"' << msg << '"' << '\n';
    }
}

// 写会话报告 JSON
bool RecheckWorkspace::writeReportFile(const QString& path, QString* err) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) {
            *err = T("无法写入文件。");
        }
        return false;
    }
    int ok = 0;
    int ng = 0;
    int fail = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items[i].ran) {
            continue;
        }
        if (m_items[i].verdict == VerdictOk) {
            ++ok;
        } else if (m_items[i].verdict == VerdictNg) {
            ++ng;
        } else {
            ++fail;
        }
    }
    // 手写 JSON：会话信息、统计，再逐图列出结果和各工具明细
    QTextStream out(&f);
    out.setCodec("UTF-8");
    out << QLatin1String("{\n");
    out << QLatin1String("  \"time\": \"") << jsonEsc(QDateTime::currentDateTime().toString(Qt::ISODate))
        << QLatin1String("\",\n");
    out << QLatin1String("  \"stationId\": \"") << jsonEsc(m_sessionStationId) << QLatin1String("\",\n");
    out << QLatin1String("  \"jobId\": \"") << jsonEsc(m_sessionJobId) << QLatin1String("\",\n");
    out << QLatin1String("  \"folder\": \"") << jsonEsc(m_folder->text()) << QLatin1String("\",\n");
    out << QLatin1String("  \"stats\": { \"ok\": ") << ok << QLatin1String(", \"ng\": ") << ng
        << QLatin1String(", \"fail\": ") << fail << QLatin1String(" },\n");
    out << QLatin1String("  \"items\": [\n");
    for (int i = 0; i < m_items.size(); ++i) {
        const Item& it = m_items[i];
        out << QLatin1String("    {\n");
        out << QLatin1String("      \"file\": \"") << jsonEsc(it.fileName) << QLatin1String("\",\n");
        out << QLatin1String("      \"path\": \"") << jsonEsc(it.path) << QLatin1String("\",\n");
        out << QLatin1String("      \"verdict\": \"") << jsonEsc(verdictText(it.verdict)) << QLatin1String("\",\n");
        out << QLatin1String("      \"elapsedMs\": ") << it.elapsedMs << QLatin1String(",\n");
        out << QLatin1String("      \"message\": \"") << jsonEsc(it.message) << QLatin1String("\",\n");
        out << QLatin1String("      \"tools\": [\n");
        for (int t = 0; t < it.tools.size(); ++t) {
            const ToolRow& tr = it.tools[t];
            out << QLatin1String("        { \"id\": \"") << jsonEsc(tr.toolId)
                << QLatin1String("\", \"name\": \"") << jsonEsc(tr.name)
                << QLatin1String("\", \"ran\": ") << (tr.ran ? QLatin1String("true") : QLatin1String("false"))
                << QLatin1String(", \"ok\": ") << (tr.ok ? QLatin1String("true") : QLatin1String("false"))
                << QLatin1String(", \"elapsedMs\": ") << tr.elapsedMs
                << QLatin1String(", \"summary\": \"") << jsonEsc(tr.summary) << QLatin1String("\" }");
            if (t + 1 < it.tools.size()) {
                out << QLatin1Char(',');
            }
            out << QLatin1Char('\n');
        }
        out << QLatin1String("      ]\n");
        out << QLatin1String("    }");
        if (i + 1 < m_items.size()) {
            out << QLatin1Char(',');
        }
        out << QLatin1Char('\n');
    }
    out << QLatin1String("  ]\n");
    out << QLatin1String("}\n");
    return true;
}

// 批跑结束自动落报告
void RecheckWorkspace::autoSaveReportIfNeeded()
{
    if (m_items.isEmpty()) {
        return;
    }
    AppPaths::ensureDirs();
    const QString dir = qs(AppPaths::logsDir());
    QDir().mkpath(dir);
    const QString name = QLatin1String("recheck-")
                         + QDateTime::currentDateTime().toString(QLatin1String("yyyyMMdd-HHmmss"))
                         + QLatin1String(".json");
    const QString path = dir + QLatin1Char('/') + name;
    QString err;
    if (writeReportFile(path, &err)) {
        m_lastReportPath = path;
    }
}

// 手动保存报告
void RecheckWorkspace::onSaveReport()
{
    if (m_items.isEmpty()) {
        QMessageBox::information(this, T("报告"), T("没有可保存的结果。"));
        return;
    }
    AppPaths::ensureDirs();
    QString suggest = m_lastReportPath;
    if (suggest.isEmpty()) {
        suggest = qs(AppPaths::logsDir()) + QLatin1String("/recheck-")
                  + QDateTime::currentDateTime().toString(QLatin1String("yyyyMMdd-HHmmss"))
                  + QLatin1String(".json");
    }
    const QString path = QFileDialog::getSaveFileName(this, T("保存会话报告"), suggest,
                                                      T("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!writeReportFile(path, &err)) {
        QMessageBox::warning(this, T("报告"), err.isEmpty() ? T("无法写入文件。") : err);
        return;
    }
    m_lastReportPath = path;
    QMessageBox::information(this, T("报告"), T("已保存：") + path);
}

// 用当前行跳转设计调试
void RecheckWorkspace::onOpenDesignDebug()
{
    const Item* item = currentItem();
    if (!item) {
        QMessageBox::information(this, T("设计调试"), T("请先选中一条复检结果。"));
        return;
    }
    QString jobId = m_sessionJobId;
    if (jobId.isEmpty()) {
        JobCfg* j = currentJobConfig();
        jobId = j ? qs(j->id) : QString();
    }
    QString toolId;
    const int tr = m_toolTable->currentRow();
    if (tr >= 0 && m_toolTable->item(tr, 0)) {
        toolId = m_toolTable->item(tr, 0)->data(Qt::UserRole).toString();
    }
    if (toolId.isEmpty()) {
        for (int i = 0; i < item->tools.size(); ++i) {
            if (item->tools[i].ran && !item->tools[i].ok) {
                toolId = item->tools[i].toolId;
                break;
            }
        }
    }
    if (toolId.isEmpty() && !item->tools.isEmpty()) {
        toolId = item->tools[0].toolId;
    }
    emit openDesignDebug(jobId, toolId, item->path);
}

// 双击工具结果行 → 该工具调试
void RecheckWorkspace::onToolDouble(int row, int)
{
    const Item* item = currentItem();
    if (!item || row < 0 || row >= m_toolTable->rowCount()) {
        return;
    }
    QTableWidgetItem* name = m_toolTable->item(row, 0);
    if (!name) {
        return;
    }
    QString jobId = m_sessionJobId;
    if (jobId.isEmpty()) {
        JobCfg* j = currentJobConfig();
        jobId = j ? qs(j->id) : QString();
    }
    emit openDesignDebug(jobId, name->data(Qt::UserRole).toString(), item->path);
}

// 语言热切换：刷复检页静态文案与表头
void RecheckWorkspace::retranslateUi()
{
    retranslateBound(this);
    if (!m_folder->property("vpTr").toByteArray().isEmpty()) {
        m_folder->setPlaceholderText(T(m_folder->property("vpTr").toByteArray().constData()));
    }
    const int fi = m_filter->currentIndex();
    m_filter->setItemText(0, T("全部"));
    m_filter->setItemText(1, T("问题"));
    m_filter->setItemText(2, T("仅 NG"));
    m_filter->setItemText(3, T("仅失败"));
    if (fi >= 0) {
        m_filter->setCurrentIndex(fi);
    }
    QStringList headers;
    headers << T("#") << T("文件名") << T("判定")
            << T("耗时ms") << T("摘要");
    m_table->setHorizontalHeaderLabels(headers);
    QStringList th;
    th << T("工具") << T("判定")
       << T("耗时ms") << T("说明");
    m_toolTable->setHorizontalHeaderLabels(th);
    updateProgressLabels();
    m_btnPause->setText(m_paused ? T("继续") : T("暂停"));
}
}


#include "moc_RecheckWorkspace.cpp"
