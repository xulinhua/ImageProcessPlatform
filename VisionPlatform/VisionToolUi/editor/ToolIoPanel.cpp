/**
 * @file ToolIoPanel.cpp
 * @brief ToolIoPanel实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolIoPanel.h"
#include "ParamKeys.h"
#include "Job.h"
#include "QtConv.h"
#include "RoiConv.h"
#include "ToolBase.h"
#include "UiText.h"
#include <HsCtrl/HsImageWindow.h>
#include <HsCommData/Graphic.h>
#include <HsCommData/HsInterface2D.h>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QColor>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPointF>
#include <QPushButton>
#include <QTimer>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QSizePolicy>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <cmath>

namespace Vision {

namespace {

// 按可见行重算输入页固定高度，避免行距被卡片撑开
static void syncLinkPageHeight(QWidget* page)
{
    if (!page) {
        return;
    }
    QVBoxLayout* lay = qobject_cast<QVBoxLayout*>(page->layout());
    if (!lay) {
        return;
    }
    lay->activate();
    const int h = lay->totalSizeHint().height();
    if (h > 0) {
        page->setFixedHeight(h);
    }
}

enum LinkComboRole {
    RoleFromId = Qt::UserRole,     ///< 上游工具 id
    RoleFromPort = Qt::UserRole + 1 ///<上游端口键
};

// 工具是否声明了指定方向的端口
static bool toolHasPort(ToolType type, PortDir dir, PortData data)
{
    const std::vector<PortDesc> ports = defaultPortsForType(type);
    for (size_t i = 0; i < ports.size(); ++i) {
        if (ports[i].slot.dir == dir && ports[i].slot.data == data) {
            return true;
        }
    }
    return false;
}

// 一行输入：下拉在左，状态或采集源说明在右
static QWidget* makeLinkRow(QComboBox* combo, QLabel* hint, QLabel* camText)
{
    QWidget* row = new QWidget(combo->parentWidget());
    QHBoxLayout* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->addWidget(combo, 1);
    // 保留 wordWrap 让标签能横向压缩，不挤出卡片
    // 同时锁死 28px 高，避免页宽为 0 时 sizeHint 按一行一字暴涨把窗口撑高
    if (hint) {
        lay->addWidget(hint);
        hint->setObjectName(QLatin1String("hint"));
        hint->setWordWrap(true);
        hint->setFixedHeight(28);
    }
    if (camText) {
        lay->addWidget(camText);
        camText->setObjectName(QLatin1String("hint"));
        camText->setWordWrap(true);
        camText->setFixedHeight(28);
    }
    return row;
}

// 绑定输入数据类型，供槽里识别来源
static void tagLinkCombo(QComboBox* combo, PortData data)
{
    combo->setProperty("portData", static_cast<int>(data));
}

// 取出下拉上标记的输入数据类型
static PortData linkComboPortData(const QComboBox* combo)
{
    return static_cast<PortData>(combo->property("portData").toInt());
}

// 标签在上、控件在下加入卡片
void addLinkField(QVBoxLayout* lay, QLabel* label, QWidget* field)
{
    label->setObjectName(QLatin1String("toolDebugFieldLabel"));
    lay->addWidget(label);
    lay->addWidget(field);
}

}

// 连接页与 ROI 页
ToolIoPanel::ToolIoPanel(QWidget* parent)
    : QWidget(parent)
    , m_linkPage(nullptr)
    , m_roiPage(nullptr)
    , m_imgRowLabel(nullptr)
    , m_imgCamText(nullptr)
    , m_imgCombo(nullptr)
    , m_imgHint(nullptr)
    , m_poseRowLabel(nullptr)
    , m_poseCombo(nullptr)
    , m_poseHint(nullptr)
    , m_regionRowLabel(nullptr)
    , m_regionCombo(nullptr)
    , m_regionHint(nullptr)
    , m_roiListHost(nullptr)
    , m_roiListLay(nullptr)
    , m_btnAdd(nullptr)
    , m_btnRedraw(nullptr)
    , m_btnClear(nullptr)
    , m_btnDel(nullptr)
    , m_fillingRoiList(false)
    , m_fillingLinks(false)
    , m_selectedRoiRow(-1)
    , m_tool(nullptr)
    , m_sol(nullptr)
    , m_job(nullptr)
    , m_canEdit(false)
{
    m_linkPage = new QFrame(this);
    m_roiPage = new QFrame(this);
    static_cast<QFrame*>(m_linkPage)->setFrameShape(QFrame::NoFrame);
    static_cast<QFrame*>(m_roiPage)->setFrameShape(QFrame::NoFrame);
    m_linkPage->setObjectName(QLatin1String("card"));
    m_roiPage->setObjectName(QLatin1String("card"));

    QVBoxLayout* linkLay = new QVBoxLayout(m_linkPage);
    // 卡片边框与内部控件留出呼吸间隙
    linkLay->setContentsMargins(12, 10, 12, 12);
    linkLay->setSpacing(6);

    m_imgCombo = new QComboBox(m_linkPage);
    m_imgCombo->setObjectName(QLatin1String("toolDebugCombo"));
    m_imgHint = new QLabel(m_linkPage);
    m_imgCamText = new QLabel(m_linkPage);
    tagLinkCombo(m_imgCombo, PortData::Image);
    QWidget* imgRow = makeLinkRow(m_imgCombo, m_imgHint, m_imgCamText);

    m_poseCombo = new QComboBox(m_linkPage);
    m_poseCombo->setObjectName(QLatin1String("toolDebugCombo"));
    m_poseHint = new QLabel(m_linkPage);
    tagLinkCombo(m_poseCombo, PortData::Pose);
    QWidget* poseRow = makeLinkRow(m_poseCombo, m_poseHint, 0);

    m_regionCombo = new QComboBox(m_linkPage);
    m_regionCombo->setObjectName(QLatin1String("toolDebugCombo"));
    m_regionHint = new QLabel(m_linkPage);
    tagLinkCombo(m_regionCombo, PortData::Region);
    QWidget* regionRow = makeLinkRow(m_regionCombo, m_regionHint, 0);

    m_imgRowLabel = new QLabel(T("图像"), m_linkPage);
    addLinkField(linkLay, m_imgRowLabel, imgRow);
    m_poseRowLabel = new QLabel(T("定位数据"), m_linkPage);
    addLinkField(linkLay, m_poseRowLabel, poseRow);
    m_regionRowLabel = new QLabel(T("区域"), m_linkPage);
    addLinkField(linkLay, m_regionRowLabel, regionRow);
    linkLay->setAlignment(Qt::AlignTop);
    // 输入页按内容高度，空白由外层卡片 stretch 吃掉
    m_imgCombo->setFixedHeight(28);
    m_poseCombo->setFixedHeight(28);
    m_regionCombo->setFixedHeight(28);
    m_linkPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    syncLinkPageHeight(m_linkPage);

    connect(m_imgCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onLinkComboChanged(int)));
    connect(m_poseCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onLinkComboChanged(int)));
    connect(m_regionCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onLinkComboChanged(int)));

    QVBoxLayout* roiLay = new QVBoxLayout(m_roiPage);
    // 与输入页保持一致的内边距
    roiLay->setContentsMargins(12, 10, 12, 12);
    roiLay->setSpacing(6);

    m_roiListHost = new QWidget(m_roiPage);
    m_roiListHost->setObjectName(QLatin1String("toolDebugRoiList"));
    m_roiListLay = new QVBoxLayout(m_roiListHost);
    m_roiListLay->setContentsMargins(0, 0, 0, 0);
    m_roiListLay->setSpacing(4);
    m_roiListLay->addStretch(1);

    m_btnAdd = new QPushButton(T("+ 添加"), m_roiPage);
    m_btnAdd->setObjectName(QLatin1String("toolDebugAddRoi"));
    m_btnRedraw = new QPushButton(T("重画"), m_roiPage);
    m_btnRedraw->setObjectName(QLatin1String("toolDebugAddRoi"));
    m_btnClear = new QPushButton(T("清除全部"), m_roiPage);
    m_btnClear->setObjectName(QLatin1String("toolDebugClearRoi"));
    m_btnDel = new QPushButton(T("删除"), m_roiPage);
    m_btnDel->setVisible(false);
    QHBoxLayout* btnLay = new QHBoxLayout();
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    btnLay->addStretch(1);
    btnLay->addWidget(m_btnAdd);
    btnLay->addWidget(m_btnRedraw);
    btnLay->addWidget(m_btnClear);

    roiLay->addWidget(m_roiListHost, 1);
    roiLay->addLayout(btnLay);

    connect(m_btnAdd, SIGNAL(clicked()), this, SIGNAL(addRoiRequested()));
    connect(m_btnRedraw, SIGNAL(clicked()), this, SIGNAL(addRoiRequested()));
    connect(m_btnClear, SIGNAL(clicked()), this, SIGNAL(clearRoiRequested()));
    connect(m_btnDel, SIGNAL(clicked()), this, SLOT(onDeleteClicked()));
}

// 指定子页根控件
QWidget* ToolIoPanel::pageWidget(ToolIoPage page) const
{
    if (page == ToolIoPage::Roi) {
        return m_roiPage;
    }
    return m_linkPage;
}

bool ToolIoPanel::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);
    Q_UNUSED(event);
    return QWidget::eventFilter(watched, event);
}

bool ToolIoPanel::slotAllowsRef(const ToolCaps& caps)
{
    for (size_t s = 0; s < caps.slotRules.size(); ++s) {
        const RoiSlotRule& slot = caps.slotRules[s];
        for (size_t k = 0; k < slot.itemKinds.size(); ++k) {
            if (slot.itemKinds[k] == RoiItemKind::RefRoi || slot.itemKinds[k] == RoiItemKind::RefRegion) {
                return true;
            }
        }
    }
    return false;
}

bool ToolIoPanel::slotAllowsDraw(const ToolCaps& caps)
{
    if (caps.slotRules.empty()) {
        return caps.usesRoi;
    }
    for (size_t s = 0; s < caps.slotRules.size(); ++s) {
        const RoiSlotRule& slot = caps.slotRules[s];
        if (slot.itemKinds.empty()) {
            return true;
        }
        for (size_t k = 0; k < slot.itemKinds.size(); ++k) {
            if (slot.itemKinds[k] == RoiItemKind::DrawRoi || slot.itemKinds[k] == RoiItemKind::DrawRegion) {
                return true;
            }
        }
    }
    return false;
}

std::string ToolIoPanel::defaultRoiSlot(const ToolCaps& caps)
{
    for (size_t s = 0; s < caps.slotRules.size(); ++s) {
        if (!caps.slotRules[s].slot.empty()) {
            return caps.slotRules[s].slot;
        }
    }
    return "roi";
}

// 填充输入端口候选来源
void ToolIoPanel::fillLinkCombo(QComboBox* combo, const JobCfg& job, const ObjectId& toId, PortData data)
{
    combo->clear();
    combo->addItem(T("未连接"));
    combo->setItemData(0, QString(), RoleFromId);
    combo->setItemData(0, QString(), RoleFromPort);

    const ToolCfg* toTool = job.findTool(toId);
    if (!toTool || !toolHasPort(toTool->type, PortDir::In, data)) {
        return;
    }
    for (size_t i = 0; i < job.tools.size(); ++i) {
        const ToolCfg& src = job.tools[i];
        if (src.id == toId) {
            continue;
        }
        const std::vector<PortDesc> ports = defaultPortsForType(src.type);
        for (size_t j = 0; j < ports.size(); ++j) {
            const PortSlot& slot = ports[j].slot;
            if (slot.dir != PortDir::Out || slot.data != data) {
                continue;
            }
            const QString label = qs(src.name) + QLatin1Char('_') + qs(portDataTitle(slot.data));
            combo->addItem(label);
            const int idx = combo->count() - 1;
            combo->setItemData(idx, qs(src.id.toString()), RoleFromId);
            combo->setItemData(idx, qs(portKey(slot)), RoleFromPort);
        }
    }
}

// 按流程里现有连线选中下拉项，没有连线时停在「未连接」
void ToolIoPanel::selectCurrentLink(QComboBox* combo, const JobCfg& job, const ObjectId& toId,
                                   const PortSlot& toPort)
{
    combo->setCurrentIndex(0);
    const ToolLink* L = job.findLinkToPort(toId, toPort);
    if (!L) {
        return;
    }
    const QString wantId = qs(L->fromId.toString());
    const QString wantPort = qs(portKey(L->fromPort));
    for (int i = 1; i < combo->count(); ++i) {
        if (combo->itemData(i, RoleFromId).toString() == wantId
            && combo->itemData(i, RoleFromPort).toString() == wantPort) {
            combo->setCurrentIndex(i);
            return;
        }
    }
}

// 刷新单个输入端口下拉与状态行
void ToolIoPanel::refreshLinkRow(QComboBox* combo, QLabel* hint, QLabel* camText,
                                 const JobCfg* jobConfig, const ToolCfg* tool, Job* job,
                                 PortData data, bool canEdit, bool showRow)
{
    if (m_imgRowLabel && combo == m_imgCombo) {
        m_imgRowLabel->setVisible(showRow);
    }
    if (m_poseRowLabel && combo == m_poseCombo) {
        m_poseRowLabel->setVisible(showRow);
    }
    if (m_regionRowLabel && combo == m_regionCombo) {
        m_regionRowLabel->setVisible(showRow);
    }
    combo->parentWidget()->setVisible(showRow);
    if (!showRow) {
        return;
    }

    // 取图工具的图像行不给下拉，改显示采集源尺寸和当前文件
    const bool isCam = tool && tool->type == ToolType::Acquire;
    const bool camImageRow = isCam && data == PortData::Image;
    combo->setVisible(!camImageRow);
    if (hint) {
        hint->setVisible(!camImageRow);
    }
    if (camText) {
        camText->setVisible(camImageRow);
        if (camImageRow) {
            QString line = T("本工具为采集源");
            if (job && job->hasCache(tool->id)) {
                const HsCommData::HsImage fb = job->cachedImage(tool->id);
                line += QString::fromUtf8("  (%1×%2)").arg(fb.width).arg(fb.height);
            } else {
                line += QLatin1String("  ") + T("未运行");
            }
            const std::string lf = runtimeGet(tool->runtime, RuntimeKey::LastFile).toString();
            if (!lf.empty()) {
                line += QLatin1String("\n") + T("当前文件：") + qs(lf);
            } else {
                const std::string p = tool->params.value("path").toString();
                if (!p.empty()) {
                    line += QLatin1String("\n") + T("路径覆盖：") + qs(p);
                }
            }
            camText->setText(line);
        } else {
            camText->clear();
        }
    }

    // 重填下拉期间屏蔽信号，避免误发连线变更
    m_fillingLinks = true;
    combo->blockSignals(true);
    if (jobConfig && tool && !camImageRow) {
        fillLinkCombo(combo, *jobConfig, tool->id, data);
        selectCurrentLink(combo, *jobConfig, tool->id, portSlotIn(data));
    } else if (!camImageRow) {
        combo->clear();
        combo->addItem(T("未连接"));
        combo->setCurrentIndex(0);
    }
    combo->setEnabled(canEdit && jobConfig && tool && !camImageRow);
    combo->blockSignals(false);
    m_fillingLinks = false;

    if (!hint || camImageRow) {
        return;
    }
    // 状态行：有缓存显示上游值，已连线但无缓存显示「未运行」
    hint->clear();
    if (!jobConfig || !tool) {
        return;
    }
    if (data == PortData::Image) {
        if (job) {
            const HsCommData::HsImage in = job->cachedInputImage(*jobConfig, tool->id);
            if (!in.isNull()) {
                hint->setText(QString::fromUtf8("(%1×%2)").arg(in.width).arg(in.height));
            } else if (jobConfig->findLinkToPort(tool->id, portSlotIn(PortData::Image))) {
                hint->setText(T("未运行"));
            }
        }
    } else if (data == PortData::Pose) {
        if (job) {
            HsCommData::Pose2 p;
            if (job->cachedInputPose(*jobConfig, tool->id, &p)) {
                hint->setText(QString::fromUtf8("X=%1 Y=%2 A=%3")
                                  .arg(p.tx, 0, 'f', 2)
                                  .arg(p.ty, 0, 'f', 2)
                                  .arg(p.ang(), 0, 'f', 4));
            } else if (jobConfig->findLinkToPort(tool->id, portSlotIn(PortData::Pose))) {
                hint->setText(T("未运行"));
            }
        }
    } else if (data == PortData::Logic) {
        if (job) {
            const ParamMap v = job->cachedValues(tool->id);
            if (paramHas(v, ValueKey::Ok) || paramHas(v, ValueKey::Pass)) {
                const std::string okv = paramHas(v, ValueKey::Ok)
                    ? paramGet(v, ValueKey::Ok).toString()
                    : paramGet(v, ValueKey::Pass).toString();
                hint->setText(QLatin1String("OK=") + qs(okv));
            } else if (jobConfig->findLinkToPort(tool->id, portSlotIn(PortData::Logic))) {
                hint->setText(T("未运行"));
            }
        }
    } else if (data == PortData::Region) {
        if (jobConfig->findLinkToPort(tool->id, portSlotIn(PortData::Region)) && job
            && job->cachedInputImage(*jobConfig, tool->id).isNull()) {
            hint->setText(T("未运行"));
        }
    }
}
// 输入下拉选中变化
void ToolIoPanel::onLinkComboChanged(int index)
{
    Q_UNUSED(index);
    if (m_fillingLinks) {
        return;
    }
    QComboBox* combo = qobject_cast<QComboBox*>(sender());
    if (!combo) {
        return;
    }
    const PortData data = linkComboPortData(combo);
    const QString fromId = combo->itemData(combo->currentIndex(), RoleFromId).toString();
    const QString fromPort = combo->itemData(combo->currentIndex(), RoleFromPort).toString();
    emit inputLinkChanged(static_cast<int>(data), fromId, fromPort);
}

// 刷新输入连线、位姿缓存与 ROI 摘要
void ToolIoPanel::refresh(Solution* sol, ToolCfg* tool, Job* job, bool canEdit)
{
    m_sol = sol;
    m_tool = tool;
    m_job = job;
    m_canEdit = canEdit;
    if (!tool) {
        refreshLinkRow(m_imgCombo, m_imgHint, m_imgCamText, 0, 0, job, PortData::Image, false, true);
        refreshLinkRow(m_poseCombo, m_poseHint, 0, 0, 0, job, PortData::Pose, false, false);
        refreshLinkRow(m_regionCombo, m_regionHint, 0, 0, 0, job, PortData::Region, false, false);
        fillRoiList();
        m_btnAdd->setEnabled(false);
        m_btnRedraw->setEnabled(false);
        m_btnClear->setEnabled(false);
        syncLinkPageHeight(m_linkPage);
        return;
    }

    // 输入行：图像；逻辑和通信工具把第二行改作逻辑输入，其余为定位；区域行仅在流程内显示
    JobCfg* jobConfig = sol ? sol->findJobCfgOfTool(tool->id, 0) : nullptr;
    const bool usesRoi = tool->usesRoi();
    const bool isCam = (tool->type == ToolType::Acquire);
    const bool isLogic = (tool->type == ToolType::Logic || tool->type == ToolType::Comm);

    const bool showImage = isCam || toolHasPort(tool->type, PortDir::In, PortData::Image);
    refreshLinkRow(m_imgCombo, m_imgHint, m_imgCamText, jobConfig, tool, job,
                   PortData::Image, canEdit, showImage);

    if (isLogic) {
        tagLinkCombo(m_poseCombo, PortData::Logic);
        if (m_poseRowLabel) {
            m_poseRowLabel->setText(T("输入逻辑"));
        }
        refreshLinkRow(m_poseCombo, m_poseHint, 0, jobConfig, tool, job,
                       PortData::Logic, canEdit, true);
    } else {
        tagLinkCombo(m_poseCombo, PortData::Pose);
        if (m_poseRowLabel) {
            m_poseRowLabel->setText(T("定位数据"));
        }
        const bool showPose = !isCam && toolHasPort(tool->type, PortDir::In, PortData::Pose);
        refreshLinkRow(m_poseCombo, m_poseHint, 0, jobConfig, tool, job,
                       PortData::Pose, canEdit, showPose);
    }

    const bool showRegion = jobConfig && toolHasPort(tool->type, PortDir::In, PortData::Region);
    refreshLinkRow(m_regionCombo, m_regionHint, 0, jobConfig, tool, job,
                   PortData::Region, canEdit, showRegion);

    const bool usesMulti = tool->usesMultiRoi();
    if (!usesRoi) {
        if (m_roiListHost) {
            m_roiListHost->setVisible(false);
        }
        m_btnAdd->setVisible(false);
        m_btnRedraw->setVisible(false);
        m_btnClear->setVisible(false);
        fillRoiList();
    } else {
        if (m_roiListHost) {
            m_roiListHost->setVisible(true);
        }
        m_btnAdd->setVisible(true);
        m_btnClear->setVisible(true);
        fillRoiList();
    }

    const bool roiOn = usesRoi && canEdit && tool;
    const bool hasRoi = usesRoi && !tool->param.roiItems.empty();
    if (m_btnAdd) {
        m_btnAdd->setText(T("+ 添加"));
        m_btnAdd->setToolTip(T("添加一条 ROI；可在行内切换绘制或引用"));
    }
    if (m_btnRedraw) {
        m_btnRedraw->setVisible(usesRoi && hasRoi && !usesMulti);
        m_btnRedraw->setToolTip(T("进入绘制模式，重画并替换现有 ROI"));
    }
    m_btnAdd->setEnabled(roiOn);
    if (m_btnRedraw) {
        m_btnRedraw->setEnabled(roiOn && hasRoi && !usesMulti);
    }
    m_btnClear->setEnabled(roiOn && hasRoi);
    syncLinkPageHeight(m_linkPage);
}

int ToolIoPanel::selectedRoiRow() const
{
    return m_selectedRoiRow;
}

void ToolIoPanel::clearRoiSelection()
{
    m_selectedRoiRow = -1;
}

void ToolIoPanel::selectRoiRow(int row)
{
    if (!m_tool || row < 0 || row >= static_cast<int>(m_tool->param.roiItems.size())) {
        m_selectedRoiRow = -1;
    } else {
        m_selectedRoiRow = row;
    }
}

void ToolIoPanel::onSelectRoiRow()
{
    if (m_fillingRoiList) {
        return;
    }
    QWidget* w = qobject_cast<QWidget*>(sender());
    if (!w) {
        return;
    }
    const int row = w->property("roiRow").toInt();
    if (m_selectedRoiRow == row) {
        m_selectedRoiRow = -1;
    } else {
        m_selectedRoiRow = row;
    }
    emit roiSelectionChanged(m_selectedRoiRow);
}

void ToolIoPanel::onDeleteClicked()
{
    if (m_selectedRoiRow >= 0) {
        emit removeRoiRequested(m_selectedRoiRow);
    } else {
        emit removeLastRoiRequested();
    }
}

void ToolIoPanel::onDeleteRowClicked()
{
    QWidget* btn = qobject_cast<QWidget*>(sender());
    if (!btn) {
        return;
    }
    const int row = btn->property("roiRow").toInt();
    if (row >= 0) {
        QTimer::singleShot(0, this, [this, row]() { emit removeRoiRequested(row); });
    }
}

void ToolIoPanel::onDrawRowClicked()
{
    QWidget* btn = qobject_cast<QWidget*>(sender());
    if (!btn) {
        return;
    }
    const int row = btn->property("roiRow").toInt();
    if (row >= 0) {
        m_selectedRoiRow = row;
        emit drawRoiRequested(row);
    }
}

void ToolIoPanel::onRoiEnableToggled(bool on)
{
    if (m_fillingRoiList || !m_tool || !m_canEdit) {
        return;
    }
    QCheckBox* box = qobject_cast<QCheckBox*>(sender());
    if (!box) {
        return;
    }
    const int row = box->property("roiRow").toInt();
    if (row < 0 || row >= static_cast<int>(m_tool->param.roiItems.size())) {
        return;
    }
    ToolRoiItem item = m_tool->param.roiItems[static_cast<size_t>(row)];
    item.enabled = on;
    m_tool->setRoiItemAt(row, item);
    emit roiItemsEdited();
}

void ToolIoPanel::onRoiKindChanged(int index)
{
    Q_UNUSED(index);
    if (m_fillingRoiList || !m_tool || !m_canEdit) {
        return;
    }
    QComboBox* box = qobject_cast<QComboBox*>(sender());
    if (!box) {
        return;
    }
    const int row = box->property("roiRow").toInt();
    if (row < 0 || row >= static_cast<int>(m_tool->param.roiItems.size())) {
        return;
    }
    ToolRoiItem item = m_tool->param.roiItems[static_cast<size_t>(row)];
    const int choice = box->currentData().toInt();
    const bool wantRef = (choice == static_cast<int>(RoiItemKind::RefRoi)
                          || choice == static_cast<int>(RoiItemKind::RefRegion));
    if (wantRef) {
        item.kind = (choice == static_cast<int>(RoiItemKind::RefRegion))
                        ? RoiItemKind::RefRegion : RoiItemKind::RefRoi;
        item.shape = HsCommData::HsObject();
    } else {
        item.kind = RoiItemKind::DrawRoi;
        item.refToolId = ObjectId();
        item.refIndex = -1;
    }
    m_tool->setRoiItemAt(row, item);
    emit roiItemsEdited();
    QTimer::singleShot(0, this, [this]() { fillRoiList(); });
}

void ToolIoPanel::onRoiRefChanged(int index)
{
    Q_UNUSED(index);
    if (m_fillingRoiList || !m_tool || !m_canEdit) {
        return;
    }
    QComboBox* box = qobject_cast<QComboBox*>(sender());
    if (!box) {
        return;
    }
    const int row = box->property("roiRow").toInt();
    if (row < 0 || row >= static_cast<int>(m_tool->param.roiItems.size())) {
        return;
    }
    ToolRoiItem item = m_tool->param.roiItems[static_cast<size_t>(row)];
    const QString fromId = box->currentData().toString();
    item.refToolId = fromId.isEmpty() ? ObjectId() : ObjectId::fromString(fromId.toStdString());
    if (item.kind != RoiItemKind::RefRoi && item.kind != RoiItemKind::RefRegion) {
        item.kind = RoiItemKind::RefRoi;
    }
    m_tool->setRoiItemAt(row, item);
    emit roiItemsEdited();
}

void ToolIoPanel::onRoiRefIndexChanged(int value)
{
    if (m_fillingRoiList || !m_tool || !m_canEdit) {
        return;
    }
    QSpinBox* box = qobject_cast<QSpinBox*>(sender());
    if (!box) {
        return;
    }
    const int row = box->property("roiRow").toInt();
    if (row < 0 || row >= static_cast<int>(m_tool->param.roiItems.size())) {
        return;
    }
    ToolRoiItem item = m_tool->param.roiItems[static_cast<size_t>(row)];
    item.refIndex = value;
    m_tool->setRoiItemAt(row, item);
    emit roiItemsEdited();
}

void ToolIoPanel::fillRoiList()
{
    if (!m_roiListLay) {
        return;
    }
    m_fillingRoiList = true;
    while (QLayoutItem* child = m_roiListLay->takeAt(0)) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }
    if (!m_tool || !m_tool->usesRoi()) {
        m_roiListLay->addStretch(1);
        m_fillingRoiList = false;
        return;
    }

    m_tool->syncRois();
    const ToolCaps caps = m_tool->caps();
    const bool allowRef = slotAllowsRef(caps);
    const bool allowDraw = slotAllowsDraw(caps);
    const bool multiSlot = caps.slotRules.size() > 1;
    const JobCfg* jobConfig = m_sol ? m_sol->findJobCfgOfTool(m_tool->id, 0) : nullptr;
    const std::vector<HsCommData::HsObject> preview = m_tool->displayRois(m_job);

    // 多分区：先按槽位输出标题（含空槽），再列本槽条目
    if (multiSlot) {
        for (size_t s = 0; s < caps.slotRules.size(); ++s) {
            const RoiSlotRule& rule = caps.slotRules[s];
            QString title = qs(rule.title.empty() ? rule.slot : rule.title);
            QLabel* head = new QLabel(title, m_roiListHost);
            head->setObjectName(QLatin1String("toolDebugFieldLabel"));
            m_roiListLay->addWidget(head);

            int slotCount = 0;
            for (int i = 0; i < static_cast<int>(m_tool->param.roiItems.size()); ++i) {
                const ToolRoiItem& item = m_tool->param.roiItems[static_cast<size_t>(i)];
                if (item.slot != rule.slot) {
                    continue;
                }
                ++slotCount;
                appendRoiRow(i, item, allowRef, allowDraw, jobConfig, preview);
            }
            if (slotCount == 0) {
                QLabel* empty = new QLabel(T("（空，点下方「添加」写入本分区）"), m_roiListHost);
                empty->setObjectName(QLatin1String("hint"));
                m_roiListLay->addWidget(empty);
            }
        }
        m_roiListLay->addStretch(1);
        m_fillingRoiList = false;
        return;
    }

    for (int i = 0; i < static_cast<int>(m_tool->param.roiItems.size()); ++i) {
        const ToolRoiItem& item = m_tool->param.roiItems[static_cast<size_t>(i)];
        appendRoiRow(i, item, allowRef, allowDraw, jobConfig, preview);
    }
    m_roiListLay->addStretch(1);
    m_fillingRoiList = false;
}

void ToolIoPanel::appendRoiRow(int i, const ToolRoiItem& item, bool allowRef, bool allowDraw,
                               const JobCfg* jobConfig,
                               const std::vector<HsCommData::HsObject>& preview)
{
        const bool isRef = item.kind == RoiItemKind::RefRoi || item.kind == RoiItemKind::RefRegion;
        QWidget* line = new QWidget(m_roiListHost);
        line->setObjectName(QLatin1String("toolDebugRoiRow"));
        line->setProperty("roiRow", i);
        QHBoxLayout* lay = new QHBoxLayout(line);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);

        QCheckBox* enable = new QCheckBox(T("使能"), line);
        enable->setProperty("roiRow", i);
        enable->setChecked(item.enabled);
        enable->setEnabled(m_canEdit);
        connect(enable, SIGNAL(toggled(bool)), this, SLOT(onRoiEnableToggled(bool)));
        lay->addWidget(enable);

        if (allowDraw && allowRef) {
            QComboBox* kindBox = new QComboBox(line);
            kindBox->setProperty("roiRow", i);
            kindBox->setObjectName(QLatin1String("toolDebugCombo"));
            kindBox->addItem(T("绘制区域"), static_cast<int>(RoiItemKind::DrawRoi));
            kindBox->addItem(T("引用区域"), static_cast<int>(RoiItemKind::RefRoi));
            kindBox->setCurrentIndex(isRef ? 1 : 0);
            kindBox->setEnabled(m_canEdit);
            connect(kindBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onRoiKindChanged(int)));
            lay->addWidget(kindBox);
        }

        if (isRef) {
            QComboBox* src = new QComboBox(line);
            src->setObjectName(QLatin1String("toolDebugCombo"));
            src->setProperty("roiRow", i);
            src->addItem(T("未选择"), QString());
            int pick = 0;
            if (jobConfig) {
                for (size_t t = 0; t < jobConfig->tools.size(); ++t) {
                    const ToolCfg& up = jobConfig->tools[t];
                    if (up.id == m_tool->id) {
                        continue;
                    }
                    const bool hasRoiOut = toolHasPort(up.type, PortDir::Out, PortData::Roi)
                        || toolHasPort(up.type, PortDir::Out, PortData::Region);
                    if (!hasRoiOut) {
                        continue;
                    }
                    src->addItem(qs(up.name), qs(up.id.toString()));
                    if (up.id == item.refToolId) {
                        pick = src->count() - 1;
                    }
                }
            }
            src->setCurrentIndex(pick);
            src->setEnabled(m_canEdit);
            connect(src, SIGNAL(currentIndexChanged(int)), this, SLOT(onRoiRefChanged(int)));
            QSpinBox* which = new QSpinBox(line);
            which->setProperty("roiRow", i);
            which->setRange(-1, 64);
            which->setSpecialValueText(T("全部"));
            which->setValue(item.refIndex);
            which->setEnabled(m_canEdit);
            connect(which, SIGNAL(valueChanged(int)), this, SLOT(onRoiRefIndexChanged(int)));
            lay->addWidget(src, 1);
            lay->addWidget(which);
        } else {
            HsCommData::HsObject shape = item.shape;
            if (i < static_cast<int>(preview.size()) && preview[static_cast<size_t>(i)].isInitialized()) {
                shape = preview[static_cast<size_t>(i)];
            }
            QString summary = QString::fromUtf8("ROI_%1  ").arg(i + 1);
            if (shape.isInitialized() && !shape.isEmpty()) {
                summary += qs(formatRoiDetail(shape));
            } else {
                summary += T("未绘制");
            }
            QPushButton* pick = new QPushButton(summary, line);
            pick->setFlat(true);
            pick->setProperty("roiRow", i);
            pick->setCursor(Qt::PointingHandCursor);
            connect(pick, SIGNAL(clicked()), this, SLOT(onSelectRoiRow()));
            QPushButton* draw = new QPushButton(T("绘制"), line);
            draw->setProperty("roiRow", i);
            draw->setEnabled(m_canEdit);
            connect(draw, SIGNAL(clicked()), this, SLOT(onDrawRowClicked()));
            lay->addWidget(pick, 1);
            lay->addWidget(draw);
        }

        QToolButton* del = new QToolButton(line);
        del->setObjectName(QLatin1String("toolDebugRoiDel"));
        del->setText(QString::fromUtf8("×"));
        del->setProperty("roiRow", i);
        del->setAutoRaise(true);
        del->setEnabled(m_canEdit);
        connect(del, SIGNAL(clicked()), this, SLOT(onDeleteRowClicked()));
        lay->addWidget(del);
        m_roiListLay->addWidget(line);
}

// 在 ROI 旁标注序号和几何；onlyIndex >= 0 时只标这一条
void overlayRoiDetails(HsCtrl::HsImageWindow* view, const ToolCfg* tool, int onlyIndex,
                       bool showLabels)
{
    if (!view) {
        return;
    }
    view->clearDrawInfo(HsCtrl::DrawInfoImage);
    if (!showLabels || !tool || !tool->usesRoi()) {
        return;
    }
    const std::vector<HsCommData::HsObject> rs = tool->displayRois(nullptr);
    for (int i = 0; i < static_cast<int>(rs.size()); ++i) {
        if (onlyIndex >= 0 && i != onlyIndex) {
            continue;
        }
        const HsCommData::HsObject& r = rs[static_cast<size_t>(i)];
        if (!r.isInitialized() || r.isEmpty()) {
            continue;
        }
        double ax = 0;
        double ay = 0;
        roiLabelAnchor(r, &ax, &ay);
        QString line = QLatin1String("#") + QString::number(i + 1) + QLatin1String("  ")
                       + qs(formatRoiDetail(r));
        view->addDrawInfo(HsCtrl::DrawInfoImage, line, QPointF(ax, ay), 12,
                          QColor(0xFA, 0x8C, 0x16));
    }
}

// 调试底图：采集用缓存输出，其他用上游输入
HsCommData::HsImage debugBaseFrame(ToolCfg* tool, JobCfg* jobConfig, Job* job)
{
    if (!tool || !job) {
        return HsCommData::HsImage();
    }
    if (tool->type == ToolType::Acquire) {
        return job->cachedImage(tool->id);
    }
    if (jobConfig) {
        const HsCommData::HsImage in = job->cachedInputImage(*jobConfig, tool->id);
        if (!in.isNull()) {
            return in;
        }
    }
    return HsCommData::HsImage();
}

// 按选中 ROI 与是否可编辑同步 Drawing 交互
void syncDrawingInteraction(HsCtrl::HsImageWindow* view, bool editable,
                            int selectedIndex, bool hasDrawings)
{
    if (!view) {
        return;
    }
    view->setDrawingVisible(hasDrawings || editable);
    if (hasDrawings && editable && selectedIndex >= 0) {
    } else {
    }
}

}

#include "moc_ToolIoPanel.cpp"
