/**
 * @file RuntimeWorkspace.cpp
 * @brief RuntimeWorkspace实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RuntimeWorkspace.h"
#include "StationRuntimePanel.h"
#include "UiPageHost.h"
#include "Station.h"
#include "QtConv.h"
#include "Theme.h"
#include "UiText.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>

namespace Vision {

// 运行壳：全局控制 + 多工站面板宿主
RuntimeWorkspace::RuntimeWorkspace(QWidget* parent)
    : QWidget(parent)
    , m_sol(nullptr)
    , m_previewSt(nullptr)
    , m_previewStation(nullptr)
    , m_side(nullptr)
    , m_host(nullptr)
    , m_hostLay(nullptr)
    , m_tabs(nullptr)
    , m_tileHost(nullptr)
    , m_tileLay(nullptr)
    , m_uiHost(nullptr)
    , m_solBanner(nullptr)
{
    m_btnStart = new QPushButton(T("开始"), this);
    bindTrKey(m_btnStart, "开始");
    m_btnTrigger = new QPushButton(T("手动测试"), this);
    bindTrKey(m_btnTrigger, "手动测试");
    m_btnStop = new QPushButton(T("停止"), this);
    bindTrKey(m_btnStop, "停止");
    m_btnReset = new QPushButton(T("清零"), this);
    bindTrKey(m_btnReset, "清零");
    m_btnSwitch = new QPushButton(T("切换方案"), this);
    bindTrKey(m_btnSwitch, "切换方案");
    m_btnLayout = new QPushButton(T("画面配置"), this);
    bindTrKey(m_btnLayout, "画面配置");
    m_btnStart->setObjectName(QLatin1String("rtGhost")); // 未运行：普通底；运行中改 rtStart 绿
    m_btnTrigger->setObjectName(QLatin1String("rtGhost"));
    m_btnStop->setObjectName(QLatin1String("rtStop"));
    m_btnReset->setObjectName(QLatin1String("rtGhost"));
    m_btnSwitch->setObjectName(QLatin1String("rtGhost"));
    connect(m_btnStart, SIGNAL(clicked()), this, SIGNAL(startRequested()));
    connect(m_btnTrigger, SIGNAL(clicked()), this, SIGNAL(softTriggerRequested()));
    connect(m_btnStop, SIGNAL(clicked()), this, SIGNAL(stopRequested()));
    connect(m_btnReset, SIGNAL(clicked()), this, SIGNAL(resetRequested()));
    connect(m_btnSwitch, SIGNAL(clicked()), this, SIGNAL(switchSolutionRequested()));
    connect(m_btnLayout, SIGNAL(clicked()), this, SIGNAL(configureViewRequested()));

    QFrame* left = makeCard(this);
    left->setFixedWidth(140);
    QVBoxLayout* lLay = new QVBoxLayout(left);
    // 上：产线操作（开始/停止成对，软触发次之，清零单独）
    lLay->addWidget(m_btnStart);
    lLay->addWidget(m_btnStop);
    lLay->addWidget(m_btnTrigger);
    lLay->addWidget(m_btnReset);
    lLay->addStretch();
    // 下：换方案 / 画面配置（非节拍内操作，与生产按钮隔开）
    lLay->addWidget(m_btnSwitch);
    lLay->addWidget(m_btnLayout);
    m_side = left;

    m_solBanner = new QLabel(T("方案：-"), this);
    bindTrKey(m_solBanner, "方案：-");
    m_solBanner->setObjectName(QLatin1String("section"));

    m_host = new QWidget(this);
    m_hostLay = new QVBoxLayout(m_host);
    m_hostLay->setContentsMargins(0, 0, 0, 0);
    m_hostLay->setSpacing(4);

    QVBoxLayout* right = new QVBoxLayout();
    right->setContentsMargins(0, 0, 0, 0);
    right->addWidget(m_solBanner);
    right->addWidget(m_host, 1);

    QHBoxLayout* lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->addWidget(left);
    lay->addLayout(right, 1);
}

// 焦点面板图像窗
HsCtrl::HsImageWindow* RuntimeWorkspace::imageWindow() const
{
    StationRuntimePanel* p = activePanel();
    return p ? p->imageWindow() : 0;
}

// 重建壳并绑定预览实例
void RuntimeWorkspace::bindContext(Solution* sol, StationCfg* previewSt, Station* previewStation)
{
    m_sol = sol;
    m_previewSt = previewSt;
    m_previewStation = previewStation;
    if (sol) {
        setSolutionName(qs(sol->name));
    }
    rebuildHost();
}

// 运行实例挂到对应面板（已有面板只换指针，避免停止后再开刷掉画面）
void RuntimeWorkspace::setStationInstance(const ObjectId& stationId, Station* station)
{
    if (!stationId.isNull()) {
        if (station) {
            m_liveStations.insert(qs(stationId), station);
        } else {
            m_liveStations.remove(qs(stationId));
        }
    }
    StationRuntimePanel* p = panelFor(stationId);
    if (!p || !m_sol) {
        if (m_uiHost) {
            m_uiHost->bindPage(firstCustomPage(), m_sol, stationMap());
        }
        return;
    }
    StationCfg* config = m_sol->findStationConfig(stationId);
    if (p->stationId() == stationId && config) {
        p->setRuntimeStation(station);
    } else {
        p->bindContext(m_sol, config, station);
    }
    if (m_uiHost) {
        m_uiHost->bindPage(firstCustomPage(), m_sol, stationMap());
    }
}

// 停止后卸下实例指针，保留各面板画面与计数
void RuntimeWorkspace::detachAllStations()
{
    m_liveStations.clear();
    const QList<StationRuntimePanel*> all = m_panels.values();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i]) {
            all[i]->setRuntimeStation(0);
        }
    }
    if (m_uiHost) {
        m_uiHost->bindPage(firstCustomPage(), m_sol, stationMap());
    }
}

UiPageHost* RuntimeWorkspace::uiPageHost() const
{
    return m_uiHost;
}

void RuntimeWorkspace::refreshCustomUi()
{
    if (m_uiHost) {
        m_uiHost->refreshBindings();
    }
}

UiPage RuntimeWorkspace::firstCustomPage() const
{
    UiPage empty;
    empty.enabled = false;
    if (!m_sol) {
        return empty;
    }
    for (size_t i = 0; i < m_sol->uiPages.size(); ++i) {
        const UiPage& p = m_sol->uiPages[i];
        if (p.enabled && !p.widgets.empty()) {
            return p;
        }
    }
    return empty;
}

QMap<QString, Station*> RuntimeWorkspace::stationMap() const
{
    QMap<QString, Station*> map = m_liveStations;
    if (m_previewSt && m_previewStation) {
        map.insert(qs(m_previewSt->id), m_previewStation);
    }
    return map;
}

// 首次进运行或方案变空后再建；已有面板则不重建
void RuntimeWorkspace::ensureHost(Solution* sol)
{
    m_sol = sol;
    if (sol) {
        setSolutionName(qs(sol->name));
    }
    if (!sol || sol->stations.empty()) {
        if (!m_panels.isEmpty()) {
            rebuildHost();
        } else {
            bindContext(sol, 0, 0);
        }
        return;
    }
    if (m_panels.isEmpty() || m_panels.size() != static_cast<int>(sol->stations.size())) {
        rebuildHost();
        return;
    }
    // 工站 id 集合变化则重建
    for (size_t i = 0; i < sol->stations.size(); ++i) {
        if (!m_panels.contains(qs(sol->stations[i].id))) {
            rebuildHost();
            return;
        }
    }
}

// 按 id 取面板
StationRuntimePanel* RuntimeWorkspace::panelFor(const ObjectId& stationId) const
{
    if (stationId.isNull()) {
        return 0;
    }
    return m_panels.value(qs(stationId), 0);
}

// 全部面板
QList<StationRuntimePanel*> RuntimeWorkspace::panels() const
{
    return m_panels.values();
}

// 兼容：单站标题（多站时写到预览站面板）
void RuntimeWorkspace::setStationName(const QString& name)
{
    Q_UNUSED(name);
}

// 方案名横幅
void RuntimeWorkspace::setSolutionName(const QString& name)
{
    m_solBanner->setText(T("方案：") + name);
}

// 焦点面板计数
void RuntimeWorkspace::setCounts(int total, int pass, int fail, int cycleMs)
{
    StationRuntimePanel* p = activePanel();
    if (p) {
        p->setCounts(total, pass, fail, cycleMs);
    }
}

// 焦点面板 OK/NG
void RuntimeWorkspace::setLastOk(bool ok)
{
    StationRuntimePanel* p = activePanel();
    if (p) {
        p->setLastOk(ok);
    }
}

// 全部 Live
void RuntimeWorkspace::setLive(bool live)
{
    const QList<StationRuntimePanel*> all = m_panels.values();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i]) {
            all[i]->setLive(live);
        }
    }
}

// 焦点面板历史
void RuntimeWorkspace::addResult(bool ok, const QString& line, const QImage& thumb)
{
    StationRuntimePanel* p = activePanel();
    if (p) {
        p->addResult(ok, line, thumb);
    }
}

// 清空全部
void RuntimeWorkspace::resetHistory()
{
    const QList<StationRuntimePanel*> all = m_panels.values();
    for (int i = 0; i < all.size(); ++i) {
        if (all[i]) {
            all[i]->resetHistory();
        }
    }
}

// 焦点面板刷图
void RuntimeWorkspace::setRunImages(const HsCommData::HsImage& source,
                                    const HsCommData::HsImage& result,
                                    const QVector<HsCommData::Graphic>& overlay)
{
    StationRuntimePanel* p = activePanel();
    if (p) {
        p->setRunImages(source, result, overlay);
    }
}

// 权限
void RuntimeWorkspace::setControlPerms(bool canStartStop, bool canReset, bool canSwitch)
{
    m_btnStart->setEnabled(canStartStop);
    if (m_btnTrigger) { m_btnTrigger->setEnabled(canStartStop); }
    m_btnStop->setEnabled(canStartStop);
    m_btnReset->setEnabled(canReset);
    m_btnSwitch->setEnabled(canSwitch);
}

// 画面配置按钮
void RuntimeWorkspace::setConfigureEnabled(bool on)
{
    m_btnLayout->setEnabled(on);
}

// 开始按钮随循环状态变色：未运行普通底，运行中绿色
void RuntimeWorkspace::setLoopRunning(bool running)
{
    if (!m_btnStart) {
        return;
    }
    m_btnStart->setObjectName(running ? QLatin1String("rtStart") : QLatin1String("rtGhost"));
    m_btnStart->style()->unpolish(m_btnStart);
    m_btnStart->style()->polish(m_btnStart);
    m_btnStart->update();
}

// 按 Tabs/Tiles 重建各工站面板；有自定义页时插到页签最前
void RuntimeWorkspace::rebuildHost()
{
    m_panels.clear();
    m_uiHost = nullptr;
    while (QLayoutItem* it = m_hostLay->takeAt(0)) {
        if (it->widget()) {
            it->widget()->deleteLater();
        }
        if (it) {
            delete it;
        }
    }
    m_tabs = nullptr;
    m_tileHost = nullptr;
    m_tileLay = nullptr;

    const UiPage custom = firstCustomPage();
    const bool hasCustom = custom.enabled && !custom.widgets.empty();

    if (!m_sol || m_sol->stations.empty()) {
        if (hasCustom) {
            m_uiHost = new UiPageHost(m_host);
            m_uiHost->bindPage(custom, m_sol, stationMap());
            connect(m_uiHost, SIGNAL(runOnceRequested()), this, SIGNAL(customRunOnceRequested()));
            connect(m_uiHost, SIGNAL(loopRequested()), this, SIGNAL(customLoopRequested()));
            connect(m_uiHost, SIGNAL(stopRequested()), this, SIGNAL(customStopRequested()));
            connect(m_uiHost, SIGNAL(resetRequested()), this, SIGNAL(customResetRequested()));
            m_hostLay->addWidget(m_uiHost, 1);
        } else {
            QLabel* empty = new QLabel(T("方案里没有工站"), m_host);
            empty->setAlignment(Qt::AlignCenter);
            m_hostLay->addWidget(empty, 1);
        }
        m_side->setVisible(true);
        return;
    }

    const bool tiles = (m_sol->runtimeStationLayout == RuntimeStationsTiles);
    const int n = static_cast<int>(m_sol->stations.size());

    if (tiles && !hasCustom) {
        m_tileHost = new QWidget(m_host);
        m_tileLay = new QGridLayout(m_tileHost);
        m_tileLay->setContentsMargins(0, 0, 0, 0);
        m_tileLay->setSpacing(6);
        const int cols = n <= 1 ? 1 : (n <= 4 ? 2 : 3);
        for (int i = 0; i < n; ++i) {
            StationCfg* config = &m_sol->stations[static_cast<size_t>(i)];
            StationRuntimePanel* panel = new StationRuntimePanel(m_tileHost);
            Station* stInst = nullptr;
            if (m_previewSt && m_previewStation && config->id == m_previewSt->id) {
                stInst = m_previewStation;
            }
            Station* live = m_liveStations.value(qs(config->id), 0);
            if (live) {
                stInst = live;
            }
            panel->bindContext(m_sol, config, stInst);
            m_panels.insert(qs(config->id), panel);
            m_tileLay->addWidget(panel, i / cols, i % cols);
        }
        m_hostLay->addWidget(m_tileHost, 1);
    } else {
        m_tabs = new QTabWidget(m_host);
        int tabBase = 0;
        if (hasCustom) {
            m_uiHost = new UiPageHost(m_tabs);
            m_uiHost->bindPage(custom, m_sol, stationMap());
            connect(m_uiHost, SIGNAL(runOnceRequested()), this, SIGNAL(customRunOnceRequested()));
            connect(m_uiHost, SIGNAL(loopRequested()), this, SIGNAL(customLoopRequested()));
            connect(m_uiHost, SIGNAL(stopRequested()), this, SIGNAL(customStopRequested()));
            connect(m_uiHost, SIGNAL(resetRequested()), this, SIGNAL(customResetRequested()));
            const QString tabTitle = qs(custom.name).isEmpty() ? T("操作界面") : qs(custom.name);
            m_tabs->addTab(m_uiHost, tabTitle);
            tabBase = 1;
        }
        for (int i = 0; i < n; ++i) {
            StationCfg* config = &m_sol->stations[static_cast<size_t>(i)];
            StationRuntimePanel* panel = new StationRuntimePanel(m_tabs);
            Station* stInst = nullptr;
            if (m_previewSt && m_previewStation && config->id == m_previewSt->id) {
                stInst = m_previewStation;
            }
            Station* live = m_liveStations.value(qs(config->id), 0);
            if (live) {
                stInst = live;
            }
            panel->bindContext(m_sol, config, stInst);
            m_panels.insert(qs(config->id), panel);
            QString tabName = qs(config->name);
            if (!config->enabled) {
                tabName += T("（未使能）");
            }
            m_tabs->addTab(panel, tabName);
            if (m_previewSt && config->id == m_previewSt->id) {
                m_tabs->setCurrentIndex(tabBase + i);
            }
        }
        m_hostLay->addWidget(m_tabs, 1);
    }

    // 任一工站勾选「显示开始停止」则显示壳侧栏
    bool showSide = false;
    for (int i = 0; i < n; ++i) {
        if (m_sol->stations[static_cast<size_t>(i)].runtimeView.showSideButtons) {
            showSide = true;
            break;
        }
    }
    m_side->setVisible(showSide);
}

// 当前焦点面板
StationRuntimePanel* RuntimeWorkspace::activePanel() const
{
    if (m_tabs) {
        return qobject_cast<StationRuntimePanel*>(m_tabs->currentWidget());
    }
    if (!m_panels.isEmpty()) {
        return m_panels.first();
    }
    return 0;
}

// 语言热切换：刷运行壳按钮与方案条
void RuntimeWorkspace::retranslateUi()
{
    retranslateBound(this);
    if (m_sol) {
        setSolutionName(qs(m_sol->name));
    }
}
}


#include "moc_RuntimeWorkspace.cpp"
