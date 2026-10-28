/**
 * @file JobFlowCanvasPanel.cpp
 * @brief JobFlowCanvasPanel实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "JobFlowCanvasPanel.h"
#include "JobCanvas.h"
#include "Perm.h"
#include "QtConv.h"
#include "Session.h"
#include "UiText.h"
#include <QCursor>
#include <QLabel>
#include <QToolTip>
#include <QVector>
#include <QPointF>
#include <QVBoxLayout>

namespace Vision {

// 流程画布面板
JobFlowCanvasPanel::JobFlowCanvasPanel(Session* session, Solution* solution, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_sol(solution)
    , m_job(nullptr)
    , m_editable(true)
    , m_draftMode(false)
    , m_selectedLinkIndex(-1)
    , m_hint(nullptr)
    , m_canvas(nullptr)
{
    m_hint = new QLabel(this);
    m_hint->setObjectName(QLatin1String("hint"));
    m_hint->setWordWrap(true);
    m_hint->setVisible(false);

    m_canvas = new JobCanvas(this);
    connect(m_canvas, SIGNAL(toolClicked(QString)), this, SLOT(onToolClickedClearWire(QString)));
    connect(m_canvas, SIGNAL(toolDoubleClicked(QString)), this, SIGNAL(toolDoubleClicked(QString)));
    connect(m_canvas, SIGNAL(groupClicked(QString)), this, SIGNAL(groupClicked(QString)));
    connect(m_canvas, SIGNAL(canvasMenuRequested(QString,int,QPoint)),
            this, SIGNAL(canvasMenuRequested(QString,int,QPoint)));
    connect(m_canvas, SIGNAL(linkPorts(QString,QString,QString,QString)),
            this, SLOT(onLinkPorts(QString,QString,QString,QString)));
    connect(m_canvas, SIGNAL(unlinkRequested(int)), this, SLOT(onUnlink(int)));
    connect(m_canvas, SIGNAL(nodeMoved(QString,qreal,qreal)),
            this, SLOT(onNodeMoved(QString,qreal,qreal)));
    connect(m_canvas, SIGNAL(groupMoved(QString,qreal,qreal)),
            this, SLOT(onGroupMoved(QString,qreal,qreal)));
    connect(m_canvas, SIGNAL(wireClicked(int)), this, SLOT(onWireClicked(int)));
    connect(m_canvas, SIGNAL(linkRouteChanged(int)),
            this, SLOT(onLinkRouteChanged(int)));
    connect(m_canvas, SIGNAL(linkRouteReset(int)),
            this, SLOT(onLinkRouteReset(int)));

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->addWidget(m_hint);
    lay->addWidget(m_canvas, 1);
}

// 内部画布
JobCanvas* JobFlowCanvasPanel::canvas() const
{
    return m_canvas;
}

// 是否允许改画布
void JobFlowCanvasPanel::setEditable(bool editable)
{
    m_editable = editable;
}

// 是否草稿模式
void JobFlowCanvasPanel::setDraftMode(bool draft)
{
    m_draftMode = draft;
}

// 绑定要画的流程
void JobFlowCanvasPanel::bindJob(JobCfg* job)
{
    m_job = job;
    if (job) {
        m_jobId = qs(job->id);
    }
}

// 设置流程 id
void JobFlowCanvasPanel::setJobId(const QString& jobId)
{
    m_jobId = jobId;
    m_job = m_sol ? m_sol->findJobConfig(oid(jobId)) : 0;
}

// 设置回退工具 id
void JobFlowCanvasPanel::setFallbackToolId(const QString& toolId)
{
    m_fallbackToolId = toolId;
}

// 设置空状态提示
void JobFlowCanvasPanel::setHintText(const QString& text)
{
    if (!m_hint) {
        return;
    }
    const bool show = !text.isEmpty();
    m_hint->setVisible(show);
    if (show) {
        m_hint->setText(text);
    } else {
        m_hint->clear();
    }
}

// 按流程重画画布
void JobFlowCanvasPanel::showJob(const QString& selectedId)
{
    if (!m_canvas) {
        return;
    }
    if (!selectedId.isEmpty()) {
        m_selectedLinkIndex = -1;
    }
    JobCfg* job = resolveJob();
    if (job) {
        m_job = job;
        m_canvas->showJob(job, selectedId, m_selectedLinkIndex);
    } else {
        m_job = 0;
        m_selectedLinkIndex = -1;
        m_canvas->showJob(0, QString(), -1);
    }
}

// 缩放置回 100%
void JobFlowCanvasPanel::resetZoom()
{
    if (m_canvas) {
        m_canvas->resetZoom();
    }
}

// 断开一条连线
void JobFlowCanvasPanel::unlinkLink(int linkIndex)
{
    onUnlink(linkIndex);
}

// 解析当前流程配置
JobCfg* JobFlowCanvasPanel::resolveJob() const
{
    if (m_job) {
        return m_job;
    }
    if (!m_sol) {
        return 0;
    }
    if (!m_jobId.isEmpty()) {
        JobCfg* job = m_sol->findJobConfig(oid(m_jobId));
        if (job) {
            return job;
        }
    }
    if (!m_fallbackToolId.isEmpty()) {
        return m_sol->findJobCfgOfTool(oid(m_fallbackToolId), 0);
    }
    return 0;
}

// 是否允许改连线
bool JobFlowCanvasPanel::canEditLinks() const
{
    return m_editable && m_session && m_session->has(PermEditSolution);
}

// 端口拖线连上
void JobFlowCanvasPanel::onLinkPorts(const QString& fromId, const QString& fromPort,
                                     const QString& toId, const QString& toPort)
{
    if (!canEditLinks() || !m_sol) {
        return;
    }
    JobCfg* job = resolveJob();
    if (!job) {
        return;
    }
    std::string err;
    if (!job->connectPorts(oid(fromId), portSlotFromKey(ss(fromPort)), oid(toId), portSlotFromKey(ss(toPort)), &err)) {
        QToolTip::showText(QCursor::pos(), qs(err), this);
        return;
    }
    if (!m_draftMode) {
        m_sol->dirty = true;
    }
    m_job = job;
    m_selectedLinkIndex = -1;
    m_canvas->showJob(job, toId, -1);
    emit linksChanged();
}

// 点工具时清掉选中的线
void JobFlowCanvasPanel::onToolClickedClearWire(const QString& toolId)
{
    m_selectedLinkIndex = -1;
    emit toolClicked(toolId);
}

// 点中一条连线
void JobFlowCanvasPanel::onWireClicked(int linkIndex)
{
    m_selectedLinkIndex = linkIndex;
}

// 用户改完连线折线
void JobFlowCanvasPanel::onLinkRouteChanged(int linkIndex)
{
    if (!canEditLinks() || !m_sol || !m_canvas) {
        return;
    }
    JobCfg* job = resolveJob();
    if (!job || linkIndex < 0 || linkIndex >= static_cast<int>(job->links.size())) {
        return;
    }
    ToolLink& L = job->links[static_cast<size_t>(linkIndex)];
    L.routeCustom = true;
    L.routePoints = std::vector<RoutePoint>();
    const QVector<QPointF> mids = m_canvas->pendingRoutePoints();
    for (int i = 0; i < mids.size(); ++i) {
        L.routePoints.push_back(RoutePoint(mids[i].x(), mids[i].y()));
    }
    if (!m_draftMode) {
        m_sol->dirty = true;
    }
    m_job = job;
    m_canvas->showJob(job, QString(), m_selectedLinkIndex);
    emit linksChanged();
}

// 恢复连线自动路由
void JobFlowCanvasPanel::onLinkRouteReset(int linkIndex)
{
    resetLinkRoute(linkIndex);
}

// 恢复指定连线自动路由
void JobFlowCanvasPanel::resetLinkRoute(int linkIndex)
{
    if (!canEditLinks() || !m_sol) {
        return;
    }
    JobCfg* job = resolveJob();
    if (!job || linkIndex < 0 || linkIndex >= static_cast<int>(job->links.size())) {
        return;
    }
    ToolLink& L = job->links[static_cast<size_t>(linkIndex)];
    L.routeCustom = false;
    L.routePoints.clear();
    if (!m_draftMode) {
        m_sol->dirty = true;
    }
    m_job = job;
    m_canvas->showJob(job, QString(), m_selectedLinkIndex);
    emit linksChanged();
}

// 断开选中的连线
void JobFlowCanvasPanel::onUnlink(int linkIndex)
{
    if (!canEditLinks() || !m_sol) {
        return;
    }
    JobCfg* job = resolveJob();
    if (!job || !job->disconnectLink(linkIndex)) {
        return;
    }
    if (!m_draftMode) {
        m_sol->dirty = true;
    }
    m_job = job;
    m_selectedLinkIndex = -1;
    m_canvas->showJob(job, QString(), -1);
    emit linksChanged();
}

// 组被拖动
void JobFlowCanvasPanel::onGroupMoved(const QString& groupId, qreal x, qreal y)
{
    if (!canEditLinks() || !m_sol || groupId.isEmpty()) {
        return;
    }
    JobCfg* job = resolveJob();
    ToolGroup* g = job ? job->findGroup(oid(groupId)) : nullptr;
    if (!g) {
        return;
    }
    if (g->canvasPlaced && g->canvasX == x && g->canvasY == y) {
        return;
    }
    g->canvasX = x;
    g->canvasY = y;
    g->canvasPlaced = true;
    if (!m_draftMode) {
        m_sol->dirty = true;
    }
    m_job = job;
    m_canvas->showJob(job, groupId, m_selectedLinkIndex);
}

// 工具节点被拖动
void JobFlowCanvasPanel::onNodeMoved(const QString& toolId, qreal x, qreal y)
{
    if (!canEditLinks() || !m_sol || toolId.isEmpty()) {
        return;
    }
    JobCfg* job = resolveJob();
    ToolCfg* t = job ? job->findTool(oid(toolId)) : nullptr;
    if (!t) {
        t = m_sol->findTool(oid(toolId));
    }
    if (!t) {
        return;
    }
    if (t->canvasPlaced && t->canvasX == x && t->canvasY == y) {
        return;
    }
    t->canvasX = x;
    t->canvasY = y;
    t->canvasPlaced = true;
    if (!m_draftMode) {
        m_sol->dirty = true;
    }
    emit nodeMoved(toolId, x, y);
}

}

#include "moc_JobFlowCanvasPanel.cpp"
