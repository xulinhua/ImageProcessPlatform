/**
 * @file RuntimeViewDlg.cpp
 * @brief RuntimeViewDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RuntimeViewDlg.h"
#include "QtConv.h"
#include "Theme.h"
#include "UiText.h"
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace Vision {

namespace {

struct ToolPick {
    ObjectId id;     ///< 工具 id
    QString label;   ///< JobCfg 名 / 工具名
    ToolType type;   ///< 工具大类
    ToolPick() : type(ToolType::Acquire) {}
};

// 收集本工站工具，供底图与叠加筛选
std::vector<ToolPick> collectStationTools(const StationCfg* st)
{
    std::vector<ToolPick> out;
    if (!st) {
        return out;
    }
    for (int j = 0; j < static_cast<int>(st->jobs.size()); ++j) {
        const JobCfg& job = st->jobs[static_cast<size_t>(j)];
        for (int t = 0; t < static_cast<int>(job.tools.size()); ++t) {
            const ToolCfg& rec = job.tools[static_cast<size_t>(t)];
            ToolPick p;
            p.id = rec.id;
            p.label = qs(job.name) + QLatin1String(" / ") + qs(rec.name);
            p.type = rec.type;
            out.push_back(p);
        }
    }
    return out;
}

// 向量中是否已有该 id
bool containsId(const std::vector<ObjectId>& ids, const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
        if (ids[static_cast<size_t>(i)] == id) {
            return true;
        }
    }
    return false;
}

// 在列表中按 id 查找工具
const ToolPick* findPick(const std::vector<ToolPick>& tools, const ObjectId& id)
{
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        if (tools[static_cast<size_t>(i)].id == id) {
            return &tools[static_cast<size_t>(i)];
        }
    }
    return 0;
}

} // namespace

// 底图下拉：空项为工站本拍，其余仅本站采集与预处理
void RuntimePaneDlg::fillImageCombo()
{
    m_img->clear();
    m_img->addItem(T("工站结果（默认）"), QString());
    int sel = 0;
    const std::vector<ToolPick> tools = collectStationTools(m_station);
    bool found = m_pane.imageToolId.isNull();
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        const ToolPick& p = tools[static_cast<size_t>(i)];
        if (!toolTypeIsImageSource(p.type)) {
            continue;
        }
        m_img->addItem(p.label, qs(p.id));
        if (!m_pane.imageToolId.isNull() && p.id == m_pane.imageToolId) {
            sel = m_img->count() - 1;
            found = true;
        }
    }
    if (!found && !m_pane.imageToolId.isNull()) {
        const ToolPick* extra = findPick(tools, m_pane.imageToolId);
        const QString label = extra ? extra->label : (qs(m_pane.imageToolId) + T("（非本站）"));
        m_img->addItem(label, qs(m_pane.imageToolId));
        sel = m_img->count() - 1;
    }
    m_img->setCurrentIndex(sel);
}

// 叠加列表：仅本站工具
void RuntimePaneDlg::fillOverlayList()
{
    m_ovList->clear();
    QListWidgetItem* station = new QListWidgetItem(T("工站结果图形"));
    station->setFlags(station->flags() | Qt::ItemIsUserCheckable);
    station->setData(Qt::UserRole, QString());
    station->setCheckState(containsId(m_pane.overlayToolIds, ObjectId())
        ? Qt::Checked
        : Qt::Unchecked);
    m_ovList->addItem(station);

    const std::vector<ToolPick> tools = collectStationTools(m_station);
    std::vector<ObjectId> listed;
    listed.push_back(ObjectId());
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        const ToolPick& p = tools[static_cast<size_t>(i)];
        if (!toolTypeIsOverlaySource(p.type)) {
            continue;
        }
        QListWidgetItem* it = new QListWidgetItem(p.label);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setData(Qt::UserRole, qs(p.id));
        it->setCheckState(containsId(m_pane.overlayToolIds, p.id) ? Qt::Checked : Qt::Unchecked);
        m_ovList->addItem(it);
        listed.push_back(p.id);
    }
    for (int i = 0; i < static_cast<int>(m_pane.overlayToolIds.size()); ++i) {
        const ObjectId id = m_pane.overlayToolIds[static_cast<size_t>(i)];
        if (id.isNull() || containsId(listed, id)) {
            continue;
        }
        const ToolPick* extra = findPick(tools, id);
        QListWidgetItem* it = new QListWidgetItem(
            extra ? extra->label : (qs(id) + T("（非本站）")));
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setData(Qt::UserRole, qs(id));
        it->setCheckState(Qt::Checked);
        m_ovList->addItem(it);
        listed.push_back(id);
    }
}

// 单窗口：一张底图 + 多路叠加勾选
RuntimePaneDlg::RuntimePaneDlg(Solution* sol, StationCfg* st, int index,
                                     const RuntimePane& pane, QWidget* parent)
    : QDialog(parent)
    , m_sol(sol)
    , m_station(st)
    , m_pane(pane)
{
    Q_UNUSED(m_sol);
    setWindowTitle(T("窗口配置") + QLatin1String(" - ") + T("窗口") + QString::number(index + 1));
    setModal(true);
    resize(420, 480);

    m_img = new QComboBox(this);
    fillImageCombo();

    m_showOv = new QCheckBox(T("显示叠加"), this);
    m_showOv->setChecked(m_pane.showOverlay);

    QLabel* ovHint = new QLabel(T("勾选要叠到该底图上的结果图形，可多选（仅本工站工具）。"), this);
    ovHint->setWordWrap(true);
    ovHint->setObjectName(QLatin1String("hint"));

    m_ovList = new QListWidget(this);
    m_ovList->setSelectionMode(QAbstractItemView::NoSelection);
    fillOverlayList();

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    QFormLayout* form = new QFormLayout();
    form->addRow(T("底图"), m_img);
    form->addRow(m_showOv);

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(form);
    lay->addWidget(new QLabel(T("叠加工具"), this));
    lay->addWidget(ovHint);
    lay->addWidget(m_ovList, 1);
    lay->addWidget(box);
}

// 读回底图与勾选的叠加
RuntimePane RuntimePaneDlg::pane() const
{
    RuntimePane p = m_pane;
    p.imageToolId = oid(m_img->currentData().toString());
    p.showOverlay = m_showOv->isChecked();
    p.overlayToolIds.clear();
    for (int i = 0; i < m_ovList->count(); ++i) {
        QListWidgetItem* it = m_ovList->item(i);
        if (!it || it->checkState() != Qt::Checked) {
            continue;
        }
        p.overlayToolIds.push_back(oid(it->data(Qt::UserRole).toString()));
    }
    return p;
}

// 一行摘要：底图名 + 叠加工具名
static QString paneSummary(const RuntimePane& pane, const Solution* sol)
{
    QString img = T("工站结果");
    if (!pane.imageToolId.isNull() && sol) {
        const ToolCfg* t = sol->findTool(pane.imageToolId);
        img = t ? qs(t->name) : qs(pane.imageToolId);
    }
    QString ov;
    if (!pane.showOverlay) {
        ov = T("叠加关闭");
    } else if (pane.overlayToolIds.empty()) {
        ov = T("无叠加");
    } else {
        for (int i = 0; i < static_cast<int>(pane.overlayToolIds.size()); ++i) {
            if (!ov.isEmpty()) {
                ov += QLatin1String(", ");
            }
            const ObjectId id = pane.overlayToolIds[static_cast<size_t>(i)];
            if (id.isNull()) {
                ov += T("工站结果");
                continue;
            }
            const ToolCfg* t = sol ? sol->findTool(id) : nullptr;
            ov += t ? qs(t->name) : qs(id);
        }
    }
    return T("底图") + QLatin1String(": ") + img + QLatin1String("    ")
        + T("叠加") + QLatin1String(": ") + ov;
}

// 画面配置：方案级多工站布局 + 当前工站窗格
RuntimeViewDlg::RuntimeViewDlg(Solution* sol, StationCfg* st, QWidget* parent)
    : QDialog(parent)
    , m_sol(sol)
    , m_station(st)
{
    setWindowTitle(T("画面配置"));
    setModal(true);
    resize(760, 580);

    m_shellLayout = new QComboBox(this);
    // Title 返回 UTF-8 源字符串
    m_shellLayout->addItem(QString::fromUtf8(runtimeStationLayoutTitle(RuntimeStationsTabs).c_str()),
                           static_cast<int>(RuntimeStationsTabs));
    m_shellLayout->addItem(QString::fromUtf8(runtimeStationLayoutTitle(RuntimeStationsTiles).c_str()),
                           static_cast<int>(RuntimeStationsTiles));
    if (sol) {
        m_shellLayout->setCurrentIndex(sol->runtimeStationLayout == RuntimeStationsTiles ? 1 : 0);
    }

    m_stationCombo = new QComboBox(this);
    if (sol) {
        for (int i = 0; i < static_cast<int>(sol->stations.size()); ++i) {
            const StationCfg& s = sol->stations[static_cast<size_t>(i)];
            m_stationCombo->addItem(qs(s.name), qs(s.id));
            if (st && s.id == st->id) {
                m_stationCombo->setCurrentIndex(i);
            }
        }
    }
    connect(m_stationCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onStationChanged()));

    m_rowCombo = new QComboBox(this);
    m_colCombo = new QComboBox(this);
    for (int n = 1; n <= 3; ++n) {
        m_rowCombo->addItem(QString::number(n), n);
        m_colCombo->addItem(QString::number(n), n);
    }
    connect(m_rowCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onLayoutChanged()));
    connect(m_colCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onLayoutChanged()));

    QWidget* gridPick = new QWidget(this);
    QHBoxLayout* gridLay = new QHBoxLayout(gridPick);
    gridLay->setContentsMargins(0, 0, 0, 0);
    gridLay->addWidget(new QLabel(T("行数"), gridPick));
    gridLay->addWidget(m_rowCombo);
    gridLay->addSpacing(16);
    gridLay->addWidget(new QLabel(T("列数"), gridPick));
    gridLay->addWidget(m_colCombo);
    gridLay->addStretch();

    m_stats = new QCheckBox(T("显示产量统计"), this);
    m_hist = new QCheckBox(T("显示结果历史"), this);
    m_side = new QCheckBox(T("显示开始停止"), this);

    QWidget* flags = new QWidget(this);
    QFormLayout* form = new QFormLayout(flags);
    form->setContentsMargins(0, 0, 0, 0);
    form->addRow(T("多工站布局"), m_shellLayout);
    form->addRow(T("编辑工站"), m_stationCombo);
    form->addRow(T("图像窗口布局"), gridPick);
    form->addRow(m_side);
    form->addRow(m_stats);
    form->addRow(m_hist);

    QLabel* hint = new QLabel(
        T("多工站布局为方案级：分页签一次看一个工站，一屏拼贴同时显示。下方窗格仅对当前编辑工站生效；底图/叠加只列本站工具。"),
        this);
    hint->setWordWrap(true);
    hint->setObjectName(QLatin1String("hint"));

    QVBoxLayout* paneLay = new QVBoxLayout();
    paneLay->setSpacing(6);
    for (int i = 0; i < 9; ++i) {
        m_panes[i] = RuntimePane();
        m_lines[i] = new QWidget(this);
        QHBoxLayout* row = new QHBoxLayout(m_lines[i]);
        row->setContentsMargins(0, 0, 0, 0);
        m_en[i] = new QCheckBox(m_lines[i]);
        m_en[i]->setToolTip(T("使能"));
        QLabel* idx = new QLabel(T("窗口") + QString::number(i + 1), m_lines[i]);
        idx->setMinimumWidth(48);
        m_summary[i] = new QLabel(m_lines[i]);
        m_summary[i]->setObjectName(QLatin1String("hint"));
        m_config[i] = new QPushButton(T("配置…"), m_lines[i]);
        m_config[i]->setProperty("pane", i);
        connect(m_config[i], SIGNAL(clicked()), this, SLOT(onConfigPane()));
        row->addWidget(m_en[i]);
        row->addWidget(idx);
        row->addWidget(m_summary[i], 1);
        row->addWidget(m_config[i]);
        paneLay->addWidget(m_lines[i]);
    }
    paneLay->addStretch();

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, SIGNAL(accepted()), this, SLOT(onSave()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addWidget(flags);
    lay->addWidget(hint);
    lay->addLayout(paneLay, 1);
    lay->addWidget(box);

    loadStationView();
}

// 两个窗格是否相同
static bool runtimePaneSame(const RuntimePane& a, const RuntimePane& b)
{
    if (a.imageToolId != b.imageToolId || a.showOverlay != b.showOverlay || a.enabled != b.enabled) {
        return false;
    }
    if (a.overlayToolIds.size() != b.overlayToolIds.size()) {
        return false;
    }
    for (size_t i = 0; i < a.overlayToolIds.size(); ++i) {
        if (a.overlayToolIds[i] != b.overlayToolIds[i]) {
            return false;
        }
    }
    return true;
}

// 两份运行画面是否相同
static bool runtimeViewSame(const RuntimeView& a, const RuntimeView& b)
{
    if (a.rows != b.rows || a.cols != b.cols || a.showStats != b.showStats
        || a.showHistory != b.showHistory || a.showSideButtons != b.showSideButtons
        || a.panes.size() != b.panes.size()) {
        return false;
    }
    for (size_t i = 0; i < a.panes.size(); ++i) {
        if (!runtimePaneSame(a.panes[i], b.panes[i])) {
            return false;
        }
    }
    return true;
}
// 把当前 UI 写回工站
void RuntimeViewDlg::flushCurrentStation()
{
    if (!m_station) {
        return;
    }
    RuntimeView v;
    v.rows = m_rowCombo->currentData().toInt();
    v.cols = m_colCombo->currentData().toInt();
    v.showStats = m_stats->isChecked();
    v.showHistory = m_hist->isChecked();
    v.showSideButtons = m_side->isChecked();
    for (int i = 0; i < 9; ++i) {
        RuntimePane p = m_panes[i];
        p.enabled = m_en[i]->isChecked();
        v.panes.push_back(p);
    }
    v.normalize();
    RuntimeView cur = m_station->runtimeView;
    cur.normalize();
    if (runtimeViewSame(cur, v)) {
        return;
    }
    m_station->runtimeView = v;
}

// 从当前工站加载窗格 UI
void RuntimeViewDlg::loadStationView()
{
    RuntimeView view;
    if (m_station) {
        view = m_station->runtimeView;
    }
    view.normalize();
    m_rowCombo->blockSignals(true);
    m_colCombo->blockSignals(true);
    m_rowCombo->setCurrentIndex(view.rows - 1);
    m_colCombo->setCurrentIndex(view.cols - 1);
    m_rowCombo->blockSignals(false);
    m_colCombo->blockSignals(false);
    m_stats->setChecked(view.showStats);
    m_hist->setChecked(view.showHistory);
    m_side->setChecked(view.showSideButtons);
    for (int i = 0; i < 9; ++i) {
        m_panes[i] = view.panes[static_cast<size_t>(i)];
        m_en[i]->setChecked(m_panes[i].enabled);
        refreshSummary(i);
    }
    applyRowVisible();
}

// 切换编辑工站
void RuntimeViewDlg::onStationChanged()
{
    flushCurrentStation();
    m_station = 0;
    if (m_sol) {
        m_station = m_sol->findStationConfig(oid(m_stationCombo->currentData().toString()));
    }
    loadStationView();
}

// 刷新一行摘要
void RuntimeViewDlg::refreshSummary(int index)
{
    if (index < 0 || index > 8) {
        return;
    }
    m_summary[index]->setText(paneSummary(m_panes[index], m_sol));
}

// 行列变了只留用得到的窗格行
void RuntimeViewDlg::onLayoutChanged()
{
    applyRowVisible();
}

// 打开该行的窗口配置
void RuntimeViewDlg::onConfigPane()
{
    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
        return;
    }
    const int i = btn->property("pane").toInt();
    if (i < 0 || i > 8) {
        return;
    }
    m_panes[i].enabled = m_en[i]->isChecked();
    RuntimePaneDlg dlg(m_sol, m_station, i, m_panes[i], this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const bool en = m_en[i]->isChecked();
    m_panes[i] = dlg.pane();
    m_panes[i].enabled = en;
    refreshSummary(i);
}

// 按当前行列显隐 1..9 行
void RuntimeViewDlg::applyRowVisible()
{
    int rows = m_rowCombo->currentData().toInt();
    int cols = m_colCombo->currentData().toInt();
    if (rows < 1) {
        rows = 1;
    }
    if (rows > 3) {
        rows = 3;
    }
    if (cols < 1) {
        cols = 1;
    }
    if (cols > 3) {
        cols = 3;
    }
    const int n = rows * cols;
    for (int i = 0; i < 9; ++i) {
        m_lines[i]->setVisible(i < n);
    }
}

// 写回方案布局与当前工站
void RuntimeViewDlg::onSave()
{
    if (m_sol) {
        m_sol->runtimeStationLayout =
            runtimeStationLayoutFromInt(m_shellLayout->currentData().toInt());
    }
    flushCurrentStation();
    if (!m_station) {
        reject();
        return;
    }
    accept();
}

}

#include "moc_RuntimeViewDlg.cpp"
