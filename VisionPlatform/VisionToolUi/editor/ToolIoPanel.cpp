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
#include <QComboBox>
#include <QFrame>
#include <QSizePolicy>
#include <QToolButton>
#include <QTableWidget>
#include <QTableWidgetItem>
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

// 
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

// 
void addLinkField(QVBoxLayout* lay, QLabel* label, QWidget* field)
{
    label->setObjectName(QLatin1String("toolDebugFieldLabel"));
    lay->addWidget(label);
    lay->addWidget(field);
}

}

// 
static QTableWidgetItem* roiCell(const QString& text, bool numeric)
{
    QTableWidgetItem* it = new QTableWidgetItem(text);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    if (numeric) {
        it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    return it;
}

// 一位小数；空单元格传 ok=false
static QTableWidgetItem* roiNum(double v, bool ok)
{
    if (!ok) {
        return roiCell(QString(), true);
    }
    return roiCell(QString::number(v, 'f', 1), true);
}

// 
int roiKindOf(const HsCommData::HsObject& r)
{
    return r.isInitialized() && !r.isEmpty() ? static_cast<int>(r.kind()) : -1;
}

// 从几何取出表格用的中心和尺寸
static void roiNumbers(const HsCommData::HsObject& r, double* x, double* y, double* a, double* b, double* phi, int* npts)
{
    double xx = 0, yy = 0, aa = 0, bb = 0, pp = 0;
    int nn = 0;
    if (r.isInitialized() && !r.isEmpty()) {
        using namespace HsCommData;
        const ObjectKind k = r.kind();
        if (k == ObjCircle) {
            const CircleP* c = r.circleAt(0);
            xx = c->c.x;
            yy = c->c.y;
            aa = c->r;
        } else if (k == ObjEllipse) {
            const EllipseP* e = r.ellipseAt(0);
            xx = e->c.x;
            yy = e->c.y;
            aa = e->ra;
            bb = e->rb;
            pp = e->phi;
        } else if (k == ObjRect1) {
            const Rect1P* rc = r.rect1At(0);
            xx = rc->r.x;
            yy = rc->r.y;
            aa = rc->r.w;
            bb = rc->r.h;
        } else if (k == ObjRect2) {
            const Rect2P* rc = r.rect2At(0);
            xx = rc->c.x;
            yy = rc->c.y;
            aa = rc->l1;
            bb = rc->l2;
            pp = rc->phi;
        } else if (k == ObjLine) {
            const LineP* ln = r.lineAt(0);
            xx = ln->a.x;
            yy = ln->a.y;
            aa = ln->b.x - ln->a.x;
            bb = ln->b.y - ln->a.y;
        } else if (k == ObjContour) {
            const HsContour* c = r.contourAt(0);
            nn = c ? static_cast<int>(c->ptCount()) : 0;
            if (c && !c->ptsEmpty()) {
                xx = c->ptAt(0).x;
                yy = c->ptAt(0).y;
            }
        }
    }
    if (x) *x = xx;
    if (y) *y = yy;
    if (a) *a = aa;
    if (b) *b = bb;
    if (phi) *phi = pp;
    if (npts) *npts = nn;
}

static QString formatRoiPosition(const HsCommData::HsObject& r)
{
    double x = 0, y = 0;
    roiNumbers(r, &x, &y, 0, 0, 0, 0);
    return QString::fromUtf8("%1, %2").arg(x, 0, 'f', 1).arg(y, 0, 'f', 1);
}

// 
QString formatRoiSize(const HsCommData::HsObject& r, int kind)
{
    double x = 0, y = 0, a = 0, b = 0;
    int n = 0;
    roiNumbers(r, &x, &y, &a, &b, 0, &n);
    if (kind == static_cast<int>(HsCommData::ObjCircle)) {
        return QString::fromUtf8("R=%1").arg(a, 0, 'f', 1);
    }
    if (kind == static_cast<int>(HsCommData::ObjRect1)) {
        return QString::fromUtf8("%1 脳 %2").arg(a, 0, 'f', 1).arg(b, 0, 'f', 1);
    }
    if (kind == static_cast<int>(HsCommData::ObjRect2) || kind == static_cast<int>(HsCommData::ObjEllipse)) {
        return QString::fromUtf8("%1 脳 %2").arg(a * 2.0, 0, 'f', 1).arg(b * 2.0, 0, 'f', 1);
    }
    if (kind == static_cast<int>(HsCommData::ObjLine)) {
        const double len = std::sqrt(a * a + b * b);
        return QString::fromUtf8("L=%1").arg(len, 0, 'f', 1);
    }
    if (kind == static_cast<int>(HsCommData::ObjContour)) {
        return QString::fromLocal8Bit("顶点=%1").arg(n);
    }
    return QString::fromUtf8("%1 脳 %2").arg(a, 0, 'f', 1).arg(b, 0, 'f', 1);
}

// 全部同类则用专列表头，否则用通用 A/B
static int roiTableKind(const std::vector<HsCommData::HsObject>& rs)
{
    if (rs.empty()) {
        return -1;
    }
    const int k = roiKindOf(rs[0]);
    for (int i = 1; i < static_cast<int>(rs.size()); ++i) {
        if (roiKindOf(rs[static_cast<size_t>(i)]) != k) {
            return -1;
        }
    }
    return k;
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
    , m_roiTable(nullptr)
    , m_btnAdd(nullptr)
    , m_btnRedraw(nullptr)
    , m_btnClear(nullptr)
    , m_btnDel(nullptr)
    , m_fillingRoiTable(false)
    , m_fillingLinks(false)
    , m_pressSelectedRow(-1)
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

    m_roiTable = new QTableWidget(m_roiPage);
    m_roiTable->setObjectName(QLatin1String("toolDebugRoiTable"));
    m_roiTable->setColumnCount(5);
    QStringList hh;
    hh << T("ROI ID") << T("类型") << T("位置") << T("尺寸") << QString();
    m_roiTable->setHorizontalHeaderLabels(hh);
    m_roiTable->verticalHeader()->setVisible(false);
    m_roiTable->verticalHeader()->setFixedWidth(0);
    m_roiTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_roiTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_roiTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_roiTable->setAlternatingRowColors(true);
    m_roiTable->horizontalHeader()->setStretchLastSection(true);
    m_roiTable->verticalHeader()->setDefaultSectionSize(28);
    m_roiTable->setMinimumHeight(72);
    m_roiTable->viewport()->installEventFilter(this);

    m_btnAdd = new QPushButton(T("+ 添加 ROI"), m_roiPage);
    m_btnAdd->setObjectName(QLatin1String("toolDebugAddRoi"));
    m_btnRedraw = new QPushButton(T("重画 ROI"), m_roiPage);
    m_btnRedraw->setObjectName(QLatin1String("toolDebugAddRoi"));
    m_btnClear = new QPushButton(T("清除全部"), m_roiPage);
    m_btnClear->setObjectName(QLatin1String("toolDebugClearRoi"));
    m_btnDel = new QPushButton(T("删除"), m_roiPage);
    m_btnDel->setVisible(false);
    QHBoxLayout* btnLay = new QHBoxLayout();
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);
    btnLay->addWidget(m_btnAdd);
    btnLay->addWidget(m_btnRedraw);
    btnLay->addWidget(m_btnClear);
    btnLay->addStretch();

    roiLay->addWidget(m_roiTable, 1);
    roiLay->addLayout(btnLay);

    connect(m_btnAdd, SIGNAL(clicked()), this, SIGNAL(addRoiRequested()));
    connect(m_btnRedraw, SIGNAL(clicked()), this, SIGNAL(addRoiRequested()));
    connect(m_btnClear, SIGNAL(clicked()), this, SIGNAL(clearRoiRequested()));
    connect(m_btnDel, SIGNAL(clicked()), this, SLOT(onDeleteClicked()));
    connect(m_roiTable, SIGNAL(itemSelectionChanged()), this, SLOT(onRoiSelectionChanged()));
}

