/**
 * @file ToolDebugHost.cpp
 * @brief 工具详细调试窗实现：四宫格布局、ROI 绘制与撤销、draft 试跑与提交
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolDebugHost.h"
#include <image_widget/image_draw_pad.h>
#include "DraftRegistry.h"
#include "ToolEditorBase.h"
#include "ToolEditorRegistry.h"
#include "ToolParamUi.h"
#include "ToolIoPanel.h"
#include "Job.h"
#include "ToolBase.h"
#include "ThreadPool.h"
#include "Perm.h"
#include "RoiConv.h"
#include "Session.h"
#include "Theme.h"
#include "UiText.h"
#include "QtConv.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <HsCtrl/HsImageWindow.h>
#include <HsCtrl/HsViewShell.h>
#include "CloudViewBinder.h"
#include "ToolIo.h"
#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopWidget>
#include <QEvent>
#include <QFile>
#include <QRect>
#include <QSize>
#include <QTextStream>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QAbstractButton>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPalette>
#include <QColor>
#include <QTextCursor>
#include <QShowEvent>
#include <QDateTime>
#include <QToolButton>
#include <QWindow>
#include <QVBoxLayout>

namespace Vision {

static const int kMaxRoiUndo = 32;


// 编号卡片：序号徽章 + 标题 + 内容；expandBody=false 时内容顶对齐、下方留白
static QFrame* makeDebugCard(const QString& badge, const QString& title, QWidget* body,
                             QWidget* parent, bool expandBody)
{
    QFrame* card = new QFrame(parent);
    card->setObjectName(QLatin1String("toolDebugCard"));
    card->setFrameShape(QFrame::NoFrame);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout* lay = new QVBoxLayout(card);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    QFrame* titleBar = new QFrame(card);
    titleBar->setObjectName(QLatin1String("toolDebugCardTitleBar"));
    QHBoxLayout* titleLay = new QHBoxLayout(titleBar);
    titleLay->setContentsMargins(10, 8, 10, 8);
    titleLay->setSpacing(8);
    QLabel* num = new QLabel(badge, titleBar);
    num->setObjectName(QLatin1String("toolDebugCardBadge"));
    num->setAlignment(Qt::AlignCenter);
    QLabel* head = new QLabel(title, titleBar);
    head->setObjectName(QLatin1String("toolDebugCardTitle"));
    titleLay->addWidget(num);
    titleLay->addWidget(head, 1);
    lay->addWidget(titleBar);

    QWidget* wrap = new QWidget(card);
    QVBoxLayout* inner = new QVBoxLayout(wrap);
    inner->setContentsMargins(10, 8, 10, 10);
    inner->setSpacing(6);
    if (expandBody) {
        inner->addWidget(body, 1);
    } else {
        // 参数/输入：内容按自身高度顶对齐，空白留给 stretch
        body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        inner->addWidget(body, 0, Qt::AlignTop);
        inner->addStretch(1);
    }
    lay->addWidget(wrap, 1);
    return card;
}

// 更新卡片序号徽章
static void setCardBadge(QFrame* card, const QString& badge)
{
    if (!card) {
        return;
    }
    if (QLabel* num = card->findChild<QLabel*>(QLatin1String("toolDebugCardBadge"))) {
        num->setText(badge);
    }
}

// 临时诊断：对比 Qt 侧几何与原生窗口矩形
static void dumpWindowGeometry(QWidget* root, const QString& tag, const QString& when)
{
    QFile f(QCoreApplication::applicationDirPath() + QLatin1String("/layout_debug.txt"));
    if (!root || !f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    ts << "geom " << tag << " " << when
       << "  qt=" << root->width() << "x" << root->height()
       << " hint=" << root->sizeHint().width() << "x" << root->sizeHint().height()
       << " minHint=" << root->minimumSizeHint().height()
       << " dpr=" << root->devicePixelRatioF();
    if (QWindow* win = root->windowHandle()) {
        ts << " win=" << win->width() << "x" << win->height();
    }
#ifdef _WIN32
    if (root->internalWinId()) {
        HWND hwnd = reinterpret_cast<HWND>(root->internalWinId());
        RECT wr;
        RECT cr;
        if (GetWindowRect(hwnd, &wr) && GetClientRect(hwnd, &cr)) {
            ts << " native=" << (wr.right - wr.left) << "x" << (wr.bottom - wr.top)
               << " client=" << cr.right << "x" << cr.bottom;
        }
    }
#endif
    ts << "\n";
}

// 调试窗是否已有本工具输入图（有则试跑只跑本工具）
static bool toolHasDebugInput(JobCfg* jobCfg, Job* job, const ObjectId& toolId)
{
    if (!jobCfg || !job || toolId.isNull()) {
        return false;
    }
    return !job->cachedInputImage(*jobCfg, toolId).isNull();
}

// 详细调试窗：图像、参数、连接与 ROI
ToolDebugHost::ToolDebugHost(ToolCfg* tool, Session* session, Solution* sol, Job* job,
                             QWidget* parent)
    : QDialog(parent)
    , m_shell(nullptr)
    , m_view(nullptr)
    , m_drawPad(nullptr)
    , m_editor(nullptr)
    , m_io(nullptr)
    , m_quadPanel(nullptr)
    , m_quadGrid(nullptr)
    , m_imageWrap(nullptr)
    , m_linkCard(nullptr)
    , m_roiCard(nullptr)
    , m_paramCard(nullptr)
    , m_log(nullptr)
    , m_formalTool(tool)
    , m_boundId(tool ? tool->id : ObjectId())
    , m_tool(nullptr)
    , m_session(session)
    , m_sol(sol)
    , m_job(job)
    , m_drawTools(nullptr)
    , m_drawBar(nullptr)
    , m_drawPanel(nullptr)
    , m_drawModeWrap(nullptr)
    , m_chkDrawMode(nullptr)
    , m_commitToSolution(true)
    , m_pendingNewRoi(false)
    , m_syncingRoiSelection(false)
    , m_runBusy(false)
{
    // 尺寸必须在建子控件之前定好：原生窗口可能在构造途中就被创建
    // 那时若还没定尺寸，窗口会按布局建议尺寸开出来，之后 Qt 侧再改也同步不回
    setMinimumSize(760, 480);
    resize(preferredDebugSize(this));

    m_quadEmpty[0] = 0;
    m_quadEmpty[1] = 0;
    if (m_formalTool) {
        m_draftView = *m_formalTool;
        m_draftView.migrateRuntime();
        m_formalSnapshot.loadFrom(*m_formalTool);
        DraftRegistry::registerToolDraft(m_formalTool->id);
    }
    m_tool = &m_draftView;
    updateWindowTitle();
    applyFramelessDialog(this);
    enableDialogResize(this);

    m_io = new ToolIoPanel(this);
    connect(m_io, SIGNAL(clearRoiRequested()), this, SLOT(onClearRoi()));
    connect(m_io, SIGNAL(addRoiRequested()), this, SLOT(onAddRoi()));
    connect(m_io, SIGNAL(removeLastRoiRequested()), this, SLOT(onDelLastRoi()));
    connect(m_io, SIGNAL(removeRoiRequested(int)), this, SLOT(onRemoveRoi(int)));
    connect(m_io, SIGNAL(roiSelectionChanged(int)), this, SLOT(onRoiSelectionChanged(int)));
    connect(m_io, SIGNAL(inputLinkChanged(int,QString,QString)),
            this, SLOT(onInputLinkChanged(int,QString,QString)));

    m_editor = ToolEditorRegistry::instance().create(
        m_tool ? m_tool->toolName : std::string(), this);
    QWidget* paramBody = m_editor;
    if (m_editor) {
        m_editor->bind(m_tool, session, sol);
        finalizeParamEditorLayout(m_editor);
        connect(m_editor, SIGNAL(edited()), this, SLOT(onEditorEdited()));
    } else {
        paramBody = new QWidget(this);
        paramBody->setObjectName(QLatin1String("toolDebugParamBody"));
    }

    QFrame* linkCard = makeDebugCard(QLatin1String("1"), T("输入"),
                                     m_io->pageWidget(ToolIoPage::Link), this, false);
    linkCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_linkCard = linkCard;

    m_shell = new HsCtrl::HsViewShell(this);
    m_shell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_view = m_shell->imageWindow();
    m_view->setWindowMode(HsCtrl::WindowDesign);
    connect(m_view, SIGNAL(roiCommitted(HsCommData::Graphic)), this, SLOT(onRoi(HsCommData::Graphic)));
    connect(m_view, SIGNAL(roiSelected(HsCommData::Graphic)), this, SLOT(onDrawingSelected(HsCommData::Graphic)));

    m_drawPad = new image_widget::ImageDrawPad(this);
    m_drawPad->setImageWindow(m_view);
    m_drawPad->hide();


    m_chkDrawMode = new QCheckBox(QString(), this);
    m_chkDrawMode->setObjectName(QLatin1String("toolDebugDrawSwitch"));
    m_chkDrawMode->setToolTip(T("开启后显示绘制工具，可连续添加 ROI；关闭则仅浏览与查看结果。"));
    connect(m_chkDrawMode, SIGNAL(toggled(bool)), this, SLOT(onDrawModeChanged()));

    m_drawModeWrap = new QWidget(this);
    QHBoxLayout* drawModeLay = new QHBoxLayout(m_drawModeWrap);
    drawModeLay->setContentsMargins(0, 0, 0, 0);
    drawModeLay->setSpacing(6);
    QLabel* drawModeLabel = new QLabel(T("绘制模式"), m_drawModeWrap);
    drawModeLabel->setObjectName(QLatin1String("toolDebugDrawLabel"));
    drawModeLay->addWidget(drawModeLabel);
    drawModeLay->addWidget(m_chkDrawMode);
    drawModeLay->addStretch();


    QWidget* roiBody = new QWidget(this);
    QVBoxLayout* roiBodyLay = new QVBoxLayout(roiBody);
    roiBodyLay->setContentsMargins(0, 0, 0, 0);
    roiBodyLay->setSpacing(8);
    QHBoxLayout* roiToolLay = new QHBoxLayout();
    roiToolLay->setContentsMargins(0, 0, 0, 0);
    roiToolLay->setSpacing(8);
    roiToolLay->addWidget(m_drawModeWrap);
    roiBodyLay->addLayout(roiToolLay);
    roiBodyLay->addWidget(m_io->pageWidget(ToolIoPage::Roi), 1);

    m_roiCard = makeDebugCard(QLatin1String("2"), T("ROI"), roiBody, this, true);
    m_paramCard = makeDebugCard(QLatin1String("3"), T("参数"), paramBody, this, false);
    m_roiCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_paramCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_imageWrap = new QFrame(this);
    m_imageWrap->setObjectName(QLatin1String("toolDebugImageWrap"));
    QVBoxLayout* imageLay = new QVBoxLayout(m_imageWrap);
    imageLay->setContentsMargins(6, 6, 6, 6);
    imageLay->setSpacing(0);
    imageLay->addWidget(m_shell, 1);
    m_imageWrap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_quadPanel = new QWidget(this);
    m_quadPanel->setObjectName(QLatin1String("toolDebugQuad"));
    m_quadGrid = new QGridLayout(m_quadPanel);
    m_quadGrid->setContentsMargins(0, 0, 0, 0);
    m_quadGrid->setSpacing(10);
    m_quadGrid->setRowStretch(0, 1);
    m_quadGrid->setRowStretch(1, 1);
    m_quadGrid->setColumnStretch(0, 1);
    m_quadGrid->setColumnStretch(1, 1);
    for (int i = 0; i < 2; ++i) {
        m_quadEmpty[i] = new QWidget(m_quadPanel);
        m_quadEmpty[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    m_log = new QPlainTextEdit(this);
    m_log->setObjectName(QLatin1String("toolDebugLog"));
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(200);
    m_log->setMinimumHeight(52);
    // 与右侧两行按钮等高
    m_log->setMaximumHeight(84);

    QString toolTitle = m_tool && !m_tool->name.empty() ? qs(m_tool->name) : T("工具");
    QString toolKindTitle = toolTitle;
    if (m_tool && !m_tool->toolName.empty()) {
        if (ToolBase* reg = ToolRegistry::instance().find(m_tool->toolName)) {
            toolKindTitle = qs(reg->displayName());
            if (!toolKindTitle.endsWith(T("工具"))) {
                toolKindTitle += T("工具");
            }
        }
    }
    QFrame* header = makeCvXTitleBar(this, this, toolKindTitle, toolTitle, true, 0);

    QPushButton* cancel = new QPushButton(T("取消"), this);
    connect(cancel, SIGNAL(clicked()), this, SLOT(onCancel()));
    QPushButton* save = new QPushButton(T("保存"), this);
    save->setObjectName(QLatin1String("toolDebugSaveBtn"));
    connect(save, SIGNAL(clicked()), this, SLOT(onSave()));
    QPushButton* run = new QPushButton(T("运行本工具"), this);
    run->setObjectName(QLatin1String("toolDebugRunBtn"));
    setButtonRole(run, "primary");
    run->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(run, SIGNAL(clicked()), this, SLOT(onRun()));

    // 按钮分两行：上行取消 + 保存，下行单独放运行
    QHBoxLayout* actTopLay = new QHBoxLayout();
    actTopLay->setContentsMargins(0, 0, 0, 0);
    actTopLay->setSpacing(8);
    actTopLay->addWidget(cancel, 1);
    actTopLay->addWidget(save, 1);

    QVBoxLayout* actLay = new QVBoxLayout();
    actLay->setContentsMargins(0, 0, 0, 0);
    actLay->setSpacing(8);
    actLay->addLayout(actTopLay);
    actLay->addWidget(run);

    QFrame* footer = new QFrame(this);
    footer->setObjectName(QLatin1String("toolDebugFooter"));
    QHBoxLayout* footLay = new QHBoxLayout(footer);
    footLay->setContentsMargins(12, 8, 12, 8);
    footLay->setSpacing(12);
    footLay->addWidget(m_log, 1);
    footLay->addLayout(actLay);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->setSizeConstraint(QLayout::SetNoConstraint);
    root->addWidget(header);

    QWidget* body = new QWidget(this);
    body->setObjectName(QLatin1String("toolDebugBody"));
    QVBoxLayout* bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(12, 8, 12, 8);
    bodyLay->setSpacing(10);
    bodyLay->addWidget(m_quadPanel, 1);
    root->addWidget(body, 1);
    root->addWidget(footer);

    rebuildQuadLayout();
    updateDrawShapeTools();
    refreshIo();
    applyDrawings();
    updateDrawBarVisibility();
    refreshView();
    syncDrawingInteractionView();
    warnRoiKindMismatch();
    appendDebugLog(T("调试窗口已打开：") + toolTitle);
    setAttribute(Qt::WA_Resized, true);
    resize(preferredDebugSize(this));
}

// 打开尺寸：按 1120x720 起，但不超过屏幕可用区，保证底栏按钮不被挤出屏幕
QSize ToolDebugHost::preferredDebugSize(const QWidget* ref)
{
    QRect avail;
    if (QDesktopWidget* desk = QApplication::desktop()) {
        avail = ref ? desk->availableGeometry(const_cast<QWidget*>(ref))
                    : desk->availableGeometry();
    }
    int w = 1120;
    int h = 720;
    if (avail.width() > 0 && avail.height() > 0) {
        w = qMin(w, avail.width() - 80);
        h = qMin(h, avail.height() - 80);
    }
    return QSize(qMax(760, w), qMax(480, h));
}

// 建议尺寸固定为打开尺寸，拦住按 sizeHint 累加撑大窗口的路径
QSize ToolDebugHost::sizeHint() const
{
    return preferredDebugSize(this);
}

// 最小尺寸不跟图像窗/布局内容走，避免打开时被撑高
QSize ToolDebugHost::minimumSizeHint() const
{
    return QSize(760, 480);
}

// 窗口高于屏幕可用区时收回来，并挪进屏幕内
void ToolDebugHost::clampToScreen()
{
    QDesktopWidget* desk = QApplication::desktop();
    if (!desk) {
        return;
    }
    const QRect avail = desk->availableGeometry(this);
    if (avail.width() <= 0 || avail.height() <= 0) {
        return;
    }
    const int w = qMin(width(), avail.width());
    const int h = qMin(height(), avail.height());
    if (w != width() || h != height()) {
        resize(w, h);
    }
    QRect g = frameGeometry();
    if (g.bottom() > avail.bottom()) {
        g.moveBottom(avail.bottom());
    }
    if (g.top() < avail.top()) {
        g.moveTop(avail.top());
    }
    if (g.right() > avail.right()) {
        g.moveRight(avail.right());
    }
    if (g.left() < avail.left()) {
        g.moveLeft(avail.left());
    }
    if (g.topLeft() != frameGeometry().topLeft()) {
        move(g.topLeft());
    }
}

// 追加一行带时间戳的调试日志
void ToolDebugHost::appendDebugLog(const QString& line)
{
    if (!m_log || line.isEmpty()) {
        return;
    }
    const QString stamp = QDateTime::currentDateTime().toString(QLatin1String("hh:mm:ss"));
    m_log->appendPlainText(stamp + QLatin1String("  ") + line);
    QTextCursor cur = m_log->textCursor();
    cur.movePosition(QTextCursor::End);
    m_log->setTextCursor(cur);
}

// 按工具能力重排四宫格
void ToolDebugHost::rebuildQuadLayout()
{
    if (!m_quadGrid) {
        return;
    }
    while (QLayoutItem* it = m_quadGrid->takeAt(0)) {
        if (it) {
            delete it;
        }
    }
    const bool roiOn = m_tool && m_tool->usesRoi();
    const bool paramOn = m_editor != 0;
    if (m_drawModeWrap) {
        m_drawModeWrap->setVisible(roiOn);
    }
    if (!roiOn && m_chkDrawMode && m_chkDrawMode->isChecked()) {
        m_chkDrawMode->setChecked(false);
    }

    QWidget* order[4];
    int count = 0;
    order[count++] = m_imageWrap;
    order[count++] = m_linkCard;
    if (roiOn) {
        order[count++] = m_roiCard;
    }
    if (paramOn) {
        order[count++] = m_paramCard;
    }
    // 没进四宫格的卡片必须隐藏，否则会成为脱离布局的游离控件
    if (m_roiCard && !roiOn) {
        m_roiCard->hide();
    }
    if (m_paramCard && !paramOn) {
        m_paramCard->hide();
    }

    int emptyIdx = 0;
    int badge = 1;
    for (int i = 0; i < 4; ++i) {
        QWidget* cell = nullptr;
        if (i < count) {
            cell = order[i];
            cell->show();
            if (cell == m_linkCard) {
                setCardBadge(m_linkCard, QString::number(badge++));
            } else if (cell == m_roiCard) {
                setCardBadge(m_roiCard, QString::number(badge++));
            } else if (cell == m_paramCard) {
                setCardBadge(m_paramCard, QString::number(badge++));
            }
        } else if (emptyIdx < 2) {
            cell = m_quadEmpty[emptyIdx++];
            cell->show();
        }
        if (cell) {
            m_quadGrid->addWidget(cell, i / 2, i % 2);
        }
    }
    for (int i = emptyIdx; i < 2; ++i) {
        if (m_quadEmpty[i]) {
            m_quadEmpty[i]->hide();
        }
    }
    updateDrawBarVisibility();
}

// 按工具能力显示 ROI 卡片并重排四宫格
void ToolDebugHost::updateRoiCardVisibility()
{
    rebuildQuadLayout();
}

// 按工具推荐种类显隐形状按钮（避免一排全露出）
void ToolDebugHost::updateDrawShapeTools()
{
    ToolCaps caps;
    if (m_tool) {
        caps = m_tool->caps();
    }
    const int buttonIds[] = {
        static_cast<int>(HsCtrl::ToolBrowse),
        static_cast<int>(HsCtrl::ToolCircle),
        static_cast<int>(HsCtrl::ToolRect1),
        static_cast<int>(HsCtrl::ToolRect2),
        static_cast<int>(HsCtrl::ToolEllipse),
        static_cast<int>(HsCtrl::ToolLine),
        static_cast<int>(HsCtrl::ToolContour),
        static_cast<int>(HsCtrl::ToolPolygon),
    };
    const int objectKinds[] = {
        -1,
        static_cast<int>(HsCommData::ObjCircle),
        static_cast<int>(HsCommData::ObjRect1),
        static_cast<int>(HsCommData::ObjRect2),
        static_cast<int>(HsCommData::ObjEllipse),
        static_cast<int>(HsCommData::ObjLine),
        static_cast<int>(HsCommData::ObjContour),
        static_cast<int>(HsCommData::ObjRegion),
    };
    // 浏览始终显示；形状按钮要求某个槽既接受对应条目种类（多边形为区域，其余为 ROI）又允许该形状
    const int n = static_cast<int>(sizeof(buttonIds) / sizeof(buttonIds[0]));
    std::vector<HsCtrl::DrawTool> padTools;
    for (int i = 0; i < n; ++i) {
        bool show = buttonIds[i] == static_cast<int>(HsCtrl::ToolBrowse);
        if (!show && m_tool && caps.usesRoi) {
            const RoiItemKind need = (objectKinds[i] == static_cast<int>(HsCommData::ObjRegion))
                ? RoiItemKind::DrawRegion : RoiItemKind::DrawRoi;
            for (size_t s = 0; s < caps.slotRules.size() && !show; ++s) {
                const RoiSlotRule& slot = caps.slotRules[s];
                bool kindOk = false;
                for (size_t k = 0; k < slot.itemKinds.size(); ++k) {
                    if (slot.itemKinds[k] == need) {
                        kindOk = true;
                    }
                }
                if (!kindOk) {
                    continue;
                }
                for (size_t h = 0; h < slot.shapes.size(); ++h) {
                    if (static_cast<int>(slot.shapes[h].kind) == objectKinds[i]) {
                        show = true;
                    }
                }
            }
        }
        if (show) {
            padTools.push_back(static_cast<HsCtrl::DrawTool>(buttonIds[i]));
        }
    }
    if (m_drawPad) {
        m_drawPad->setTools(padTools);
    }
}

// 绘制模式开/关时显隐形状栏
void ToolDebugHost::updateDrawBarVisibility()
{
    const bool roiOn = m_tool && m_tool->usesRoi();
    const bool on = roiOn && isDrawMode();
    if (m_drawPad) {
        if (on && isVisible()) {
            m_drawPad->popup(m_view);
        } else {
            if (m_view && m_view->drawSessionActive()) {
                m_view->cancelDrawSession();
            }
            m_drawPad->hide();
        }
    }
    if (!on && m_view) {
        m_view->setDrawTool(HsCtrl::ToolBrowse);
    }
}

// Ctrl+Z 撤销 ROI
void ToolDebugHost::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Undo)) {
        undoRoi();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

// 修改 ROI 前压栈
void ToolDebugHost::pushRoiUndo()
{
    if (!m_tool) {
        return;
    }
    RoiSnapshot snap;
    snap.rois = m_tool->rois;
    snap.drawStyles = m_drawStyles;
    snap.drawStyleOn = m_drawStyleOn;
    m_roiUndo.append(snap);
    while (m_roiUndo.size() > kMaxRoiUndo) {
        m_roiUndo.removeFirst();
    }
}

// 撤销上一步 ROI
void ToolDebugHost::undoRoi()
{
    if (m_roiUndo.isEmpty() || !m_tool) {
        return;
    }
    const RoiSnapshot snap = m_roiUndo.takeLast();
    m_tool->rois = snap.rois;
    m_tool->syncRois();
    m_drawStyles = snap.drawStyles;
    m_drawStyleOn = snap.drawStyleOn;
    m_pendingNewRoi = false;
    if (m_view) {
    }
    markDraftDirty();
    applyDrawings();
    emit roiChanged();
}

// ROI 种类与工具不匹配时提示
void ToolDebugHost::warnRoiKindMismatch()
{
    if (!m_tool || !m_tool->usesRoi()) {
        return;
    }
    const std::vector<HsCommData::HsObject> rs = m_tool->effectiveRois();
    for (size_t i = 0; i < rs.size(); ++i) {
        if (!rs[i].isInitialized() || rs[i].isEmpty() || !m_tool->roiKindMatches(static_cast<int>(rs[i].kind()))) {
            QMessageBox::warning(
                this,
                T("ROI 类型"),
                T("当前 ROI 类型与工具推荐不一致（%1 建议使用 %2）。请重画或切换绘制工具。")
                    .arg(qs(m_tool->name))
                    .arg(QString::fromUtf8(
                        roiKindTitle(m_tool->preferredRoiKind()).c_str())));
            return;
        }
    }
}

// 删除 ROI 前确认
bool ToolDebugHost::confirmRoiDelete(const QString& message) const
{
    const QMessageBox::StandardButton btn = QMessageBox::question(
        const_cast<ToolDebugHost*>(this),
        T("删除 ROI"),
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    return btn == QMessageBox::Yes;
}

// 页内试跑用的 draft 视图
const ToolCfg* ToolDebugHost::draftTool() const
{
    return m_formalTool ? &m_draftView : 0;
}

// 保存时是否标记 Solution::dirty
void ToolDebugHost::setSolutionCommit(bool commit)
{
    m_commitToSolution = commit;
}

// 首次显示后按实际布局重算参数页高度，并把窗口收进屏幕
void ToolDebugHost::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_editor) {
        finalizeParamEditorLayout(m_editor);
    }
    resize(preferredDebugSize(this));
    setAttribute(Qt::WA_Resized, true);
    clampToScreen();
    updateDrawBarVisibility();
    dumpWindowGeometry(this, m_tool ? qs(m_tool->name) : QString(), QLatin1String("show"));
}

// 绘制小窗跟着调试窗，贴在图像左上
void ToolDebugHost::moveEvent(QMoveEvent* event)
{
    QDialog::moveEvent(event);
    placeDrawPad();
}

// 调试窗变大时绘制小窗仍贴着图像
void ToolDebugHost::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    placeDrawPad();
}

// 把绘制小窗摆到图像上方
void ToolDebugHost::placeDrawPad()
{
    if (!m_drawPad || !m_view || !m_drawPad->isVisible()) {
        return;
    }
    if (m_drawPad->property("userMoved").toBool()) {
        return;
    }
    m_drawPad->popup(m_view);
}

// 关闭前：取消=留在窗内，不保存=丢弃并关，已保存=保存后关
void ToolDebugHost::closeEvent(QCloseEvent* event)
{
    const int action = askCloseAction();
    if (action == 0) {
        event->ignore();
        return;
    }
    if (m_drawPad) {
        m_drawPad->hide();
    }
    // 等试跑线程结束，再关窗（避免回调已销毁的 this）
    if (m_runBusy) {
        ThreadPool::shared().waitIdle();
        QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    }
    event->accept();
    if (action == 2) {
        accept();
    } else {
        reject();
    }
}

// 把参数页写回 draft
void ToolDebugHost::applyParams()
{
    if (m_editor) {
        m_editor->apply();
    }
}

// 页内试跑：投递 C++ 线程池，结果 Queued 回 UI
void ToolDebugHost::onRun()
{
    if (!m_formalTool || !m_job || !m_sol || !m_tool) {
        return;
    }
    if (m_runBusy) {
        appendDebugLog(T("试跑进行中，请稍候。"));
        return;
    }
    if (m_job->isRunning()) {
        appendDebugLog(T("流程正在执行，请稍候。"));
        return;
    }
    applyParams();
    appendDebugLog(T("开始运行。"));

    struct Pack {
        ToolCfg draft;
        JobCfg recipe;
        bool useRecipe;
        bool useToolOnly;
        ObjectId toolId;
    };
    Pack* pack = new Pack();
    pack->draft = m_draftView;
    pack->useRecipe = false;
    pack->useToolOnly = false;
    pack->toolId = m_formalTool->id;

    // 复检强制图：复制整条流程配方，取图工具改读该文件，本工具换成 draft；
    // 否则取图工具或已有调试输入时只跑本工具，其余跑到本工具为止的链
    JobCfg* jobCfg = m_sol->findJobCfgOfTool(m_formalTool->id, 0);
    if (!m_forcedImagePath.empty() && jobCfg) {
        pack->recipe = *jobCfg;
        for (size_t i = 0; i < pack->recipe.tools.size(); ++i) {
            ToolCfg& t = pack->recipe.tools[i];
            if (t.toolName == "camera.file" || t.type == ToolType::Acquire) {
                t.params.insert("path", m_forcedImagePath);
                t.params.insert("sourceMode", std::string("file"));
            }
            if (t.id == m_formalTool->id) {
                t = pack->draft;
            }
        }
        pack->useRecipe = true;
    } else {
        const bool cam = (m_tool->type == ToolType::Acquire || m_tool->type == ToolType::Acquire3D);
        pack->useToolOnly = cam || (jobCfg && toolHasDebugInput(jobCfg, m_job, m_formalTool->id));
    }

    // 线程池里跑，结果用堆上 ToolOut 排队回 onAsyncRunDone
    m_runBusy = true;
    Job* job = m_job;
    Solution* sol = m_sol;
    ThreadPool::shared().enqueue([this, job, sol, pack]() {
        ToolOut out;
        if (pack->useRecipe) {
            out = job->runToolChain(sol, pack->toolId, &pack->draft, &pack->recipe);
        } else if (pack->useToolOnly) {
            out = job->runTool(sol, pack->toolId, &pack->draft);
        } else {
            out = job->runToolChain(sol, pack->toolId, &pack->draft);
        }
        Pack* held = pack;
        if (held) {
            delete held;
            held = nullptr;
        }
        ToolOut* heap = new ToolOut(out);
        QMetaObject::invokeMethod(this, "onAsyncRunDone", Qt::QueuedConnection,
                                  Q_ARG(void*, heap));
    });
}

// 试跑结果回 UI
void ToolDebugHost::onAsyncRunDone(void* outPtr)
{
    m_runBusy = false;
    ToolOut* out = static_cast<ToolOut*>(outPtr);
    if (!out) {
        return;
    }
    refreshView(out);
    QString runLine = (out->ok ? T("OK") : T("NG")) + QLatin1String("  ")
                      + QString::number(out->elapsedMs) + QLatin1String(" ms");
    if (!out->message.empty()) {
        runLine += QLatin1String("  ") + qs(out->message);
    }
    appendDebugLog(runLine);
    emit testRunFinished(*out);
    if (out) {
        delete out;
        out = nullptr;
    }
}

// 复检跳转：强制单文件图路径
void ToolDebugHost::applyForcedImageFile(const std::string& imagePath)
{
    m_forcedImagePath = imagePath;
    if (imagePath.empty() || !m_tool) {
        return;
    }
    if (m_tool->toolName == "camera.file" || m_tool->type == ToolType::Acquire) {
        m_tool->params.insert("path", imagePath);
        m_tool->params.insert("sourceMode", std::string("file"));
        markDraftDirty();
        if (m_editor) {
            m_editor->bind(m_tool, m_session, m_sol);
        }
        refreshIo();
    }
    appendDebugLog(T("已载入复检图：") + qs(imagePath));
    refreshView();
}

// 保存 draft，调试窗保持打开
void ToolDebugHost::onSave()
{
    trySave();
}

// 取消：未保存时三选一，否则直接关
void ToolDebugHost::onCancel()
{
    const int action = askCloseAction();
    if (action == 0) {
        return;
    }
    if (action == 2) {
        accept();
    } else {
        reject();
    }
}

// 提交 draft 写回 formal
void ToolDebugHost::commitDraft()
{
    if (!m_formalTool || !m_sol) {
        return;
    }
    applyParams();
    ToolRecipe recipe;
    recipe.loadFrom(m_draftView);
    const bool changed = !recipe.equals(m_formalSnapshot);
    recipe.commitTo(m_formalTool);
    m_formalTool->runtime = m_draftView.runtime;
    m_formalSnapshot = recipe;
    if (changed && m_commitToSolution && m_sol) {
        m_sol->dirty = true;
    }
    DraftRegistry::setToolDraftDirty(m_formalTool->id, false);
    updateWindowTitle();
    m_roiUndo.clear();
    emit draftSaved();
}

// 更新 draft 肮标记
void ToolDebugHost::markDraftDirty()
{
    if (!m_formalTool) {
        return;
    }
    ToolRecipe cur;
    cur.loadFrom(m_draftView);
    DraftRegistry::setToolDraftDirty(m_formalTool->id, !cur.equals(m_formalSnapshot));
    updateWindowTitle();
}

// 刷新标题
void ToolDebugHost::updateWindowTitle()
{
    QString title = T("详细调试");
    if (m_tool && !m_tool->name.empty()) {
        title = qs(m_tool->name) + QLatin1Char(' ') + QString(QChar(0x00B7)) + QLatin1Char(' ') + title;
        ToolBase* reg = ToolRegistry::instance().find(m_tool->toolName);
        if (reg) {
            title += QLatin1String("  ") + qs(m_tool->toolName) + QLatin1Char('@') + qs(reg->version());
        }
    }
    if (m_formalTool && DraftRegistry::toolDraftBlocksFormalRun(m_formalTool->id)) {
        title += T(" · 未保存");
    }
    setWindowTitle(title);
}

// 写回 formal 并刷新标题，不关闭
bool ToolDebugHost::trySave()
{
    if (!m_formalTool || !m_sol || !m_session->has(PermEditSolution)) {
        return false;
    }
    applyParams();
    commitDraft();
    appendDebugLog(T("已保存"));
    return true;
}

// 关窗前三选一：0 取消，1 不保存并关，2 保存并关
int ToolDebugHost::askCloseAction()
{
    if (m_boundId.isNull() || !DraftRegistry::toolDraftBlocksFormalRun(m_boundId)) {
        if (!m_boundId.isNull()) {
            DraftRegistry::unregisterToolDraft(m_boundId);
        }
        return 1;
    }
    QMessageBox box(QMessageBox::Question,
                    T("未保存的修改"),
                    T("有未保存的修改，是否保存？"),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                    this);
    box.setDefaultButton(QMessageBox::Yes);
    if (QAbstractButton* saveBtn = box.button(QMessageBox::Yes)) {
        saveBtn->setText(T("保存"));
    }
    if (QAbstractButton* discardBtn = box.button(QMessageBox::No)) {
        discardBtn->setText(T("不保存"));
    }
    if (QAbstractButton* cancelBtn = box.button(QMessageBox::Cancel)) {
        cancelBtn->setText(T("取消"));
    }
    const int r = box.exec();
    if (r == QMessageBox::Cancel) {
        return 0;
    }
    if (r == QMessageBox::Yes) {
        if (!trySave()) {
            return 0;
        }
        DraftRegistry::unregisterToolDraft(m_boundId);
        return 2;
    }
    DraftRegistry::unregisterToolDraft(m_boundId);
    return 1;
}

// 切换绘制工具
void ToolDebugHost::onDrawTool(int id)
{
    m_view->setDrawTool(static_cast<HsCtrl::DrawTool>(id));
    if (id == static_cast<int>(HsCtrl::ToolBrowse)) {
        m_pendingNewRoi = false;
    } else if (m_pendingNewRoi || (m_io && m_io->selectedRoiRow() < 0)) {
    } else {
    }
    syncDrawingInteractionView();
}

// 输入下拉变更连线
void ToolDebugHost::onInputLinkChanged(int portData, const QString& fromId, const QString& fromPort)
{
    if (!m_formalTool || !m_sol || !m_session || !m_session->has(PermEditSolution)) {
        return;
    }
    JobCfg* job = m_sol->findJobCfgOfTool(m_formalTool->id, 0);
    if (!job) {
        return;
    }
    const PortData data = static_cast<PortData>(portData);
    const PortSlot toPort = portSlotIn(data);
    if (fromId.isEmpty()) {
        job->disconnectPort(m_formalTool->id, toPort);
    } else {
        std::string err;
        if (!job->connectPorts(oid(fromId), portSlotFromKey(ss(fromPort)), m_formalTool->id, toPort, &err)) {
            QMessageBox::warning(this, T("连线"), qs(err));
            refreshIo();
            return;
        }
    }
    if (m_commitToSolution) {
        m_sol->dirty = true;
    }
    refreshIo();
    refreshView();
    if (fromId.isEmpty()) {
        appendDebugLog(T("输入已断开：") + qs(portDataTitle(data)));
    } else {
        const ToolCfg* src = job->findTool(oid(fromId));
        const QString srcName = src ? qs(src->name) : fromId;
        appendDebugLog(T("输入已连接：") + qs(portDataTitle(data)) + QLatin1String(" -> ")
                       + srcName);
    }
    emit linksChanged();
}

// 参数页改动：路径变更后按 draft 重采图
void ToolDebugHost::onEditorEdited()
{
    markDraftDirty();
    if (!m_tool || !m_job || !m_sol || !m_formalTool) {
        return;
    }
    if (m_tool->type != ToolType::Acquire && m_tool->type != ToolType::Acquire3D) {
        return;
    }
    if (!m_tool->params.value("pathChanged").toInt()) {
        return;
    }
    m_tool->params.insert("pathChanged", 0);
    const ToolOut out = m_job->runTool(m_sol, m_formalTool->id, &m_draftView);
    if (!out.image.isNull() || !out.io.clouds.empty()) {
        refreshView(&out);
    } else if (!out.message.empty()) {
        refreshIo();
    }
}

// 「绘制模式」勾选变化
void ToolDebugHost::onDrawModeChanged()
{
    updateDrawBarVisibility();
    applyDrawings(false);
}

// 表中选中 ROI 行变化
void ToolDebugHost::onRoiSelectionChanged(int index)
{
    if (index >= 0) {
        m_pendingNewRoi = false;
        if (m_view) {
        }
    } else if (m_tool && m_tool->usesMultiRoi() && isDrawMode() && m_view) {
        // 多 ROI 取消表选后仍可继续在空白处添加
        m_pendingNewRoi = true;
    }
    applyDrawings(false);
}

// 图上选中 ROI 变化，同步表格
void ToolDebugHost::onDrawingSelected(const HsCommData::Graphic& g)
{
    if (m_syncingRoiSelection || !m_io) {
        return;
    }
    if (g.id.empty()) {
        if (m_io->selectedRoiRow() >= 0) {
            m_io->clearRoiSelection();
            applyDrawings(false);
        }
        return;
    }
    const int idx = roiIndexFromDrawingId(g.id);
    if (idx < 0) {
        return;
    }
    if (m_io->selectedRoiRow() == idx) {
        return;
    }
    m_pendingNewRoi = false;
    if (m_view) {
    }
    m_io->selectRoiRow(idx);
    applyDrawings(false);
}

// 工具推荐的绘制工具 id
int ToolDebugHost::preferredDrawToolId() const
{
    if (!m_tool) {
        return static_cast<int>(HsCtrl::ToolRect1);
    }
    const ToolCaps caps = m_tool->caps();
    for (size_t s = 0; s < caps.slotRules.size(); ++s) {
        for (size_t i = 0; i < caps.slotRules[s].shapes.size(); ++i) {
            const int k = static_cast<int>(caps.slotRules[s].shapes[i].kind);
            if (k == static_cast<int>(HsCommData::ObjCircle)) {
                return static_cast<int>(HsCtrl::ToolCircle);
            }
            if (k == static_cast<int>(HsCommData::ObjLine)) {
                return static_cast<int>(HsCtrl::ToolLine);
            }
            if (k == static_cast<int>(HsCommData::ObjRect2)) {
                return static_cast<int>(HsCtrl::ToolRect2);
            }
            if (k == static_cast<int>(HsCommData::ObjEllipse)) {
                return static_cast<int>(HsCtrl::ToolEllipse);
            }
            if (k == static_cast<int>(HsCommData::ObjRegion)) {
                return static_cast<int>(HsCtrl::ToolPolygon);
            }
            if (k == static_cast<int>(HsCommData::ObjContour)) {
                return static_cast<int>(HsCtrl::ToolContour);
            }
            if (k == static_cast<int>(HsCommData::ObjRect1)) {
                return static_cast<int>(HsCtrl::ToolRect1);
            }
        }
    }
    return static_cast<int>(HsCtrl::ToolRect1);
}

// 按工具与已有 ROI 选默认形状
int ToolDebugHost::defaultDrawToolId() const
{
    if (!m_tool) {
        return static_cast<int>(HsCtrl::ToolRect1);
    }
    const std::vector<HsCommData::HsObject> rs = m_tool->effectiveRois();
    if (!rs.empty()) {
        switch (rs.back().isInitialized() && !rs.back().isEmpty() ? static_cast<int>(rs.back().kind()) : -1) {
        case static_cast<int>(HsCommData::ObjCircle):
            return static_cast<int>(HsCtrl::ToolCircle);
        case static_cast<int>(HsCommData::ObjRect2):
            return static_cast<int>(HsCtrl::ToolRect2);
        case static_cast<int>(HsCommData::ObjEllipse):
            return static_cast<int>(HsCtrl::ToolEllipse);
        case static_cast<int>(HsCommData::ObjLine):
            return static_cast<int>(HsCtrl::ToolLine);
        case static_cast<int>(HsCommData::ObjContour):
            return static_cast<int>(HsCtrl::ToolContour);
        case static_cast<int>(HsCommData::ObjRegion):
            return static_cast<int>(HsCtrl::ToolPolygon);
        default:
            break;
        }
    }
    return preferredDrawToolId();
}

// 点「新建 ROI」或「重画」
void ToolDebugHost::onAddRoi()
{
    if (!m_tool || !m_session || !m_session->has(PermEditSolution)) {
        return;
    }
    if (!m_tool->usesRoi()) {
        return;
    }
    if (m_chkDrawMode && !m_chkDrawMode->isChecked()) {
        m_chkDrawMode->setChecked(true);
    } else {
        updateDrawBarVisibility();
    }
    m_pendingNewRoi = true;
    if (m_io) {
        m_io->clearRoiSelection();
    }
    if (m_view) {
    }
    const int id = defaultDrawToolId();
    if (m_drawPad) {
        m_drawPad->startTool(static_cast<HsCtrl::DrawTool>(id));
    } else {
        onDrawTool(id);
    }
    applyDrawings(false);
}

// 是否处于连续绘制模式
bool ToolDebugHost::isDrawMode() const
{
    return m_chkDrawMode && m_chkDrawMode->isChecked();
}

// 画完 ROI 写回 draft
void ToolDebugHost::onRoi(const HsCommData::Graphic& g)
{
    if (!m_tool || !m_session || !m_session->has(PermEditSolution)) {
        return;
    }
    pushRoiUndo();
    const int idx = roiIndexFromDrawingId(g.id);
    const int before = static_cast<int>(m_tool->effectiveRois().size());
    const bool kindOk = g.geom.isInitialized() && m_tool->roiKindMatches(static_cast<int>(g.geom.kind()));
    if (idx >= 0 && idx < before) {
        m_tool->applyDrawnRoiAt(g, idx);
        if (kindOk) {
            rememberDrawStyle(idx, g.geomStyle);
        }
    } else {
        m_tool->applyDrawnRoi(g);
        const int after = static_cast<int>(m_tool->effectiveRois().size());
        if (kindOk && after > 0) {
            if (!m_tool->usesMultiRoi()) {
                m_drawStyles.clear();
                m_drawStyleOn.clear();
                rememberDrawStyle(0, g.geomStyle);
            } else if (after > before) {
                rememberDrawStyle(after - 1, g.geomStyle);
            }
        }
    }
    markDraftDirty();
    applyDrawings();
    const bool multi = m_tool->usesMultiRoi();
    if (multi) {
        // 多 ROI：保持待画状态，便于连续添加
        m_pendingNewRoi = true;
        if (m_view) {
        }
        if (m_io) {
            m_io->clearRoiSelection();
        }
    } else {
        m_pendingNewRoi = false;
        if (m_view) {
        }
        if (m_io) {
            const std::vector<HsCommData::HsObject> rs = m_tool->effectiveRois();
            if (!rs.empty()) {
                m_io->selectRoiRow(static_cast<int>(rs.size()) - 1);
            }
        }
    }
    applyDrawings(false);
    emit roiChanged();
}

// 清空 ROI
void ToolDebugHost::onClearRoi()
{
    if (!m_tool || !m_session || !m_session->has(PermEditSolution)) {
        return;
    }
    if (!confirmRoiDelete(T("确定清空全部 ROI？"))) {
        return;
    }
    pushRoiUndo();
    m_tool->clearRois();
    m_drawStyles.clear();
    m_drawStyleOn.clear();
    markDraftDirty();
    applyDrawings();
    emit roiChanged();
}

// 删除最后一个 ROI
void ToolDebugHost::onDelLastRoi()
{
    if (!m_tool || !m_session || !m_session->has(PermEditSolution)) {
        return;
    }
    if (!confirmRoiDelete(T("确定删除最后一项 ROI？"))) {
        return;
    }
    pushRoiUndo();
    const int roiCount = static_cast<int>(m_tool->effectiveRois().size());
    m_tool->removeLastRoi();
    if (roiCount > 0 && static_cast<int>(m_drawStyles.size()) == roiCount) {
        m_drawStyles.pop_back();
        m_drawStyleOn.pop_back();
    }
    markDraftDirty();
    applyDrawings();
    emit roiChanged();
}

// 删除指定行 ROI
void ToolDebugHost::onRemoveRoi(int index)
{
    if (!m_tool || !m_session || !m_session->has(PermEditSolution)) {
        return;
    }
    if (!confirmRoiDelete(T("确定删除选中的 ROI？"))) {
        return;
    }
    pushRoiUndo();
    const int roiCount = static_cast<int>(m_tool->effectiveRois().size());
    m_tool->removeRoiAt(index);
    if (index >= 0 && index < roiCount && static_cast<int>(m_drawStyles.size()) == roiCount) {
        m_drawStyles.erase(m_drawStyles.begin() + index);
        m_drawStyleOn.erase(m_drawStyleOn.begin() + index);
    }
    markDraftDirty();
    applyDrawings();
    emit roiChanged();
}

// 刷新输入摘要
void ToolDebugHost::refreshIo()
{
    if (!m_io) {
        return;
    }
    const bool canEdit = m_session && m_session->has(PermEditSolution);
    m_io->refresh(m_sol, m_tool, m_job, canEdit);
    updateRoiCardVisibility();
}

// 记住小窗提交时的画笔
void ToolDebugHost::rememberDrawStyle(int index, const HsCommData::Style& style)
{
    if (index < 0) {
        return;
    }
    if (static_cast<int>(m_drawStyles.size()) <= index) {
        m_drawStyles.resize(static_cast<size_t>(index) + 1);
        m_drawStyleOn.resize(static_cast<size_t>(index) + 1, 0);
    }
    m_drawStyles[static_cast<size_t>(index)] = style;
    m_drawStyleOn[static_cast<size_t>(index)] = 1;
}
// 把当前工具的 ROI 刷到图像上
void ToolDebugHost::applyDrawings(bool refreshTable)
{
    const int selIdx = m_io ? m_io->selectedRoiRow() : -1;
    if (m_tool) {
        std::vector<HsCommData::Graphic> shown = m_tool->drawingsForDisplay(selIdx);
        if (m_drawPad && m_drawPad->isVisible()) {
            const int n = static_cast<int>(shown.size());
            for (int i = 0; i < n && i < static_cast<int>(m_drawStyles.size()); ++i) {
                if (i < static_cast<int>(m_drawStyleOn.size()) && m_drawStyleOn[static_cast<size_t>(i)]) {
                    const double opacity = shown[static_cast<size_t>(i)].geomStyle.opacity;
                    shown[static_cast<size_t>(i)].geomStyle = m_drawStyles[static_cast<size_t>(i)];
                    shown[static_cast<size_t>(i)].geomStyle.opacity = opacity;
                }
            }
        }
        m_view->setDrawings(shown);
    } else {
        m_view->setDrawings(std::vector<HsCommData::Graphic>());
    }
    overlayRoiDetails(m_view, m_tool, selIdx, isDrawMode());
    if (refreshTable) {
        refreshIo();
    }
    syncDrawingInteractionView();
}

// 按选中行与绘制模式同步 Drawing 交互
void ToolDebugHost::syncDrawingInteractionView()
{
    if (!m_view || !m_tool) {
        return;
    }
    const std::vector<HsCommData::HsObject> rs = m_tool->effectiveRois();
    const int idx = m_io ? m_io->selectedRoiRow() : -1;
    const bool has = !rs.empty();
    m_syncingRoiSelection = true;
    syncDrawingInteraction(m_view, isDrawMode(), idx, has);
    m_syncingRoiSelection = false;
}

// 对外：只刷新 ROI
void ToolDebugHost::reloadDrawings()
{
    applyDrawings();
}

// 刷新图与 ROI；底图尺寸未变时不重置缩放
void ToolDebugHost::showResult(const HsCommData::HsImage& fb, const QVector<HsCommData::Graphic>& overlay,
                              const QVector<HsCommData::Graphic>& /*drawings*/, bool replaceBase)
{
    int64_t frameId = 0;
    if (replaceBase && !fb.isNull()) {
        const QSize prev = m_view->imageSize();
        applyFrame(m_view, fb);
        frameId = fb.frameId;
        if (!prev.isValid() || prev.width() != fb.width || prev.height() != fb.height) {
            m_view->fitToWindow();
        }
    }
    if (!overlay.isEmpty()) {
        if (frameId != 0) {
            m_view->setOverlay(toStd(overlay), m_view->currentFrameId());
        } else {
            m_view->setOverlays(toStd(overlay));
        }
    } else if (frameId != 0) {
        m_view->setOverlay(std::vector<HsCommData::Graphic>(), m_view->currentFrameId());
    } else {
        m_view->setOverlays(std::vector<HsCommData::Graphic>());
    }
    applyDrawings();
}

