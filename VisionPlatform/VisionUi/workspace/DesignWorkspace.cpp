/**
 * @file DesignWorkspace.cpp
 * @brief DesignWorkspace实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DesignWorkspace.h"
#include "JobFlowCanvasPanel.h"
#include "JobCanvas.h"
#include "ToolBase.h"
#include "ToolTreeIcons.h"
#include "QtConv.h"
#include "Solution.h"
#include "JobCfg.h"
#include "ToolCfg.h"
#include "RoiConv.h"
#include "Session.h"
#include "Perm.h"
#include "Theme.h"
#include "UiText.h"
#include "ToolDebugHost.h"
#include "ToolRstPanel.h"
#include "ToolLibrary.h"
#include "ToolIoPanel.h"
#include "CloudViewBinder.h"
#include "Station.h"
#include "Job.h"
#include <image_widget/image_draw_pad.h>
#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QCursor>
#include <QDateTime>
#include <QFrame>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QToolTip>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QBrush>
#include <QColor>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QVBoxLayout>

namespace Vision {

enum TreeRole {
    RoleId = Qt::UserRole,
    RoleKind = Qt::UserRole + 1,
    RoleRunState = Qt::UserRole + 2,
    RoleDisabled = Qt::UserRole + 3 ///< disabled gray
};

enum TreeRunState {
    RunStateNone = 0,
    RunStateOk = 1,
    RunStateNg = 2
};

enum TreeKind {
    KindSol = 0,
    KindStation,
    KindJob,
    KindTool
};

// 资源树委托：按运行状态绘制节点
class ResourceTreeDelegate : public QStyledItemDelegate {
public:
    explicit ResourceTreeDelegate(QObject* parent = 0) : QStyledItemDelegate(parent) {}

    // 自绘节点，按运行状态着色
void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const int kind = index.data(RoleKind).toInt();
        const int runState = index.data(RoleRunState).toInt();
        if (kind == KindTool && runState != RunStateNone) {
            QColor bg = (runState == RunStateOk)
                            ? QColor(0xc8, 0xe6, 0xc9)
                            : QColor(0xff, 0xcd, 0xd2);
            QColor fg = (runState == RunStateOk)
                            ? QColor(0x1b, 0x5e, 0x20)
                            : QColor(0xb7, 0x1c, 0x1c);
            const bool selected = (opt.state & QStyle::State_Selected) != 0;
            if (selected) {
                bg = bg.darker(112);
            }
            painter->save();
            painter->fillRect(opt.rect, bg);
            if (selected) {
                painter->setPen(QPen(QColor(0x18, 0x90, 0xff), 2));
                painter->drawRect(opt.rect.adjusted(1, 1, -2, -2));
            }
            painter->restore();

            opt.palette.setColor(QPalette::Text, fg);
            opt.palette.setColor(QPalette::HighlightedText, fg);
            opt.backgroundBrush = Qt::NoBrush;
            opt.state &= ~QStyle::State_Selected;
        }

        QStyledItemDelegate::paint(painter, opt, index);
    }
};

// 清掉工具节点的运行着色
static void clearTreeToolRunLook(QTreeWidgetItem* item)
{
    if (!item) {
        return;
    }
    item->setData(0, RoleRunState, RunStateNone);
    item->setBackground(0, QBrush());
    if (item->data(0, RoleDisabled).toBool()) {
        item->setForeground(0, QBrush(QColor(0xA0, 0xA0, 0xA0)));
    } else {
        item->setForeground(0, QBrush());
    }
    QFont f = item->font(0);
    f.setBold(false);
    item->setFont(0, f);
}

// 按 OK/NG 给工具节点着色
static void applyTreeToolRunLook(QTreeWidgetItem* item, bool ok)
{
    if (!item) {
        return;
    }
    item->setData(0, RoleRunState, ok ? RunStateOk : RunStateNg);
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
}

// 从工具节点向上找所属流程 id
static QString treeToolJobId(QTreeWidgetItem* toolItem)
{
    QTreeWidgetItem* p = toolItem ? toolItem->parent() : nullptr;
    while (p) {
        if (p->data(0, RoleKind).toInt() == KindJob) {
            return p->data(0, RoleId).toString();
        }
        p = p->parent();
    }
    return QString();
}

// 取工具所在流程的运行对象
inline Job* currentJobForTool(Station* station, const ObjectId& toolId)
{
    return station ? station->findJobOfTool(toolId) : 0;
}

// 设计页：方案树、画布、图像和结果
DesignWorkspace::DesignWorkspace(Session* session, Solution* solution, Station* station, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_sol(solution)
    , m_station(station)
    , m_canUp(false)
    , m_canDown(false)
    , m_canEditTool(false)
    , m_toolLib(nullptr)
    , m_solTitle(nullptr)
    , m_algoTitle(nullptr)
    , m_canvasTitle(nullptr)
    , m_debug(nullptr)
    , m_result(nullptr)
    , m_drawPad(nullptr)
    , m_chkDrawRoi(nullptr)
    , m_hasLastRun(false)
{
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setMinimumWidth(180);
    m_tree->setIconSize(QSize(18, 18));
    m_tree->setItemDelegate(new ResourceTreeDelegate(m_tree));
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onTreeDouble(QTreeWidgetItem*,int)));
    connect(m_tree, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(onTreeClicked(QTreeWidgetItem*,int)));
    connect(m_tree, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onTreeMenu(QPoint)));

    m_toolLib = new QTreeWidget(this);
    m_toolLib->setHeaderHidden(true);
    m_toolLib->setIconSize(QSize(20, 20));
    m_toolLib->setMinimumHeight(120);
    fillRegisteredToolTree(m_toolLib, true);
    connect(m_toolLib, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(onAlgLibDouble(QTreeWidgetItem*,int)));

    m_shell = new HsCtrl::HsViewShell(this);
    m_view = m_shell->imageWindow();
    m_view->setWindowMode(HsCtrl::WindowDesign);
    // 隐藏测量叠加工具条；ROI 绘制走 ImageDrawPad
    m_view->setOverlayToolbarVisible(false);
    m_view->setDrawTool(HsCtrl::ToolBrowse);
    connect(m_view, SIGNAL(cursorMoved(QPointF,double,bool)), this, SLOT(onCursor(QPointF,double,bool)));
    connect(m_view, SIGNAL(roiCommitted(HsCommData::Graphic)), this, SLOT(onRoi(HsCommData::Graphic)));

    m_drawPad = new image_widget::ImageDrawPad(this);
    m_drawPad->setImageWindow(m_view);
    m_drawPad->hide();

    m_chkDrawRoi = new QCheckBox(T("绘制 ROI"), this);
    m_chkDrawRoi->setToolTip(T("开启后可在设计区图像上直接绘制当前工具的 ROI"));
    m_chkDrawRoi->setVisible(false);
    connect(m_chkDrawRoi, SIGNAL(toggled(bool)), this, SLOT(onDesignDrawModeChanged()));

    m_status = new QLabel(T("图像 / ROI"), this);
    bindTrKey(m_status, "图像 / ROI");
    m_status->setObjectName(QLatin1String("hint"));

    QAction* actUp = new QAction(T("上移"), this);
    bindTrKey(actUp, "上移");
    actUp->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Up));
    connect(actUp, SIGNAL(triggered()), this, SLOT(onMoveUp()));
    QAction* actDown = new QAction(T("下移"), this);
    bindTrKey(actDown, "下移");
    actDown->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Down));
    connect(actDown, SIGNAL(triggered()), this, SLOT(onMoveDown()));
    QAction* actRename = new QAction(T("重命名"), this);
    bindTrKey(actRename, "重命名");
    actRename->setShortcut(QKeySequence(Qt::Key_F2));
    connect(actRename, SIGNAL(triggered()), this, SLOT(onRename()));
    QAction* actCopy = new QAction(T("复制"), this);
    bindTrKey(actCopy, "复制");
    actCopy->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
    connect(actCopy, SIGNAL(triggered()), this, SLOT(onCopyTool()));
    QAction* actDel = new QAction(T("删除"), this);
    bindTrKey(actDel, "删除");
    actDel->setShortcut(QKeySequence::Delete);
    connect(actDel, SIGNAL(triggered()), this, SLOT(onDeleteTool()));
    addAction(actUp);
    addAction(actDown);
    addAction(actRename);
    addAction(actCopy);
    addAction(actDel);

    // 方案树卡片
    QFrame* solCard = makeCard(this);
    QVBoxLayout* solLay = new QVBoxLayout(solCard);
    solLay->setContentsMargins(8, 8, 8, 8);
    solLay->setSpacing(4);
    m_solTitle = new QLabel(T("方案"), solCard);
    bindTrKey(m_solTitle, "方案");
    m_solTitle->setObjectName(QLatin1String("section"));
    solLay->addWidget(m_solTitle);
    solLay->addWidget(m_tree, 1);

    QFrame* algoCard = makeCard(this);
    QVBoxLayout* algoLay = new QVBoxLayout(algoCard);
    algoLay->setContentsMargins(8, 8, 8, 8);
    algoLay->setSpacing(4);
    m_algoTitle = new QLabel(T("工具库"), algoCard);
    bindTrKey(m_algoTitle, "工具库");
    m_algoTitle->setObjectName(QLatin1String("section"));
    algoLay->addWidget(m_algoTitle);
    algoLay->addWidget(m_toolLib, 1);

    QSplitter* leftSplit = new QSplitter(Qt::Vertical, this);
    leftSplit->addWidget(solCard);
    leftSplit->addWidget(algoCard);
    leftSplit->setStretchFactor(0, 1);
    leftSplit->setStretchFactor(1, 2);
    leftSplit->setSizes(QList<int>() << 180 << 320);
    leftSplit->setMinimumWidth(220);

    m_flowCanvas = new JobFlowCanvasPanel(m_session, m_sol, this);
    connect(m_flowCanvas, SIGNAL(toolClicked(QString)), this, SLOT(onCanvasTool(QString)));
    connect(m_flowCanvas, SIGNAL(toolDoubleClicked(QString)), this, SLOT(onCanvasToolDouble(QString)));
    connect(m_flowCanvas, SIGNAL(groupClicked(QString)), this, SLOT(onCanvasGroup(QString)));
    connect(m_flowCanvas, SIGNAL(canvasMenuRequested(QString,int,QPoint)),
            this, SLOT(onCanvasMenu(QString,int,QPoint)));
    connect(m_flowCanvas, SIGNAL(linksChanged()), this, SLOT(onCanvasLinksChanged()));

    QFrame* canvasCard = makeCard(this);
    QVBoxLayout* canvasLay = new QVBoxLayout(canvasCard);
    canvasLay->setContentsMargins(8, 8, 8, 8);
    m_canvasTitle = new QLabel(T("流程画布"), canvasCard);
    bindTrKey(m_canvasTitle, "流程画布");
    m_canvasTitle->setObjectName(QLatin1String("section"));
    canvasLay->addWidget(m_canvasTitle);
    canvasLay->addWidget(m_flowCanvas, 1);

    QFrame* imgCard = makeCard(this);
    imgCard->setMinimumWidth(260);
    QVBoxLayout* imgLay = new QVBoxLayout(imgCard);
    imgLay->setContentsMargins(8, 8, 8, 8);
    imgLay->setSpacing(6);
    QLabel* iTitle = new QLabel(T("图像"), imgCard);
    bindTrKey(iTitle, "图像");
    iTitle->setObjectName(QLatin1String("section"));
    QHBoxLayout* imgTitleLay = new QHBoxLayout();
    imgTitleLay->setContentsMargins(0, 0, 0, 0);
    imgTitleLay->addWidget(iTitle, 1);
    imgTitleLay->addWidget(m_chkDrawRoi);
    imgLay->addLayout(imgTitleLay);
    imgLay->addWidget(m_shell, 1);
    imgLay->addWidget(m_status);

    m_result = new ToolRstPanel(this);

    QPushButton* runT = new QPushButton(T("运行本工具"), this);
    bindTrKey(runT, "运行本工具");
    QPushButton* runJ = new QPushButton(T("运行本流程"), this);
    m_btnDebug = new QPushButton(T("详细调试"), this);
    bindTrKey(m_btnDebug, "详细调试");
    m_btnRunTool = runT;
    m_btnRunJob = runJ;
    setButtonRole(runJ, "primary");
    connect(runT, SIGNAL(clicked()), this, SIGNAL(runThisTool()));
    connect(runJ, SIGNAL(clicked()), this, SIGNAL(runThisJob()));
    connect(m_btnDebug, SIGNAL(clicked()), this, SLOT(onOpenDebug()));

    QFrame* resultCard = makeCard(this);
    resultCard->setMinimumWidth(260);
    QVBoxLayout* rLay = new QVBoxLayout(resultCard);
    rLay->setContentsMargins(8, 8, 8, 8);
    rLay->setSpacing(6);
    QLabel* pTitle = new QLabel(T("调试结果"), resultCard);
    bindTrKey(pTitle, "调试结果");
    pTitle->setObjectName(QLatin1String("section"));
    rLay->addWidget(pTitle);
    rLay->addWidget(m_result, 1);
    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);
    btnRow->addWidget(m_btnDebug);
    btnRow->addWidget(runT);
    rLay->addLayout(btnRow);
    rLay->addWidget(runJ);

    QSplitter* split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(leftSplit);
    split->addWidget(canvasCard);
    split->addWidget(imgCard);
    split->addWidget(resultCard);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 1);
    split->setStretchFactor(3, 0);
    split->setSizes(QList<int>() << 240 << 420 << 360 << 300);
    m_log = new QTableWidget(this);
    m_log->setColumnCount(3);
    QStringList hh;
    hh << T("时间") << T("级别") << T("消息");
    m_log->setHorizontalHeaderLabels(hh);
    m_log->horizontalHeader()->setStretchLastSection(true);
    m_log->verticalHeader()->setVisible(false);
    m_log->setMaximumHeight(140);
    m_log->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_log->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_log->setAlternatingRowColors(true);
    m_log->setColumnWidth(0, 72);
    m_log->setColumnWidth(1, 48);

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->addWidget(split, 1);
    QLabel* logTitle = new QLabel(T("日志"), this);
    bindTrKey(logTitle, "日志");
    logTitle->setObjectName(QLatin1String("section"));
    lay->addWidget(logTitle);
    lay->addWidget(m_log);
    reloadTree();
    syncDrawingInteractionView();
}


// 当前工站 id：先看树选中，再看当前流程
QString 
DesignWorkspace::currentStationId() const
{
    if (m_tree) {
        QTreeWidgetItem* cur = m_tree->currentItem();
        while (cur) {
            const int kind = cur->data(0, RoleKind).toInt();
            if (kind == KindStation) {
                return cur->data(0, RoleId).toString();
            }
            cur = cur->parent();
        }
    }
    if (m_sol && !m_jobId.isEmpty()) {
        if (const StationCfg* st = m_sol->findStationCfgOfJob(oid(m_jobId))) {
            return qs(st->id);
        }
    }
    if (m_sol && !m_sol->stations.empty()) {
        return qs(m_sol->stations[0].id);
    }
    return QString();
}
// 切换当前工站
void DesignWorkspace::setStation(Station* station)
{
    m_station = station;
}

// 图像光标位置和灰度
void DesignWorkspace::onCursor(const QPointF& pt, double gray, bool valid)
{
    if (!valid) {
        return;
    }
    m_status->setText(T("行 %1  列 %2  灰度 %3  缩放 %4%")
                      .arg(qRound(pt.y()))
                      .arg(qRound(pt.x()))
                      .arg(gray, 0, 'f', 1)
                      .arg(m_view->zoomScale() * 100.0, 0, 'f', 0));
}

// 追加日志
void DesignWorkspace::appendLog(const QString& line)
{
    QString level = QLatin1String("INF");
    if (line.contains(QString::fromUtf8("[OK]")) || line.contains(QLatin1String(" OK"))) {
        level = QLatin1String("OK");
    } else if (line.contains(QString::fromUtf8("[NG]")) || line.contains(QLatin1String(" NG"))) {
        level = QLatin1String("NG");
    }
    appendLog(level, line);
}

// 追加日志
void DesignWorkspace::appendLog(const QString& level, const QString& message)
{
    m_log->insertRow(0);
    m_log->setItem(0, 0, new QTableWidgetItem(QDateTime::currentDateTime().toString(QLatin1String("hh:mm:ss"))));
    m_log->setItem(0, 1, new QTableWidgetItem(level));
    m_log->setItem(0, 2, new QTableWidgetItem(message));
    while (m_log->rowCount() > 200) {
        m_log->removeRow(m_log->rowCount() - 1);
    }
}

// 按当前流程重画画布
void DesignWorkspace::refreshCanvas()
{
    JobCfg* job = m_sol->findJobConfig(oid(m_jobId));
    if (!job) {
        job = m_sol->findJobCfgOfTool(oid(m_toolId), 0);
    }
    if (job) {
        job->ensureTree();
    }
    if (m_flowCanvas) {
        m_flowCanvas->setFallbackToolId(m_toolId);
        m_flowCanvas->bindJob(job);
        m_flowCanvas->showJob(m_toolId);
    }
}

// 打开当前工具的调试窗，并接上 ROI、草稿、试跑和连线变化
void DesignWorkspace::onOpenDebug()
{
    ToolCfg* t = m_sol->findTool(oid(m_toolId));
    if (!t) {
        return;
    }
    if (m_debug) {
        m_debug->close();
        m_debug->deleteLater();
        m_debug = 0;
    }
    m_debug = new ToolDebugHost(t, m_session, m_sol, currentJobForTool(m_station, t->id), this);
    connect(m_debug, SIGNAL(roiChanged()), this, SLOT(onDebugRoiChanged()));
    connect(m_debug, SIGNAL(draftSaved()), this, SLOT(onDebugDraftSaved()));
    connect(m_debug, SIGNAL(testRunFinished(ToolOut)), this, SLOT(onDebugTestRun(ToolOut)));
    connect(m_debug, SIGNAL(linksChanged()), this, SLOT(onCanvasLinksChanged()));
    m_debug->show();
    m_debug->raise();
    m_debug->refreshView();
    syncToolView(true);
}

// 从复检定位到指定流程和工具，必要时打开对应图像
void DesignWorkspace::focusRecheckDebug(const QString& jobId, const QString& toolId,
                                        const QString& imagePath)
{
    m_jobId = jobId;
    QString tid = toolId;
    if (tid.isEmpty()) {
        JobCfg* j = m_sol ? m_sol->findJobConfig(oid(jobId)) : nullptr;
        if (j && !j->tools.empty()) {
            tid = qs(j->tools[0].id);
        }
    }
    m_toolId = tid;
    reloadTree();
    if (!tid.isEmpty()) {
        selectTool(tid);
    }
    onOpenDebug();
    if (m_debug && !imagePath.isEmpty()) {
        m_debug->applyForcedImageFile(ss(imagePath));
    }
}

// 按当前已登记工具重填工具库
void DesignWorkspace::reloadToolLib()
{
    if (m_toolLib) {
        fillRegisteredToolTree(m_toolLib, true);
    }
}

void DesignWorkspace::reloadTree()
{
    int keepKind = KindSol;
    QString keepId;
    QTreeWidgetItem* cur = m_tree->currentItem();
    if (cur) {
        keepKind = cur->data(0, RoleKind).toInt();
        keepId = cur->data(0, RoleId).toString();
    } else if (!m_jobId.isEmpty()) {
        keepKind = KindJob;
        keepId = m_jobId;
    }
    m_tree->clear();
    QTreeWidgetItem* root = new QTreeWidgetItem(m_tree);
    root->setText(0, T("方案 ") + qs(m_sol->name));
    root->setData(0, RoleKind, KindSol);

    QTreeWidgetItem* keepItem = (keepKind == KindSol) ? root : nullptr;
    // 遍历工站填树
    for (int i = 0; i < m_sol->stations.size(); ++i) {
        const StationCfg& st = m_sol->stations[i];
        QTreeWidgetItem* stItem = new QTreeWidgetItem(root);
        stItem->setText(0, T("工站 ") + qs(st.stationCode) + QLatin1String(" ") + qs(st.name));
        stItem->setData(0, RoleId, qs(st.id));
        stItem->setData(0, RoleKind, KindStation);
        if (!st.enabled) {
            stItem->setText(0, stItem->text(0) + T("（未启用）"));
            stItem->setData(0, RoleDisabled, true);
            stItem->setForeground(0, QBrush(QColor(0xA0, 0xA0, 0xA0)));
        }
        if (keepKind == KindStation && qs(st.id) == keepId) {
            keepItem = stItem;
        }
        for (int j = 0; j < st.jobs.size(); ++j) {
            const JobCfg& job = st.jobs[j];
            QTreeWidgetItem* jItem = new QTreeWidgetItem(stItem);
            jItem->setText(0, T("流程 ") + qs(job.name));
            jItem->setData(0, RoleId, qs(job.id));
            jItem->setData(0, RoleKind, KindJob);
            if (!job.enabled || !st.enabled) {
                if (!job.enabled) {
                    jItem->setText(0, jItem->text(0) + T("（未启用）"));
                }
                jItem->setData(0, RoleDisabled, true);
                jItem->setForeground(0, QBrush(QColor(0xA0, 0xA0, 0xA0)));
            }
            if (keepKind == KindJob && qs(job.id) == keepId) {
                keepItem = jItem;
            }
            // UTF-8 字符串转 QString
            if (keepKind == KindTool && qs(job.id) == m_jobId) {
                keepItem = jItem;
            }
        }
    }
    m_tree->expandAll();
    if (keepItem) {
        m_tree->setCurrentItem(keepItem);
    }
    refreshCanvas();
    updateToolButtons();
    JobCfg* coloredJob = m_lastRunJobId.isEmpty() ? 0 : m_sol->findJobConfig(oid(m_lastRunJobId));
    refreshTreeRunStatus(coloredJob);
}

// 刷新树上的运行状态
void DesignWorkspace::refreshTreeRunStatus(const JobCfg* scopeJob)
{
    if (!m_tree || !m_station || !scopeJob) {
        if (m_tree && !scopeJob) {
            QTreeWidgetItemIterator it(m_tree);
            while (*it) {
                QTreeWidgetItem* node = *it;
                if (node->data(0, RoleKind).toInt() == KindTool) {
                    clearTreeToolRunLook(node);
                }
                ++it;
            }
        }
        return;
    }
    Job* jobRt = m_station->findJob(scopeJob->id);
    if (!jobRt) {
        return;
    }
    const QString scopeId = qs(scopeJob->id);
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        QTreeWidgetItem* node = *it;
        if (node->data(0, RoleKind).toInt() != KindTool) {
            ++it;
            continue;
        }
        const QString toolId = node->data(0, RoleId).toString();
        if (treeToolJobId(node) != scopeId || !scopeJob->findTool(oid(toolId))) {
            clearTreeToolRunLook(node);
            ++it;
            continue;
        }
        bool tok = false;
        const ToolCfg* tRun = scopeJob->findTool(oid(toolId));
        if (tRun && !tRun->enabled) {
            ++it;
            continue;
        }
        if (jobRt->cachedToolOk(oid(toolId), &tok)) {
            applyTreeToolRunLook(node, tok);
        } else {
            clearTreeToolRunLook(node);
        }
        ++it;
    }
    if (m_tree->viewport()) {
        m_tree->viewport()->update();
    }
}

// 双击树节点
void DesignWorkspace::onTreeDouble(QTreeWidgetItem* item, int)
{
    if (!item) {
        return;
    }
    const int kind = item->data(0, RoleKind).toInt();
    const QString id = item->data(0, RoleId).toString();
    if (kind == KindSol) {
        emit stationsRequested();
    } else if (kind == KindStation) {
        emit stationActivated(id);
    } else if (kind == KindJob) {
        emit jobActivated(id);
    } else if (kind == KindTool) {
        selectTool(id);
        emit toolActivated(id);
        onOpenDebug();
    }
}

// 单击树节点
void DesignWorkspace::onTreeClicked(QTreeWidgetItem* item, int)
{
    if (!item) {
        return;
    }
    const int kind = item->data(0, RoleKind).toInt();
    const QString id = item->data(0, RoleId).toString();
    if (kind == KindJob) {
        m_jobId = id;
        m_toolId.clear();
        if (m_flowCanvas) {
            m_flowCanvas->setFallbackToolId(QString());
            m_flowCanvas->bindJob(m_sol->findJobConfig(oid(id)));
        }
        refreshCanvas();
        refreshJobResultPreview();
        updateToolButtons();
    } else if (kind == KindTool) {
        selectTool(id);
        emit toolActivated(id);
    }
}

// 选中工具
void DesignWorkspace::selectTool(const QString& toolId)
{
    m_toolId = toolId;
    StationCfg* st = nullptr;
    JobCfg* job = m_sol->findJobCfgOfTool(oid(toolId), &st);
    ToolCfg* t = m_sol->findTool(oid(toolId));
    if (job) {
        m_jobId = qs(job->id);
    }
    if (m_flowCanvas) {
        m_flowCanvas->setFallbackToolId(m_toolId);
        m_flowCanvas->bindJob(job);
    }
    if (t) {
        applyDrawingsToView();
        refreshResult();
        syncToolView(true);
        syncDrawingInteractionView();
    } else {
        refreshResult();
    }
    updateDesignDrawUi();
    refreshCanvas();
    updateToolButtons();
}

// 把界面参数写回工具
void DesignWorkspace::applyParamsFromUi()
{
}

// 按权限开关调试
void DesignWorkspace::setDebugPerms(bool canDebug)
{
    if (m_btnRunTool) {
        m_btnRunTool->setEnabled(canDebug);
    }
    if (m_btnRunJob) {
        m_btnRunJob->setEnabled(canDebug);
    }
    if (m_btnDebug) {
        m_btnDebug->setEnabled(canDebug && !m_toolId.isEmpty());
    }
}

// 图上 ROI 变化
void DesignWorkspace::onRoi(const HsCommData::Graphic& g)
{
    ToolCfg* t = m_sol->findTool(oid(m_toolId));
    if (!t || !m_session->has(PermEditSolution)) {
        return;
    }
    const int idx = roiIndexFromDrawingId(g.id);
    if (idx >= 0 && idx < static_cast<int>(t->effectiveRois().size())) {
        t->applyDrawnRoiAt(g, idx);
    } else {
        t->applyDrawnRoi(g);
    }
    m_sol->dirty = true;
    appendLog(T("ROI 已写入 ") + qs(t->name));
    afterRoiEdited();
    syncDrawingInteractionView();
}

// 调试窗 ROI 变化
void DesignWorkspace::onDebugRoiChanged()
{
    if (m_debug && m_debug->isVisible()) {
        m_debug->reloadDrawings();
    }
}

// 调试保存后刷新草稿
void DesignWorkspace::onDebugDraftSaved()
{
    afterRoiEdited();
    reloadTree();
    emit namesChanged();
}

// 调试试运行结果
void DesignWorkspace::onDebugTestRun(const ToolOut& out)
{
    showToolResult(qs(out.toolId), out.image, qg(out.objects), &out);
}

// ROI 改完后同步
void DesignWorkspace::afterRoiEdited()
{
    applyDrawingsToView();
    refreshResult();
    if (m_debug && m_debug->isVisible()) {
        m_debug->reloadDrawings();
    }
}

// 把图形画到图像窗
void DesignWorkspace::applyDrawingsToView()
{
    ToolCfg* t = m_sol->findTool(oid(m_toolId));
    m_view->setDrawings(t ? t->drawingsForDisplay(-1) : std::vector<HsCommData::Graphic>());
    const bool showLabels = m_view && m_view->drawTool() != HsCtrl::ToolBrowse;
    overlayRoiDetails(m_view, t, -1, showLabels);
}

// 同步图像窗的交互模式
void DesignWorkspace::syncDrawingInteractionView()
{
    ToolCfg* t = m_sol->findTool(oid(m_toolId));
    const bool has = t && !t->effectiveRois().empty();
    const bool drawOn = m_chkDrawRoi && m_chkDrawRoi->isChecked();
    const bool editable = drawOn || (m_view && m_view->drawTool() != HsCtrl::ToolBrowse);
    syncDrawingInteraction(m_view, editable, -1, has);
}

// 按当前工具显隐「绘制 ROI」与绘制条
void DesignWorkspace::updateDesignDrawUi()
{
    ToolCfg* t = m_sol->findTool(oid(m_toolId));
    const bool canRoi = t && t->usesRoi() && m_session && m_session->has(PermEditSolution);
    if (m_chkDrawRoi) {
        m_chkDrawRoi->setVisible(canRoi);
        if (!canRoi && m_chkDrawRoi->isChecked()) {
            m_chkDrawRoi->blockSignals(true);
            m_chkDrawRoi->setChecked(false);
            m_chkDrawRoi->blockSignals(false);
        }
    }
    onDesignDrawModeChanged();
}

// 设计区绘制模式开/关
void DesignWorkspace::onDesignDrawModeChanged()
{
    const bool on = m_chkDrawRoi && m_chkDrawRoi->isVisible() && m_chkDrawRoi->isChecked();
    if (m_drawPad) {
        if (on) {
            m_drawPad->setImageWindow(m_view);
            placeDesignDrawPad();
            m_drawPad->show();
            m_drawPad->raise();
            m_drawPad->startTool(HsCtrl::ToolRect1);
        } else {
            m_drawPad->hide();
            if (m_view) {
                m_view->setDrawTool(HsCtrl::ToolBrowse);
            }
        }
    }
    applyDrawingsToView();
    syncDrawingInteractionView();
}

// 绘制小窗摆到图像区左上外侧
void DesignWorkspace::placeDesignDrawPad()
{
    if (!m_drawPad || !m_shell) {
        return;
    }
    const QPoint g = m_shell->mapToGlobal(QPoint(8, 8));
    m_drawPad->move(g);
}

// 刷新结果面板
void DesignWorkspace::refreshResult(const ToolOut* runMeta, bool afterJobRun)
{
    if (!m_result) {
        return;
    }
    ToolCfg* t = m_sol->findTool(oid(m_toolId));
    const ToolOut* use = nullptr;
    if (runMeta && t && runMeta->toolId == t->id) {
        use = runMeta;
    } else if (!afterJobRun && m_hasLastRun && t && m_lastRun.toolId == t->id) {
        use = &m_lastRun;
    }
    Job* jobRt = t ? currentJobForTool(m_station, t->id) : nullptr;
    m_result->refresh(m_sol, t, jobRt, use, afterJobRun);
}

// 流程跑完后同步界面
void DesignWorkspace::syncAfterJobRun(const ToolOut& jobOut)
{
    if (!m_station) {
        return;
    }
    QString jobId = m_jobId;
    if (jobId.isEmpty() && !m_toolId.isEmpty()) {
        JobCfg* j = m_sol->findJobCfgOfTool(oid(m_toolId), 0);
        if (j) {
            jobId = qs(j->id);
        }
    }
    JobCfg* job = jobId.isEmpty() ? 0 : m_sol->findJobConfig(oid(jobId));
    if (!job) {
        showToolResult(m_toolId, jobOut.image, qg(jobOut.objects), &jobOut, true);
        return;
    }
    m_jobId = jobId;
    Job* jobRt = m_station->findJob(job->id);

    // 有当前工具则显示其结果
if (!m_toolId.isEmpty()) {
        ToolCfg* t = m_sol->findTool(oid(m_toolId));
        if (t && job->findTool(t->id)) {
            HsCommData::HsImage fb;
            QVector<HsCommData::Graphic> ov;
            if (jobRt) {
                fb = jobRt->cachedImage(t->id);
                ov = qg(jobRt->cachedOverlay(t->id));
            }
            if (fb.isNull() && !jobOut.image.isNull()) {
                fb = jobOut.image;
            }
            if (ov.isEmpty()) {
                ov = qg(jobOut.objects);
            }
            showToolResult(m_toolId, fb, ov, 0, true);
            return;
        }
    }

    showJobResult(job, jobOut);
}

// 显示流程运行结果
void DesignWorkspace::showJobResult(const JobCfg* job, const ToolOut& jobOut)
{
    Job* jobRt = (job && m_station) ? m_station->findJob(job->id) : nullptr;
    if (!job || !jobRt) {
        return;
    }
    m_lastRun = jobOut;
    m_hasLastRun = true;

    // 底图：流程源图，其次第一个有缓存的取图工具，最后用流程输出图
    HsCommData::HsImage base = jobOut.source;
    if (base.isNull()) {
        for (size_t i = 0; i < job->tools.size(); ++i) {
            const ToolCfg& t = job->tools[i];
            if (t.type == ToolType::Acquire && jobRt->hasCache(t.id)) {
                base = jobRt->cachedImage(t.id);
                break;
            }
        }
    }
    if (base.isNull() && !jobOut.image.isNull()) {
        base = jobOut.image;
    }

    // 叠加按执行顺序拼各工具缓存，都没有时用流程输出对象
    QVector<HsCommData::Graphic> ov;
    const std::vector<ObjectId> order = job->execOrder();
    for (size_t i = 0; i < order.size(); ++i) {
        const std::vector<HsCommData::HsObject> toolOv = jobRt->cachedOverlay(order[i]);
        if (!toolOv.empty()) {
            const QVector<HsCommData::Graphic> qov = qg(toolOv);
            for (int k = 0; k < qov.size(); ++k) {
                ov.append(qov[k]);
            }
        }
    }
    if (ov.isEmpty() && !jobOut.objects.empty()) {
        ov = qg(jobOut.objects);
    }

    if (!base.isNull()) {
        applyFrame(m_view, base);
        m_view->fitToWindow();
        if (!ov.isEmpty()) {
            m_view->setOverlay(toStd(ov), m_view->currentFrameId());
        } else {
            m_view->setOverlay(std::vector<HsCommData::Graphic>(), m_view->currentFrameId());
        }
    } else if (!ov.isEmpty()) {
        m_view->setOverlays(toStd(ov));
    }
    m_view->setDrawings(std::vector<HsCommData::Graphic>());
    if (m_shell) {
        for (int i = static_cast<int>(order.size()) - 1; i >= 0; --i) {
            const ToolIo io = jobRt->cachedIo(order[static_cast<size_t>(i)]);
            if (CloudViewBinder::apply(m_shell, io, true)) {
                break;
            }
        }
    }
    m_status->setText(T("流程 ") + qs(job->name) + T("：显示采集底图与各工具叠加"));

    m_result->refreshJob(m_sol, job, jobRt, &jobOut);
    syncDrawingInteractionView();
    m_lastRunJobId = qs(job->id);
    refreshTreeRunStatus(job);
}

// 刷新流程结果预览
void DesignWorkspace::refreshJobResultPreview()
{
    JobCfg* job = m_jobId.isEmpty() ? 0 : m_sol->findJobConfig(oid(m_jobId));
    if (!job || !m_result) {
        return;
    }
    Job* jobRt = m_station ? m_station->findJob(job->id) : nullptr;
    m_result->refreshJob(m_sol, job, jobRt, m_hasLastRun ? &m_lastRun : 0);

    if (!jobRt) {
        return;
    }
    HsCommData::HsImage base;
    for (size_t i = 0; i < job->tools.size(); ++i) {
        if (job->tools[i].type == ToolType::Acquire && jobRt->hasCache(job->tools[i].id)) {
            base = jobRt->cachedImage(job->tools[i].id);
            break;
        }
    }
    if (base.isNull()) {
        return;
    }
    QVector<HsCommData::Graphic> ov;
    const std::vector<ObjectId> order = job->execOrder();
    for (size_t i = 0; i < order.size(); ++i) {
        const QVector<HsCommData::Graphic> toolOv = qg(jobRt->cachedOverlay(order[i]));
        for (int k = 0; k < toolOv.size(); ++k) {
            ov.append(toolOv[k]);
        }
    }
    applyFrame(m_view, base);
    m_view->fitToWindow();
    m_view->setOverlay(toStd(ov), m_view->currentFrameId());
    m_view->setDrawings(std::vector<HsCommData::Graphic>());
    m_status->setText(T("流程 ") + qs(job->name) + T("：显示采集底图与各工具叠加"));
}

// 同步工具图像，优先输入图
void DesignWorkspace::syncToolView(bool preferInput)
{
    Q_UNUSED(preferInput);
    ToolCfg* t = m_sol->findTool(oid(m_toolId));
    Job* jobRt = t ? currentJobForTool(m_station, t->id) : nullptr;
    if (!t || !jobRt) {
        refreshResult();
        return;
    }
    StationCfg* st = nullptr;
    JobCfg* job = m_sol->findJobCfgOfTool(t->id, &st);
    HsCommData::HsImage fb;
    if (t->type == ToolType::Acquire) {
        fb = jobRt->cachedImage(t->id);
    } else if (job) {
        fb = debugBaseFrame(t, job, jobRt);
    }
    showToolResult(m_toolId, fb, qg(jobRt->cachedOverlay(t->id)));
}

// 显示工具输出图和叠加
void DesignWorkspace::showToolResult(const QString& toolId, const HsCommData::HsImage& fb,
                                     const QVector<HsCommData::Graphic>& overlay,
                                     const ToolOut* runMeta, bool afterJobRun)
{

    const QString displayId = toolId.isEmpty() ? m_toolId : toolId;
    ToolCfg* t = m_sol->findTool(oid(displayId));
    Job* jobRt = t ? currentJobForTool(m_station, t->id) : nullptr;
    const bool metaMatches = runMeta && t && runMeta->toolId == t->id;
    if (runMeta && metaMatches) {
        m_lastRun = *runMeta;
        m_hasLastRun = true;
    }

    // 底图：取图工具用传入图或缓存，其余工具用上游输入图（传入的 fb 不用）
    StationCfg* st = nullptr;
    JobCfg* job = t ? m_sol->findJobCfgOfTool(t->id, &st) : nullptr;
    const bool isCam = t && (t->type == ToolType::Acquire);
    HsCommData::HsImage showFb;
    if (isCam) {
        if (!fb.isNull()) {
            showFb = fb;
        } else if (jobRt) {
            showFb = jobRt->cachedImage(t->id);
        }
    } else if (t && jobRt && job) {
        showFb = debugBaseFrame(t, job, jobRt);
    }

    QVector<HsCommData::Graphic> showOv = overlay;
    if (metaMatches && runMeta && !runMeta->objects.empty()) {
        showOv = qg(runMeta->objects);
    } else if (t && jobRt && showOv.isEmpty()) {
        const std::vector<HsCommData::HsObject> cachedOv = jobRt->cachedOverlay(t->id);
        if (!cachedOv.empty()) {
            showOv = qg(cachedOv);
        }
    }

    // 刚跑完本工具（非取图）时保留当前底图，只换叠加
    const bool replaceBase = isCam || !metaMatches;
    if (replaceBase && !showFb.isNull()) {
        applyFrame(m_view, showFb);
        m_view->fitToWindow();
    }
    if (!showOv.isEmpty()) {
        if (replaceBase && !showFb.isNull()) {
            m_view->setOverlay(toStd(showOv), m_view->currentFrameId());
        } else {
            m_view->setOverlays(toStd(showOv));
        }
    } else if (replaceBase && !showFb.isNull()) {
        m_view->setOverlay(std::vector<HsCommData::Graphic>(), m_view->currentFrameId());
    }
    QVector<HsCommData::Graphic> drawings;
    if (t) {
        drawings = qg(t->drawings());
    }
    m_view->setDrawings(toStd(drawings));
    const bool showLabels = m_view && m_view->drawTool() != HsCtrl::ToolBrowse;
    overlayRoiDetails(m_view, t, -1, showLabels);
    if (m_shell && t) {
        if (metaMatches && runMeta) {
            CloudViewBinder::apply(m_shell, runMeta->io, true);
        } else if (jobRt) {
            CloudViewBinder::apply(m_shell, jobRt->cachedIo(t->id), true);
        }
    }
    refreshResult(metaMatches ? runMeta : 0, afterJobRun);
    syncDrawingInteractionView();
    if (job) {
        m_lastRunJobId = qs(job->id);
        refreshTreeRunStatus(job);
    }
    if (m_debug && m_debug->isVisible()) {
        m_debug->refreshView(metaMatches ? runMeta : 0);
    }
}
// 按选中项刷新按钮
void DesignWorkspace::updateToolButtons()
{
    StationCfg* st = nullptr;
    JobCfg* job = m_sol->findJobCfgOfTool(oid(m_toolId), &st);
    if (!job) {
        job = m_sol->findJobConfig(oid(m_jobId));
    }
    const int idx = job ? job->indexOf(oid(m_toolId)) : -1;
    const bool ok = m_session->has(PermEditSolution) && idx >= 0;
    m_canEditTool = ok;
    m_canUp = ok && idx > 0;
    m_canDown = ok && job && idx < job->tools.size() - 1;
}

// 上移
void DesignWorkspace::onMoveUp()
{
    applyParamsFromUi();
    StationCfg* st = nullptr;
    JobCfg* job = m_sol->findJobCfgOfTool(oid(m_toolId), &st);
    if (!job || !m_session->has(PermEditSolution)) {
        return;
    }
    if (!job->moveTool(oid(m_toolId), -1)) {
        return;
    }
    m_sol->dirty = true;
    reloadTree();
    selectTool(m_toolId);
}

// 下移
void DesignWorkspace::onMoveDown()
{
    applyParamsFromUi();
    StationCfg* st = nullptr;
    JobCfg* job = m_sol->findJobCfgOfTool(oid(m_toolId), &st);
    if (!job || !m_session->has(PermEditSolution)) {
        return;
    }
    if (!job->moveTool(oid(m_toolId), 1)) {
        return;
    }
    m_sol->dirty = true;
    reloadTree();
    selectTool(m_toolId);
}

// 重命名
void DesignWorkspace::onRename()
{
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item || !m_session->has(PermEditSolution)) {
        return;
    }
    applyParamsFromUi();
    const int kind = item->data(0, RoleKind).toInt();
    const QString id = item->data(0, RoleId).toString();
    QString cur;
    QString label;
    if (kind == KindSol) {
        cur = qs(m_sol->name);
        label = T("方案名称");
    } else if (kind == KindStation) {
        StationCfg* st = m_sol->findStationConfig(oid(id));
        if (!st) {
            return;
        }
        cur = qs(st->name);
        label = T("工站名称");
    } else if (kind == KindJob) {
        JobCfg* job = m_sol->findJobConfig(oid(id));
        if (!job) {
            return;
        }
        cur = qs(job->name);
        label = T("流程名称");
    } else if (kind == KindTool) {
        ToolCfg* t = m_sol->findTool(oid(id));
        if (!t) {
            return;
        }
        cur = qs(t->name);
        label = T("工具名称");
    } else {
        return;
    }
    bool ok = false;
    const QString n = QInputDialog::getText(this, T("重命名"), label,
                                            QLineEdit::Normal, cur, &ok).trimmed();
    if (!ok || n.isEmpty() || n == cur) {
        return;
    }
    // 方案节点的菜单
    if (kind == KindSol) {
        m_sol->name = ss(n);
    } else if (kind == KindStation) {
        m_sol->findStationConfig(oid(id))->name = ss(n);
    } else if (kind == KindJob) {
        m_sol->findJobConfig(oid(id))->name = ss(n);
    } else if (!m_sol->findJobCfgOfTool(oid(id), 0)->renameTool(oid(id), ss(n))) {
        return;
    }
    m_sol->dirty = true;
    reloadTree();
    if (kind == KindTool) {
        selectTool(id);
    }
    emit namesChanged();
}

// 复制工具
void DesignWorkspace::onCopyTool()
{
    applyParamsFromUi();
    StationCfg* st = nullptr;
    JobCfg* job = m_sol->findJobCfgOfTool(oid(m_toolId), &st);
    if (!job || !m_session->has(PermEditSolution)) {
        return;
    }
    const ObjectId nid = job->duplicateTool(oid(m_toolId));
    if (nid.isNull()) {
        return;
    }
    m_sol->dirty = true;
    m_toolId = qs(nid);
    reloadTree();
    selectTool(m_toolId);
}

// 删除工具
void DesignWorkspace::onDeleteTool()
{
    applyParamsFromUi();
    StationCfg* st = nullptr;
    JobCfg* job = m_sol->findJobCfgOfTool(oid(m_toolId), &st);
    if (!job || !m_session->has(PermEditSolution)) {
        return;
    }
    const ObjectId tid = oid(m_toolId);
    const int idx = job->indexOf(tid);
    if (idx < 0) {
        return;
    }
    if (QMessageBox::question(this, T("删除"), T("删除此工具？")) != QMessageBox::Yes) {
        return;
    }
    if (!job->removeTool(tid)) {
        return;
    }
    m_sol->dirty = true;
    QString next;
    const ToolCfg* n = job->findToolAt(qMin(idx, static_cast<int>(job->tools.size()) - 1));
    if (n) {
        next = qs(n->id);
    }
    m_toolId = next;
    reloadTree();
    if (!next.isEmpty()) {
        selectTool(next);
    } else {
        refreshCanvas();
        updateToolButtons();
    }
}

// 树右键菜单
void DesignWorkspace::onTreeMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) {
        return;
    }
    m_tree->setCurrentItem(item);
    const int kind = item->data(0, RoleKind).toInt();
    const QString id = item->data(0, RoleId).toString();
    if (kind == KindTool) {
        selectTool(id);
    } else if (kind == KindJob) {
        m_jobId = id;
        refreshCanvas();
    }
    updateToolButtons();
    // 方案、工站、流程节点只有重命名；工具节点另有移动、复制、删除
    const bool canEdit = m_session->has(PermEditSolution);
    QMenu menu(this);
    if (kind == KindSol || kind == KindStation || kind == KindJob) {
        QAction* rename = menu.addAction(T("重命名"));
        rename->setEnabled(canEdit);
        QAction* chosen = menu.exec(m_tree->mapToGlobal(pos));
        if (chosen == rename) {
            onRename();
        }
        return;
    }
    if (kind != KindTool) {
        return;
    }
    QAction* up = menu.addAction(T("上移"));
    QAction* down = menu.addAction(T("下移"));
    menu.addSeparator();
    QAction* rename = menu.addAction(T("重命名"));
    QAction* copy = menu.addAction(T("复制"));
    QAction* del = menu.addAction(T("删除"));
    up->setEnabled(m_canUp);
    down->setEnabled(m_canDown);
    rename->setEnabled(m_canEditTool);
    copy->setEnabled(m_canEditTool);
    del->setEnabled(m_canEditTool);
    QAction* chosen = menu.exec(m_tree->mapToGlobal(pos));
    if (chosen == up) {
        onMoveUp();
    } else if (chosen == down) {
        onMoveDown();
    } else if (chosen == rename) {
        onRename();
    } else if (chosen == copy) {
        onCopyTool();
    } else if (chosen == del) {
        onDeleteTool();
    }
}

// 画布选中工具
void DesignWorkspace::onCanvasTool(const QString& toolId)
{
    if (toolId.isEmpty()) {
        return;
    }
    selectTool(toolId);
}

// 画布双击工具
void DesignWorkspace::onCanvasToolDouble(const QString& toolId)
{
    if (toolId.isEmpty()) {
        return;
    }
    selectTool(toolId);
    emit toolActivated(toolId);
    onOpenDebug();
}

// 画布选中组
void DesignWorkspace::onCanvasGroup(const QString& groupId)
{
    JobCfg* job = m_sol->findJobConfig(oid(m_jobId));
    if (!job) {
        job = m_sol->findJobCfgOfTool(oid(m_toolId), 0);
    }
    if (m_flowCanvas) {
        m_flowCanvas->bindJob(job);
        m_flowCanvas->showJob(groupId);
    }
}

// 画布右键菜单
void DesignWorkspace::onCanvasMenu(const QString& id, int kind, const QPoint& globalPos)
{
    JobCfg* job = m_sol->findJobConfig(oid(m_jobId));
    if (!job) {
        job = m_sol->findJobCfgOfTool(oid(m_toolId), 0);
    }
    // 连线：恢复自动路由或断开，需编辑权限
    const bool canEdit = m_session->has(PermEditSolution);
    if (kind == CanvasHitWire) {
        if (!canEdit || !job) {
            return;
        }
        QMenu menu(this);
        QAction* reset = menu.addAction(T("恢复自动路由"));
        QAction* del = menu.addAction(T("断开连线"));
        QAction* chosen = menu.exec(globalPos);
        if (!m_flowCanvas || !chosen) {
            return;
        }
        if (chosen == reset) {
            m_flowCanvas->resetLinkRoute(id.toInt());
        } else if (chosen == del) {
            m_flowCanvas->unlinkLink(id.toInt());
        }
        return;
    }
    if (kind == CanvasHitTool && !id.isEmpty()) {
        selectTool(id);
    }
    // 节点或空白处：添加工具子菜单列出全部注册工具，其余项可用性跟随工具按钮
    updateToolButtons();
    QMenu menu(this);
    QMenu* addMenu = menu.addMenu(T("在此处添加工具"));
    addMenu->setEnabled(canEdit && job);
    const std::vector<ToolBase*> tools = ToolRegistry::instance().all();
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        QAction* a = addMenu->addAction(qs(tools[static_cast<size_t>(i)]->displayName()));
        a->setData(qs(tools[static_cast<size_t>(i)]->toolName()));
    }
    menu.addSeparator();
    QAction* up = menu.addAction(T("上移"));
    QAction* down = menu.addAction(T("下移"));
    menu.addSeparator();
    QAction* rename = menu.addAction(T("重命名"));
    QAction* copy = menu.addAction(T("复制"));
    QAction* del = menu.addAction(T("删除"));
    up->setEnabled(m_canUp);
    down->setEnabled(m_canDown);
    rename->setEnabled(m_canEditTool);
    copy->setEnabled(m_canEditTool);
    del->setEnabled(m_canEditTool);
    menu.addSeparator();
    QAction* autoLay = menu.addAction(T("自动布局"));
    autoLay->setEnabled(canEdit && job);
    QAction* zoom = menu.addAction(T("复位缩放"));
    QAction* chosen = menu.exec(globalPos);
    if (!chosen) {
        return;
    }
    if (chosen == autoLay) {
        if (job && canEdit) {
            job->autoLayoutCanvas();
            m_sol->dirty = true;
            refreshCanvas();
        }
        return;
    }
    if (chosen == zoom) {
        if (m_flowCanvas) {
            m_flowCanvas->resetZoom();
        }
        return;
    }
    if (chosen == up) {
        onMoveUp();
        return;
    }
    if (chosen == down) {
        onMoveDown();
        return;
    }
    if (chosen == rename) {
        onRename();
        return;
    }
    if (chosen == copy) {
        onCopyTool();
        return;
    }
    if (chosen == del) {
        onDeleteTool();
        return;
    }
    // 添加工具：默认绑定所属工站的第一台设备（经流程第一个工具反查工站，空流程时不绑），按点击的节点挂到执行树
    const QString tn = chosen->data().toString();
    if (tn.isEmpty() || !job || !canEdit) {
        return;
    }
    ObjectId dev;
    StationCfg* st = nullptr;
    if (!job->tools.empty()) {
        m_sol->findJobCfgOfTool(job->tools[0].id, &st);
    }
    if (st && !st->boundDevices.empty()) {
        dev = st->boundDevices[0].deviceId;
    }
    const ToolCfg t = ToolCfg::make(ss(tn), dev);
    if (!job->insertTool(t, oid(id.isEmpty() ? m_toolId : id))) {
        return;
    }
    m_sol->dirty = true;
    m_jobId = qs(job->id);
    reloadTree();
    selectTool(qs(t.id));
}

// 双击算法库
void DesignWorkspace::onAlgLibDouble(QTreeWidgetItem* item, int)
{
    if (!item || !m_session || !m_session->has(PermEditSolution)) {
        return;
    }
    const QString tn = item->data(0, Qt::UserRole).toString();
    if (tn.isEmpty()) {
        return;
    }
    JobCfg* job = m_sol ? m_sol->findJobConfig(oid(m_jobId)) : nullptr;
    if (!job) {
        QMessageBox::information(this, T("提示"), T("请先选择一条流程。"));
        return;
    }
    if (tn == QLatin1String("group.parallel") || tn == QLatin1String("group.if") || tn == QLatin1String("group.loop")) {
        QMessageBox::information(this, T("提示"), T("并行组、条件组和循环组请在流程配置里添加。"));
        return;
    }
    ObjectId dev;
    StationCfg* st = m_sol->findStationCfgOfJob(job->id);
    if (st && !st->boundDevices.empty()) {
        dev = st->boundDevices[0].deviceId;
    }
    const ToolCfg t = ToolCfg::make(ss(tn), dev);
    if (!job->insertTool(t, oid(m_toolId))) {
        return;
    }
    m_sol->dirty = true;
    m_jobId = qs(job->id);
    reloadTree();
    refreshCanvas();
    selectTool(qs(t.id));
}
void DesignWorkspace::onCanvasLinksChanged()
{
    refreshCanvas();
    refreshResult();
}

// 切换语言后刷新文案
void DesignWorkspace::retranslateUi()
{
    retranslateBound(this);
    if (m_solTitle) {
        m_solTitle->setText(T("方案"));
    }
    if (m_algoTitle) {
        m_algoTitle->setText(T("工具库"));
    }
    if (m_canvasTitle) {
        m_canvasTitle->setText(T("流程画布"));
    }
    if (m_toolLib) {
        fillRegisteredToolTree(m_toolLib, true);
    }
    QStringList hh;
    hh << T("时间") << T("级别") << T("消息");
    m_log->setHorizontalHeaderLabels(hh);
    reloadTree();
}
// 图像窗口
HsCtrl::HsImageWindow* DesignWorkspace::imageWindow() const
{
    return m_view;
}



// 当前工具 id
QString DesignWorkspace::currentToolId() const
{
    return m_toolId;
}



// 当前流程 id
QString DesignWorkspace::currentJobId() const
{
    return m_jobId;
}



}


#include "moc_DesignWorkspace.cpp"