// 指定子页根控件
QWidget* ToolIoPanel::pageWidget(ToolIoPage page) const
{
    if (page == ToolIoPage::Roi) {
        return m_roiPage;
    }
    return m_linkPage;
}

// 按下时已选中的行，释放时仍在同一格则取消选中
bool ToolIoPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_roiTable || watched != m_roiTable->viewport()) {
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            const QModelIndex idx = m_roiTable->indexAt(me->pos());
            if (idx.isValid() && m_roiTable->selectionModel()
                && m_roiTable->selectionModel()->isSelected(idx)) {
                m_pressSelectedRow = idx.row();
            } else {
                m_pressSelectedRow = -1;
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_pressSelectedRow >= 0 && !m_fillingRoiTable) {
            const QModelIndex idx = m_roiTable->indexAt(me->pos());
            if (idx.isValid() && idx.row() == m_pressSelectedRow
                && m_roiTable->currentRow() == m_pressSelectedRow) {
                clearRoiSelection();
                emit roiSelectionChanged(-1);
                m_pressSelectedRow = -1;
                return true;
            }
        }
        m_pressSelectedRow = -1;
    }
    return QWidget::eventFilter(watched, event);
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

// 
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
                line += QString::fromUtf8("  (%1脳%2)").arg(fb.width).arg(fb.height);
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
    hint->clear();
    if (!jobConfig || !tool) {
        return;
    }
    if (data == PortData::Image) {
        if (job) {
            const HsCommData::HsImage in = job->cachedInputImage(*jobConfig, tool->id);
            if (!in.isNull()) {
                hint->setText(QString::fromUtf8("(%1脳%2)").arg(in.width).arg(in.height));
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
    if (!tool) {
        refreshLinkRow(m_imgCombo, m_imgHint, m_imgCamText, 0, 0, job, PortData::Image, false, true);
        refreshLinkRow(m_poseCombo, m_poseHint, 0, 0, 0, job, PortData::Pose, false, false);
        refreshLinkRow(m_regionCombo, m_regionHint, 0, 0, 0, job, PortData::Region, false, false);
        fillRoiTable(std::vector<HsCommData::HsObject>());
        m_roiTable->setEnabled(false);
        m_btnAdd->setEnabled(false);
        m_btnRedraw->setEnabled(false);
        m_btnClear->setEnabled(false);
        m_btnDel->setEnabled(false);
        syncLinkPageHeight(m_linkPage);
        return;
    }

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
        fillRoiTable(std::vector<HsCommData::HsObject>());
        m_roiTable->setVisible(false);
        m_btnAdd->setVisible(false);
        m_btnRedraw->setVisible(false);
        m_btnClear->setVisible(false);
        m_btnDel->setVisible(false);
    } else {
        const std::vector<HsCommData::HsObject> rs = tool->effectiveRois();
        m_roiTable->setVisible(true);
        m_btnAdd->setVisible(true);
        m_btnClear->setVisible(true);
        m_btnDel->setVisible(false);
        fillRoiTable(rs);
        m_roiTable->setEnabled(true);
    }

    const bool roiOn = usesRoi && canEdit && tool;
    const bool hasRoi = usesRoi && !tool->effectiveRois().empty();
    if (m_btnAdd) {
        m_btnAdd->setText(T("+ 添加 ROI"));
        m_btnAdd->setToolTip(T("进入绘制模式，在空白处添加 ROI"));
    }
    if (m_btnRedraw) {
        m_btnRedraw->setVisible(usesRoi && hasRoi && !usesMulti);
        m_btnRedraw->setToolTip(T("进入绘制模式，在空白处重画并替换现有 ROI"));
    }
    m_btnAdd->setEnabled(roiOn);
    if (m_btnRedraw) {
        m_btnRedraw->setVisible(usesRoi && hasRoi && !usesMulti);
        m_btnRedraw->setEnabled(roiOn && hasRoi && !usesMulti);
    }
    m_btnClear->setEnabled(roiOn && hasRoi);
    syncLinkPageHeight(m_linkPage);
}
// 
int ToolIoPanel::selectedRoiRow() const
{
    return m_roiTable ? m_roiTable->currentRow() : -1;
}

// 取消表格选中
void ToolIoPanel::clearRoiSelection()
{
    if (!m_roiTable) {
        return;
    }
    m_fillingRoiTable = true;
    m_roiTable->blockSignals(true);
    if (m_roiTable->selectionModel()) {
        m_roiTable->selectionModel()->blockSignals(true);
    }
    m_roiTable->clearSelection();
    m_pressSelectedRow = -1;
    if (m_roiTable->selectionModel()) {
        m_roiTable->selectionModel()->blockSignals(false);
    }
    m_roiTable->blockSignals(false);
    m_fillingRoiTable = false;
}

// 
void ToolIoPanel::selectRoiRow(int row)
{
    if (!m_roiTable) {
        return;
    }
    m_fillingRoiTable = true;
    m_roiTable->blockSignals(true);
    if (m_roiTable->selectionModel()) {
        m_roiTable->selectionModel()->blockSignals(true);
    }
    if (row >= 0 && row < m_roiTable->rowCount()) {
        m_roiTable->selectRow(row);
    } else {
        m_roiTable->clearSelection();
    }
    m_pressSelectedRow = -1;
    if (m_roiTable->selectionModel()) {
        m_roiTable->selectionModel()->blockSignals(false);
    }
    m_roiTable->blockSignals(false);
    m_fillingRoiTable = false;
}

// 
void ToolIoPanel::onRoiSelectionChanged()
{
    if (m_fillingRoiTable) {
        return;
    }
    emit roiSelectionChanged(selectedRoiRow());
}

// 删除选中行；无选中则删末个
void ToolIoPanel::onDeleteClicked()
{
    const int row = m_roiTable ? m_roiTable->currentRow() : -1;
    if (row >= 0) {
        emit removeRoiRequested(row);
    } else {
        emit removeLastRoiRequested();
    }
}

// 
void ToolIoPanel::onDeleteRowClicked()
{
    QToolButton* btn = qobject_cast<QToolButton*>(sender());
    if (!btn) {
        return;
    }
    const int row = btn->property("roiRow").toInt();
    if (row >= 0) {
        QTimer::singleShot(0, this, [this, row]() { emit removeRoiRequested(row); });
    }
}

// 按种类设列并填行
void ToolIoPanel::fillRoiTable(const std::vector<HsCommData::HsObject>& rs)
{
    if (!m_roiTable) {
        return;
    }
    const int keep = m_roiTable->currentRow();
    int kind = roiTableKind(rs);
    const bool compact = (kind >= 0 || rs.empty());
    m_fillingRoiTable = true;
    m_roiTable->blockSignals(true);
    if (m_roiTable->selectionModel()) {
        m_roiTable->selectionModel()->blockSignals(true);
    }
    m_roiTable->clearContents();
    m_roiTable->setRowCount(static_cast<int>(rs.size()));

    if (compact) {
        if (kind < 0 && rs.empty()) {
            kind = static_cast<int>(HsCommData::ObjRect1);
        }
        QStringList hh;
        hh << T("ROI ID") << T("类型") << T("位置") << T("尺寸");
        hh << QString();
        const int cols = hh.size();
        const int delCol = cols - 1;
        m_roiTable->setColumnCount(cols);
        m_roiTable->setHorizontalHeaderLabels(hh);
        for (int i = 0; i < static_cast<int>(rs.size()); ++i) {
            const HsCommData::HsObject& r = rs[static_cast<size_t>(i)];
            const int rk = (kind >= 0) ? kind : roiKindOf(r);
            m_roiTable->setItem(i, 0, roiCell(QString::fromUtf8("ROI_%1").arg(i + 1), false));
            m_roiTable->setItem(i, 1, roiCell(QString::fromUtf8(roiKindTitle(roiKindOf(r)).c_str()), false));
            m_roiTable->setItem(i, 2, roiCell(formatRoiPosition(r), false));
            m_roiTable->setItem(i, 3, roiCell(formatRoiSize(r, rk), false));
            QWidget* cell = new QWidget(m_roiTable);
            QHBoxLayout* cellLay = new QHBoxLayout(cell);
            cellLay->setContentsMargins(0, 0, 0, 0);
            cellLay->setAlignment(Qt::AlignCenter);
            QToolButton* del = new QToolButton(cell);
            del->setObjectName(QLatin1String("toolDebugRoiDel"));
            del->setText(QString::fromUtf8("脳"));
            del->setToolTip(T("删除该 ROI"));
            del->setProperty("roiRow", i);
            del->setAutoRaise(true);
            connect(del, SIGNAL(clicked()), this, SLOT(onDeleteRowClicked()));
            cellLay->addWidget(del);
            m_roiTable->setCellWidget(i, delCol, cell);
        }
        m_roiTable->resizeColumnsToContents();
        if (cols > 0) {
            m_roiTable->horizontalHeader()->setStretchLastSection(false);
        }
    } else {
        QStringList hh;
        hh << T("序号") << T("类型") << T("X") << T("Y") << T("A") << T("B") << T("角度");
        const int cols = hh.size();
        m_roiTable->setColumnCount(cols);
        m_roiTable->setHorizontalHeaderLabels(hh);
        for (int i = 0; i < static_cast<int>(rs.size()); ++i) {
            const HsCommData::HsObject& r = rs[static_cast<size_t>(i)];
            double x = 0, y = 0, a = 0, b = 0, phi = 0;
            roiNumbers(r, &x, &y, &a, &b, &phi, 0);
            m_roiTable->setItem(i, 0, roiCell(QString::number(i + 1), true));
            m_roiTable->setItem(i, 1, roiCell(QString::fromUtf8(roiKindTitle(roiKindOf(r)).c_str()), false));
            m_roiTable->setItem(i, 2, roiNum(x, true));
            m_roiTable->setItem(i, 3, roiNum(y, true));
            m_roiTable->setItem(i, 4, roiNum(a, true));
            m_roiTable->setItem(i, 5, roiNum(b, b != 0.0));
            const bool hasPhi = (roiKindOf(r) == static_cast<int>(HsCommData::ObjRect2)
                || roiKindOf(r) == static_cast<int>(HsCommData::ObjEllipse));
            m_roiTable->setItem(i, 6, roiNum(HsCommData::toDegree(phi), hasPhi));
        }
        m_roiTable->resizeColumnsToContents();
        if (cols > 0) {
            m_roiTable->horizontalHeader()->setStretchLastSection(true);
        }
    }

    if (keep >= 0 && keep < m_roiTable->rowCount()) {
        m_roiTable->selectRow(keep);
    } else {
        m_roiTable->clearSelection();
    }
    m_pressSelectedRow = -1;
    if (m_roiTable->selectionModel()) {
        m_roiTable->selectionModel()->blockSignals(false);
    }
    m_roiTable->blockSignals(false);
    m_fillingRoiTable = false;
}

// 
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
    const std::vector<HsCommData::HsObject> rs = tool->effectiveRois();
    for (int i = 0; i < static_cast<int>(rs.size()); ++i) {
        if (onlyIndex >= 0 && i != onlyIndex) {
            continue;
        }
        const HsCommData::HsObject& r = rs[static_cast<size_t>(i)];
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