// 底图=本工具输入图，叠加本工具输出；跑完后始终刷新
void ToolDebugHost::refreshView(const ToolOut* runOut)
{
    if (!m_tool || !m_job || !m_view || !m_formalTool) {
        return;
    }
    JobCfg* job = m_sol ? m_sol->findJobCfgOfTool(m_formalTool->id, 0) : nullptr;
    HsCommData::HsImage base;
    if (m_tool->type == ToolType::Acquire) {
        if (runOut && !runOut->image.isNull()) {
            base = runOut->image;
        } else {
            base = debugBaseFrame(m_tool, job, m_job);
        }
    } else {
        base = debugBaseFrame(m_tool, job, m_job);
        if (base.isNull() && runOut && !runOut->image.isNull()) {
            base = runOut->image;
        }
    }
    QVector<HsCommData::Graphic> ov;
    if (runOut && !runOut->objects.empty()) {
        ov = qg(runOut->objects);
    } else {
        ov = qg(m_job->cachedOverlay(m_formalTool->id));
    }
    const QVector<HsCommData::Graphic> drawings = qg(m_tool->drawings());
    showResult(base, ov, drawings, !base.isNull());
    applyCloudResult(runOut);
}

void ToolDebugHost::applyCloudResult(const ToolOut* runOut)
{
    if (!m_shell || !runOut) {
        return;
    }
    CloudViewBinder::apply(m_shell, runOut->io, true);
}

}

#include "moc_ToolDebugHost.cpp"
