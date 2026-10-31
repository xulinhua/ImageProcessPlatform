/**
 * @file StationRuntimePanel.cpp
 * @brief StationRuntimePanel实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StationRuntimePanel.h"
#include "Station.h"
#include "Job.h"
#include "QtConv.h"
#include "Theme.h"
#include "UiText.h"
#include "CloudViewBinder.h"
#include "ToolIo.h"
#include <QButtonGroup>
#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace Vision {

namespace {
// 运行格绑定工具的未使能原因；空串表示正常
QString paneDisableReason(Solution* sol, StationCfg* st, const ObjectId& toolId)
{
    if (!st) {
        return QString();
    }
    if (!st->enabled) {
        return T("工站未使能");
    }
    if (toolId.isNull()) {
        return QString();
    }
    if (!sol) {
        return QString();
    }
    const ToolCfg* t = sol->findTool(toolId);
    if (!t) {
        return QString();
    }
    JobCfg* job = nullptr;
    for (size_t i = 0; i < st->jobs.size(); ++i) {
        if (st->jobs[i].findTool(toolId)) {
            job = &st->jobs[i];
            break;
        }
    }
    if (job && !job->enabled) {
        return T("流程未使能");
    }
    if (!t->enabled) {
        return T("工具未使能");
    }
    return QString();
}
} // namespace


// 统计卡片中的一行：名称 + 右侧数值标签
static QLabel* metricRow(QWidget* parent, const QString& name, QLabel** valueOut)
{
    QWidget* row = new QWidget(parent);
    QHBoxLayout* h = new QHBoxLayout(row);
    h->setContentsMargins(8, 6, 8, 6);
    QLabel* k = new QLabel(name, row);
    k->setObjectName(QLatin1String("hint"));
    QLabel* v = new QLabel(QLatin1String("-"), row);
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    h->addWidget(k);
    h->addWidget(v, 1);
    QVBoxLayout* parentLay = qobject_cast<QVBoxLayout*>(parent->layout());
    if (parentLay) {
        parentLay->addWidget(row);
    }
    *valueOut = v;
    return v;
}

// 单工站：图像网格、产量统计与结果历史
StationRuntimePanel::StationRuntimePanel(QWidget* parent)
    : QWidget(parent)
    , m_gridLay(nullptr)
    , m_statsCard(nullptr)
    , m_histBlock(nullptr)
    , m_stationTitle(nullptr)
    , m_histFilter(0)
    , m_needFit(true)
    , m_sol(nullptr)
    , m_stationPtr(nullptr)
    , m_station(nullptr)
{
    for (int i = 0; i < 9; ++i) {
        m_shells[i] = 0;
        m_cells[i] = 0;
        m_cellTitles[i] = 0;
        m_cellPanes[i] = 0;
    }
    m_viewConfig.normalize();

    // 图像区：状态指示 + 3×3 预建的图像窗，实际显示几个由 applyLayout 决定
    m_online = new QLabel(T("● 在线"), this);
    m_online->setStyleSheet(QLatin1String("color:#52C41A;"));
    m_live = new QLabel(T("● Live"), this);
    m_live->setStyleSheet(QLatin1String("color:#888888;"));
    QLabel* trig = new QLabel(T("● 软触发"), this);
    trig->setStyleSheet(QLatin1String("color:#1890FF;"));

    QHBoxLayout* imgHead = new QHBoxLayout();
    imgHead->addStretch();
    imgHead->addWidget(m_online);
    imgHead->addWidget(m_live);
    imgHead->addWidget(trig);

    QWidget* gridHost = new QWidget(this);
    m_gridLay = new QGridLayout(gridHost);
    m_gridLay->setContentsMargins(0, 0, 0, 0);
    m_gridLay->setSpacing(6);
    for (int i = 0; i < 9; ++i) {
        QWidget* pane = new QWidget(gridHost);
        QVBoxLayout* v = new QVBoxLayout(pane);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        QLabel* title = new QLabel(T("窗口") + QString::number(i + 1), pane);
        title->setObjectName(QLatin1String("hint"));
        HsCtrl::HsViewShell* shell = new HsCtrl::HsViewShell(pane);
        HsCtrl::HsImageWindow* w = shell->imageWindow();
        w->setWindowMode(HsCtrl::WindowRuntime);
        // 运行态不显示叠加层工具条，避免窗右侧一列空白方块
        w->setOverlayToolbarVisible(false);
        w->setDrawTool(HsCtrl::ToolBrowse);
        v->addWidget(title);
        v->addWidget(shell, 1);
        m_cellPanes[i] = pane;
        m_cellTitles[i] = title;
        m_shells[i] = shell;
        m_cells[i] = w;
        m_gridLay->addWidget(pane, i / 3, i % 3);
    }

    QFrame* imgCard = makeCard(this);
    QVBoxLayout* imgLay = new QVBoxLayout(imgCard);
    imgLay->addLayout(imgHead);
    imgLay->addWidget(gridHost, 1);

    // 右侧统计卡：判定横幅与产量、良率、节拍
    m_stationTitle = new QLabel(T("工站：-"), this);
    m_stationTitle->setObjectName(QLatin1String("section"));
    m_banner = new QLabel(QLatin1String("OK"), this);
    m_banner->setObjectName(QLatin1String("bannerOk"));
    m_banner->setAlignment(Qt::AlignCenter);
    m_banner->setMinimumHeight(72);

    QFrame* stats = makeCard(this);
    stats->setFixedWidth(220);
    QVBoxLayout* sLay = new QVBoxLayout(stats);
    sLay->addWidget(m_stationTitle);
    sLay->addWidget(m_banner);
    metricRow(stats, T("产量"), &m_total);
    metricRow(stats, T("合格"), &m_pass);
    metricRow(stats, T("不合格"), &m_fail);
    metricRow(stats, T("良率"), &m_yield);
    metricRow(stats, T("节拍"), &m_cycle);
    m_fail->setStyleSheet(QLatin1String("color:#FF4D4F; font-weight:bold;"));
    m_yield->setStyleSheet(QLatin1String("color:#52C41A; font-weight:bold;"));
    sLay->addStretch();
    m_statsCard = stats;

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(imgCard, 1);
    top->addWidget(stats);

    // 底部结果历史：全部 / OK / NG 互斥过滤 + 横向滚动的缩略图行
    QLabel* histTitle = new QLabel(T("结果历史"), this);
    histTitle->setObjectName(QLatin1String("section"));
    m_fAll = new QPushButton(T("全部"), this);
    m_fOk = new QPushButton(T("OK"), this);
    m_fNg = new QPushButton(T("NG"), this);
    m_fAll->setCheckable(true);
    m_fOk->setCheckable(true);
    m_fNg->setCheckable(true);
    m_fAll->setChecked(true);
    QButtonGroup* fg = new QButtonGroup(this);
    fg->setExclusive(true);
    fg->addButton(m_fAll, 0);
    fg->addButton(m_fOk, 1);
    fg->addButton(m_fNg, 2);
    connect(fg, SIGNAL(buttonClicked(int)), this, SLOT(onHistFilter()));
    QHBoxLayout* histHead = new QHBoxLayout();
    histHead->addWidget(histTitle);
    histHead->addStretch();
    histHead->addWidget(m_fAll);
    histHead->addWidget(m_fOk);
    histHead->addWidget(m_fNg);
    m_ngRow = new QWidget(this);
    m_ngLay = new QHBoxLayout(m_ngRow);
    m_ngLay->setContentsMargins(4, 4, 4, 4);
    m_ngLay->addStretch();
    QScrollArea* ngScroll = new QScrollArea(this);
    ngScroll->setWidgetResizable(true);
    ngScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ngScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ngScroll->setFixedHeight(110);
    ngScroll->setWidget(m_ngRow);

    m_histBlock = new QWidget(this);
    QVBoxLayout* histLay = new QVBoxLayout(m_histBlock);
    histLay->setContentsMargins(0, 0, 0, 0);
    histLay->setSpacing(4);
    histLay->addLayout(histHead);
    histLay->addWidget(ngScroll);

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->addLayout(top, 1);
    lay->addWidget(m_histBlock);

    applyLayout();
}

// 当前绑定工站 id
ObjectId StationRuntimePanel::stationId() const
{
    return m_stationPtr ? m_stationPtr->id : ObjectId();
}

// 当前布局里第一个已使能格的图像窗
HsCtrl::HsImageWindow* StationRuntimePanel::imageWindow() const
{
    const int n = m_viewConfig.slotCount();
    for (int i = 0; i < n && i < 9; ++i) {
        if (m_viewConfig.panes[static_cast<size_t>(i)].enabled && m_cells[i]) {
            return m_cells[i];
        }
    }
    return m_cells[0];
}

// 记下方案/工站配置/运行实例并按配方刷新画面
void StationRuntimePanel::bindContext(Solution* sol, StationCfg* st, Station* station)
{
    m_sol = sol;
    m_stationPtr = st;
    m_station = station;
    if (st) {
        st->runtimeView.normalize();
        m_viewConfig = st->runtimeView;
        QString stTitle = T("工站：") + qs(st->name);
        if (!st->enabled) {
            stTitle += T("（未使能）");
        }
        m_stationTitle->setText(stTitle);
        m_stationTitle->setStyleSheet(st->enabled ? QString() : QLatin1String("color:#A0A0A0;"));
    } else {
        m_viewConfig = RuntimeView();
        m_viewConfig.normalize();
        m_stationTitle->setText(T("工站：-"));
        m_stationTitle->setStyleSheet(QString());
    }
    applyLayout();
    applyImages();
}

// 只换运行实例指针，保留上一拍画面与产量显示
void StationRuntimePanel::setRuntimeStation(Station* station)
{
    m_station = station;
}

// 刷新产量、良率与节拍
void StationRuntimePanel::setCounts(int total, int pass, int fail, int cycleMs)
{
    m_total->setText(QString::number(total));
    m_pass->setText(QString::number(pass));
    m_fail->setText(QString::number(fail));
    const double y = total > 0 ? (100.0 * pass / total) : 0.0;
    m_yield->setText(QString::number(y, 'f', 2) + QLatin1Char('%'));
    m_cycle->setText(QString::number(cycleMs) + QLatin1String(" ms"));
}

// 切换 OK/NG 横幅样式
void StationRuntimePanel::setLastOk(bool ok)
{
    m_banner->setText(ok ? QLatin1String("OK") : QLatin1String("NG"));
    m_banner->setObjectName(ok ? QLatin1String("bannerOk") : QLatin1String("bannerNg"));
    m_banner->style()->unpolish(m_banner);
    m_banner->style()->polish(m_banner);
}

// Live 指示灯颜色
void StationRuntimePanel::setLive(bool live)
{
    m_live->setStyleSheet(live ? QLatin1String("color:#1890FF;") : QLatin1String("color:#888888;"));
}

// 追加一条结果历史（最多保留 40 条）
void StationRuntimePanel::addResult(bool ok, const QString& line, const QImage& thumb)
{
    HistItem it;
    it.ok = ok;
    it.tip = line;
    it.time = QDateTime::currentDateTime().toString(QLatin1String("hh:mm:ss"));
    it.thumb = thumb;
    // 异步缩略图为空时用本拍结果/源图补，避免历史只剩纯色 OK/NG 块
    if (it.thumb.isNull()) {
        if (!m_lastRst.isNull()) {
            it.thumb = toQImage(m_lastRst);
        } else if (!m_lastSrc.isNull()) {
            it.thumb = toQImage(m_lastSrc);
        }
    }
    m_hist.prepend(it);
    while (m_hist.size() > 40) {
        m_hist.removeLast();
    }
    rebuildHist();
}

// 清空历史并复位计数（不清图像；停止后画面仍保留）
void StationRuntimePanel::resetHistory()
{
    m_hist.clear();
    rebuildHist();
    setCounts(0, 0, 0, 0);
    setLastOk(true);
}

// 缓存本拍汇总图并刷新各格
void StationRuntimePanel::setRunImages(const HsCommData::HsImage& source,
                                       const HsCommData::HsImage& result,
                                       const QVector<HsCommData::Graphic>& overlay)
{
    m_lastSrc = source;
    m_lastRst = result;
    m_lastGfx = overlay;
    applyImages();
}

// 按行列重排窗格；未使能的格子留空占位
void StationRuntimePanel::applyLayout()
{
    m_viewConfig.normalize();
    const int rows = m_viewConfig.rows;
    const int cols = m_viewConfig.cols;
    const int n = m_viewConfig.slotCount();
    for (int i = 0; i < 9; ++i) {
        m_gridLay->removeWidget(m_cellPanes[i]);
        m_cellPanes[i]->hide();
    }
    for (int r = 0; r < 3; ++r) {
        m_gridLay->setRowStretch(r, 0);
    }
    for (int c = 0; c < 3; ++c) {
        m_gridLay->setColumnStretch(c, 0);
    }
    for (int i = 0; i < n; ++i) {
        const int r = i / cols;
        const int c = i % cols;
        m_gridLay->addWidget(m_cellPanes[i], r, c);
        m_cellPanes[i]->show();
        const bool on = m_viewConfig.panes[static_cast<size_t>(i)].enabled;
        m_cellTitles[i]->setVisible(on);
        m_cells[i]->setVisible(on);
        m_gridLay->setRowStretch(r, 1);
        m_gridLay->setColumnStretch(c, 1);
    }
    Q_UNUSED(rows);
    m_statsCard->setVisible(m_viewConfig.showStats);
    m_histBlock->setVisible(m_viewConfig.showHistory);
    m_needFit = true;
}

// 工具显示名；空 id 表示用工站本拍
QString StationRuntimePanel::toolCaption(const ObjectId& id) const
{
    if (id.isNull() || !m_sol) {
        return T("工站结果");
    }
    const ToolCfg* t = m_sol->findTool(id);
    return t ? qs(t->name) : T("工站结果");
}

// 每格：指定底图只用来源缓存；叠加必须与底图同源，避免窗2增强图上错位工站圆
void StationRuntimePanel::applyImages()
{
    m_viewConfig.normalize();
    const int n = m_viewConfig.slotCount();
    for (int i = 0; i < n; ++i) {
        const RuntimePane& pane = m_viewConfig.panes[static_cast<size_t>(i)];
        if (!pane.enabled) {
            continue;
        }
        HsCommData::HsImage img;
        QVector<HsCommData::Graphic> gfx;
        bool hasImage = false;

        if (pane.imageToolId.isNull()) {
            img = m_lastRst;
            hasImage = !img.isNull();
        } else if (m_station) {
            Job* job = m_station->findJobOfTool(pane.imageToolId);
            if (job) {
                const HsCommData::HsImage cached = job->cachedImage(pane.imageToolId);
                if (!cached.isNull()) {
                    img = cached;
                    hasImage = true;
                }
            }
        }

        if (pane.showOverlay) {
            auto appendToolOverlay = [&](const ObjectId& toolId) {
                if (!m_station || toolId.isNull()) {
                    return;
                }
                Job* job = m_station->findJobOfTool(toolId);
                if (job) {
                    gfx += qg(job->cachedOverlay(toolId));
                }
            };
            if (pane.overlayToolIds.empty()) {
                // 未配置：底图是工站结果用本拍叠加，否则用该工具自身叠加
                if (pane.imageToolId.isNull()) {
                    gfx += m_lastGfx;
                } else {
                    appendToolOverlay(pane.imageToolId);
                }
            } else {
                for (int k = 0; k < static_cast<int>(pane.overlayToolIds.size()); ++k) {
                    const ObjectId ovId = pane.overlayToolIds[static_cast<size_t>(k)];
                    if (ovId.isNull()) {
                        // 「工站本拍」叠加只画在工站结果底图上；画在其它工具图上会错位
                        if (pane.imageToolId.isNull()) {
                            gfx += m_lastGfx;
                        } else {
                            appendToolOverlay(pane.imageToolId);
                        }
                    } else {
                        appendToolOverlay(ovId);
                    }
                }
            }
        }

        QString cap = T("窗口") + QString::number(i + 1) + QLatin1String("  ") + toolCaption(pane.imageToolId);
        if (pane.showOverlay && !pane.overlayToolIds.empty()) {
            QString ov;
            for (int k = 0; k < static_cast<int>(pane.overlayToolIds.size()); ++k) {
                const ObjectId ovId = pane.overlayToolIds[static_cast<size_t>(k)];
                if (ovId.isNull() || ovId == pane.imageToolId) {
                    continue;
                }
                if (!ov.isEmpty()) {
                    ov += QLatin1String(", ");
                }
                ov += toolCaption(ovId);
            }
            if (!ov.isEmpty()) {
                cap += QLatin1String(" / ") + ov;
            }
        }
        QString disReason = paneDisableReason(m_sol, m_stationPtr, pane.imageToolId);
        if (!disReason.isEmpty()) {
            cap += QLatin1String("  [") + disReason + QLatin1Char(']');
            m_cellTitles[i]->setStyleSheet(QLatin1String("color:#A0A0A0;"));
        } else {
            m_cellTitles[i]->setStyleSheet(QString());
        }
        m_cellTitles[i]->setText(cap);

        if (!hasImage) {
            // 无新图时保留窗口上一拍（停止/再开尚未出图时不清空）
            continue;
        }
        ObjectId cam;
        if (m_sol && !pane.imageToolId.isNull()) {
            const ToolCfg* camTool = m_sol->findTool(pane.imageToolId);
            if (camTool && camTool->type == ToolType::Acquire && !camTool->deviceId.isNull()) {
                cam = camTool->deviceId;
            }
        }
        m_cells[i]->setImageCalib(imageCalibForCamera(cam));
        applyFrame(m_cells[i], img);
        // 叠加帧号与底图一致（applyFrame 已写入 HsImage::frameId）
        const int64_t fid = (img.frameId != 0) ? img.frameId : m_cells[i]->currentFrameId();
        m_cells[i]->setOverlay(toStd(gfx), fid);
        if (m_needFit) {
            m_cells[i]->fitToWindow();
        }
        // 格绑定工具若有点云输出，灌入该格壳并可切 3D
        if (m_shells[i] && m_station && !pane.imageToolId.isNull()) {
            Job* job = m_station->findJobOfTool(pane.imageToolId);
            if (job) {
                const bool got3d = CloudViewBinder::apply(m_shells[i], job->cachedIo(pane.imageToolId), true);
                if (!got3d) {
                    m_shells[i]->setViewMode(HsCtrl::ViewModeImage2D);
                }
            }
        }
    }
    m_needFit = false;
}

// 全部/OK/NG 过滤后重建历史条
void StationRuntimePanel::onHistFilter()
{
    if (m_fOk->isChecked()) {
        m_histFilter = 1;
    } else if (m_fNg->isChecked()) {
        m_histFilter = 2;
    } else {
        m_histFilter = 0;
    }
    rebuildHist();
}

// 按过滤条件重建最多 12 张历史缩略图
void StationRuntimePanel::rebuildHist()
{
    // 清掉旧缩略图，保留末尾的弹簧
    while (m_ngLay->count() > 1) {
        QLayoutItem* it = m_ngLay->takeAt(0);
        if (!it) {
            break;
        }
        if (it->widget()) {
            it->widget()->deleteLater();
        }
        delete it;
        it = nullptr;
    }
    int shown = 0;
    for (int i = 0; i < m_hist.size() && shown < 12; ++i) {
        const HistItem& rec = m_hist[i];
        if (m_histFilter == 1 && !rec.ok) {
            continue;
        }
        if (m_histFilter == 2 && rec.ok) {
            continue;
        }
        // 每格：缩略图（无图时显示 OK/NG 色块）、判定徽标、时间，悬停显示详情
        QFrame* cell = makeCard(m_ngRow);
        cell->setFixedSize(128, 96);
        QVBoxLayout* v = new QVBoxLayout(cell);
        v->setContentsMargins(4, 4, 4, 4);
        QLabel* pic = new QLabel(cell);
        pic->setAlignment(Qt::AlignCenter);
        pic->setMinimumHeight(52);
        if (!rec.thumb.isNull()) {
            pic->setPixmap(QPixmap::fromImage(rec.thumb).scaled(110, 52, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            // 仍无图时画小色块，避免大块纯色 OK 字占满历史条
            QPixmap ph(110, 52);
            ph.fill(rec.ok ? QColor(0xF6, 0xFF, 0xED) : QColor(0xFF, 0xF1, 0xF0));
            pic->setPixmap(ph);
            pic->setToolTip(rec.ok ? QLatin1String("OK") : QLatin1String("NG"));
        }
        QLabel* badge = new QLabel(rec.ok ? QLatin1String("OK") : QLatin1String("NG"), cell);
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(rec.ok
            ? QLatin1String("background:#52C41A; color:white; border-radius:2px;")
            : QLatin1String("background:#FF4D4F; color:white; border-radius:2px;"));
        QLabel* tm = new QLabel(rec.time, cell);
        tm->setObjectName(QLatin1String("hint"));
        tm->setAlignment(Qt::AlignCenter);
        tm->setToolTip(rec.tip);
        v->addWidget(pic, 1);
        v->addWidget(badge);
        v->addWidget(tm);
        m_ngLay->insertWidget(shown, cell);
        ++shown;
    }
}

}

#include "moc_StationRuntimePanel.cpp"
