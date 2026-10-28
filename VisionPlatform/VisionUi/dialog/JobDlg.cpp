/**
 * @file JobDlg.cpp
 * @brief JobDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "JobDlg.h"
#include "DraftRegistry.h"
#include "Job.h"
#include "JobCanvas.h"
#include "JobFlowCanvasPanel.h"
#include "JobRstRuleDlg.h"
#include "JobRstRulePanel.h"
#include "Perm.h"
#include "QtConv.h"
#include "RecipeDraft.h"
#include "Session.h"
#include "Theme.h"
#include "ThreadPool.h"
#include "ToolDebugHost.h"
#include "ToolLibrary.h"
#include "ToolTreeIcons.h"
#include "UiText.h"
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QToolTip>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWidget>

namespace Vision {

enum {
    ToolListId = Qt::UserRole,           ///< 节点 id
    ToolListKind = Qt::UserRole + 1,     ///< ListKind*
    ToolListRunState = Qt::UserRole + 2, ///< TreeRunState*
    ToolListIconKey = Qt::UserRole + 3, ///< ToolTreeIconKey*
    ToolListBadge = Qt::UserRole + 4,   ///< TreeBadge*
    ToolListChecked = Qt::UserRole + 5, ///< 勾选状态
    ToolListToolName = Qt::UserRole + 6 ///< 工具 toolName，用于图标
};

enum {
    ListKindGroup = 0,  ///< 并行组
    ListKindBranch = 1, ///< 分支
    ListKindTool = 2,   ///< 工具
    ListKindIf = 3,     ///< 条件组
    ListKindLoop = 4     ///< 循环组
};

enum {
    TreeRunNone = 0, ///< 未运行
    TreeRunOk = 1,   ///< OK
    TreeRunNg = 2    ///< NG
};

enum {
    TreeBadgeNone = 0,     ///< 无角标
    TreeBadgeParallel = 1, ///< 并行
    TreeBadgeIf = 2,       ///< 条件
    TreeBadgeLoop = 3      ///< 循环
};

/**
 * @class JobToolTreeDelegate
 * @brief 流程树委托：按运行状态和角标绘制节点
 */
class JobToolTreeDelegate : public QStyledItemDelegate {
public:
    explicit JobToolTreeDelegate(JobDlg* owner, QObject* parent = 0)
        : QStyledItemDelegate(parent)
        , m_owner(owner)
    {
    }

    // 行高至少 28
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        if (s.height() < 28) {
            s.setHeight(28);
        }
        return s;
    }

    // 自绘图标、文字和角标
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.features &= ~QStyleOptionViewItem::HasCheckIndicator;
        opt.icon = QIcon();
        opt.text.clear();

        const bool selected = (opt.state & QStyle::State_Selected) != 0;
        painter->save();
        if (selected) {
            painter->fillRect(opt.rect, QColor(0xE3, 0xF2, 0xFD));
            painter->setPen(QPen(QColor(0x18, 0x90, 0xff), 1));
            painter->drawRect(opt.rect.adjusted(0, 0, -1, -1));
        } else if (opt.state & QStyle::State_MouseOver) {
            painter->fillRect(opt.rect, QColor(0xF5, 0xF7, 0xFA));
        }
        painter->restore();

        const int iconKey = index.data(ToolListIconKey).toInt();
        const QString toolName = index.data(ToolListToolName).toString();
        const int runState = index.data(ToolListRunState).toInt();
        const int badge = index.data(ToolListBadge).toInt();
        const bool checked = index.data(ToolListChecked).toBool();
        const bool enabledLook = (opt.state & QStyle::State_Enabled) != 0;
        const QString text = index.data(Qt::DisplayRole).toString();

        const int pad = 6;
        const int iconSz = 20;
        const int checkSz = 16;
        const int dotSz = 8;
        const int gap = 8;
        const int rightReserve = checkSz + gap + dotSz + gap + pad;

        QRect content = opt.rect.adjusted(pad, 0, -rightReserve, 0);
        QRect iconRect(content.left(), content.center().y() - iconSz / 2, iconSz, iconSz);
        toolTreeIconForName(toolName, iconKey).paint(painter, iconRect);

        int textLeft = iconRect.right() + gap;
        QFontMetrics fm(opt.font);
        const int maxText = qMax(0, content.right() - textLeft);

        QRect textRect(textLeft, content.top(), maxText, content.height());
        painter->save();
        QColor fg = enabledLook ? opt.palette.color(QPalette::Text) : QColor(0xA0, 0xA0, 0xA0);
        if (!index.data(ToolListChecked).toBool()
            && index.data(ToolListKind).toInt() == ListKindTool) {
            fg = QColor(0xA0, 0xA0, 0xA0);
        }
        painter->setPen(fg);
        painter->setFont(opt.font);
        const QString elided = fm.elidedText(text, Qt::ElideRight, maxText);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);

        if (badge != TreeBadgeNone) {
            QString badgeText; if (badge == TreeBadgeParallel) badgeText = T("并行"); else if (badge == TreeBadgeLoop) badgeText = T("循环"); else badgeText = T("条件");
            const int tw = fm.width(elided);
            const int bw = fm.width(badgeText) + 10;
            const int bh = fm.height() + 2;
            QRect br(textLeft + tw + gap, content.center().y() - bh / 2, bw, bh);
            if (br.right() <= content.right()) {
                painter->setBrush(QColor(0xE3, 0xF2, 0xFD));
                painter->setPen(QPen(QColor(0x1E, 0x88, 0xE5)));
                painter->drawRoundedRect(br, 3, 3);
                painter->setPen(QColor(0x15, 0x65, 0xC0));
                painter->drawText(br, Qt::AlignCenter, badgeText);
            }
        }
        painter->restore();

        const QRect checkRect(opt.rect.right() - pad - checkSz,
                              opt.rect.center().y() - checkSz / 2, checkSz, checkSz);
        const QRect dotRect(checkRect.left() - gap - dotSz,
                            opt.rect.center().y() - dotSz / 2, dotSz, dotSz);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        if (runState == TreeRunOk) {
            painter->setBrush(QColor(0x4C, 0xAF, 0x50));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(dotRect);
        } else if (runState == TreeRunNg) {
            painter->setBrush(QColor(0xE5, 0x39, 0x35));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(dotRect);
        } else {
            painter->setBrush(QColor(0xCF, 0xD8, 0xDC));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(dotRect);
        }
        painter->restore();

        QStyleOptionButton cb;
        cb.rect = checkRect;
        cb.state = QStyle::State_Enabled;
        if (!m_owner || !m_owner->canEdit()) {
            cb.state = QStyle::State_None;
        } else {
            cb.state |= QStyle::State_Enabled;
        }
        if (checked) {
            cb.state |= QStyle::State_On;
        } else {
            cb.state |= QStyle::State_Off;
        }
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &cb, painter, opt.widget);
    }

    // 勾选框点击切换使能
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option, const QModelIndex& index)
    {
        if (!m_owner || !m_owner->canEdit()) {
            return false;
        }
        if (event->type() != QEvent::MouseButtonRelease) {
            return false;
        }
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton) {
            return false;
        }
        const int pad = 4;
        const int checkSz = 16;
        const QRect checkRect(option.rect.right() - pad - checkSz,
                              option.rect.center().y() - checkSz / 2, checkSz, checkSz);
        if (!checkRect.contains(me->pos())) {
            return false;
        }
        QTreeWidget* tree = qobject_cast<QTreeWidget*>(const_cast<QWidget*>(option.widget));
        if (!tree) {
            return false;
        }
        // itemFromIndex 是 protected，改用 itemAt
        QTreeWidgetItem* item = tree->itemAt(me->pos());
        if (!item) {
            return false;
        }
        const bool cur = index.data(ToolListChecked).toBool();
        m_owner->applyTreeNodeEnabled(item, !cur);
        Q_UNUSED(model);
        return true;
    }

private:
    JobDlg* m_owner; ///< 所属对话框
};

// 创建树节点并写入 id、种类和图标
static QTreeWidgetItem* makeItem(QTreeWidgetItem* parent, QTreeWidget* tree,
                                 const QString& text, const QString& id, int kind,
                                 int iconKey, int badge, bool checked,
                                 const QString& toolName = QString())
{
    QTreeWidgetItem* it = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
    it->setText(0, text);
    it->setData(0, ToolListId, id);
    it->setData(0, ToolListKind, kind);
    it->setData(0, ToolListRunState, TreeRunNone);
    it->setData(0, ToolListIconKey, iconKey);
    it->setData(0, ToolListBadge, badge);
    it->setData(0, ToolListChecked, checked);
    it->setData(0, ToolListToolName, toolName);
    it->setToolTip(0, text);
    if (!checked && kind == ListKindTool) {
        it->setForeground(0, QBrush(QColor(0xA0, 0xA0, 0xA0)));
    }
    return it;
}

// 组内工具是否全部使能
static bool groupToolsAllEnabled(const JobCfg* job, const ToolGroup& g)
{
    const std::vector<ObjectId> ids = g.toolIds();
    if (ids.empty()) {
        return true;
    }
    for (size_t i = 0; i < ids.size(); ++i) {
        const ToolCfg* t = job->findTool(ids[i]);
        if (!t || !t->enabled) {
            return false;
        }
    }
    return true;
}

// 流程配置对话框
JobDlg::JobDlg(Session* session, Solution* solution, Job* job, const QString& jobId,
                     QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_sol(solution)
    , m_job(job)
    , m_jobId(jobId)
    , m_debug(nullptr)
{
    setWindowTitle(T("流程配置"));
    resize(1100, 640);
    setMinimumSize(760, 420);
    enableDialogResize(this);
    const bool can = canEdit();

    m_name = new QLineEdit(this);
    m_enabled = new QCheckBox(T("启用"), this);
    m_enabled->setEnabled(can);
    m_timeout = new QSpinBox(this);
    m_timeout->setRange(1, 600000);
    m_timeout->setSuffix(QLatin1String(" ms"));
    m_fail = new QComboBox(this);
    m_fail->addItem(T("继续"), FailContinue);
    m_fail->addItem(T("停工站"), FailStopStation);
    m_fail->addItem(T("停全部"), FailStopAll);
    m_fail->addItem(T("跳过"), FailSkip);
    m_fromRule = new QComboBox(this);

    JobCfg* formalJob = m_sol->findJobConfig(oid(jobId));
    if (formalJob) {
        m_draftJob = formalJob->clone();
        m_formalSnapshot = formalJob->clone();
        m_name->setText(qs(m_draftJob.name));
        m_enabled->setChecked(m_draftJob.enabled);
        m_timeout->setValue(m_draftJob.timeoutMs);
        m_fail->setCurrentIndex(static_cast<int>(m_draftJob.failPolicy));
        JobRstRulePanel::populateFromRuleCombo(m_fromRule, &m_draftJob);
        DraftRegistry::registerJobDraft(m_draftJob.id);
    }
    updateWindowTitle();
    connect(m_name, SIGNAL(textEdited(QString)), this, SLOT(onHeaderEdited()));
    connect(m_enabled, SIGNAL(toggled(bool)), this, SLOT(onHeaderEdited()));
    connect(m_timeout, SIGNAL(valueChanged(int)), this, SLOT(onHeaderEdited()));
    connect(m_fail, SIGNAL(currentIndexChanged(int)), this, SLOT(onHeaderEdited()));
    connect(m_fromRule, SIGNAL(currentIndexChanged(int)), this, SLOT(onFromRuleChanged()));

    QGridLayout* head = new QGridLayout();
    head->setHorizontalSpacing(12);
    head->setVerticalSpacing(6);
    head->addWidget(new QLabel(T("名称"), this), 0, 0);
    QWidget* nameRow = new QWidget(this);
    QHBoxLayout* nameLay = new QHBoxLayout(nameRow);
    nameLay->setContentsMargins(0, 0, 0, 0);
    nameLay->addWidget(m_name, 1);
    nameLay->addWidget(m_enabled);
    head->addWidget(nameRow, 0, 1);
    head->addWidget(new QLabel(T("超时"), this), 0, 2);
    head->addWidget(m_timeout, 0, 3);
    head->addWidget(new QLabel(T("失败策略"), this), 1, 0);
    head->addWidget(m_fail, 1, 1);
    head->addWidget(new QLabel(T("结果来源"), this), 1, 2);
    head->addWidget(m_fromRule, 1, 3);
    head->setColumnStretch(1, 1);
    head->setColumnStretch(3, 1);

    m_lib = new QTreeWidget(this);
    m_lib->setHeaderLabel(T("工具库"));
    m_lib->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_lib->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_lib->setMinimumHeight(80);
    fillRegisteredToolTree(m_lib, true);
    connect(m_lib, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onAdd()));

    m_tools = new QTreeWidget(this);
    m_tools->setHeaderHidden(true);
    m_tools->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tools->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_tools->setMinimumHeight(80);
    m_tools->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tools->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tools->setItemDelegate(new JobToolTreeDelegate(this, m_tools));
    m_tools->setMouseTracking(true);
    m_tools->setIndentation(18);
    connect(m_tools, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onToolMenu(QPoint)));
    connect(m_tools, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(onTreeDouble(QTreeWidgetItem*,int)));
    connect(m_tools, SIGNAL(itemSelectionChanged()), this, SLOT(updateToolButtons()));

    m_flowCanvas = new JobFlowCanvasPanel(m_session, m_sol, this);
    m_flowCanvas->setJobId(jobId);
    m_flowCanvas->bindJob(&m_draftJob);
    m_flowCanvas->setDraftMode(true);
    m_flowCanvas->setEditable(can);
    m_flowCanvas->setHintText(T("滚轮缩放，拖动画布平移，拖工具可移动，从输出端口拖到输入端口连线。右键与右侧树相同。"));
    m_flowCanvas->setMinimumHeight(80);
    m_flowCanvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (JobCanvas* cv = m_flowCanvas->canvas()) {
        cv->setMinimumHeight(80);
        cv->setMinimumWidth(160);
        cv->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    connect(m_flowCanvas, SIGNAL(toolClicked(QString)), this, SLOT(onCanvasTool(QString)));
    connect(m_flowCanvas, SIGNAL(toolDoubleClicked(QString)), this, SLOT(onCanvasToolDouble(QString)));
    connect(m_flowCanvas, SIGNAL(groupClicked(QString)), this, SLOT(onCanvasGroup(QString)));
    connect(m_flowCanvas, SIGNAL(canvasMenuRequested(QString,int,QPoint)),
            this, SLOT(onCanvasMenu(QString,int,QPoint)));
    connect(m_flowCanvas, SIGNAL(linksChanged()), this, SLOT(onHeaderEdited()));
    connect(m_flowCanvas, SIGNAL(nodeMoved(QString,qreal,qreal)), this, SLOT(onHeaderEdited()));

    QPushButton* add = new QPushButton(T("添加工具"), this);
    QPushButton* par = new QPushButton(T("添加并行组"), this);
    m_ifBtn = new QPushButton(T("添加条件组"), this);
    m_loopBtn = new QPushButton(T("添加循环组"), this);
    m_branch = new QPushButton(T("添加支路"), this);
    QPushButton* run = new QPushButton(T("运行本流程"), this);
    setButtonRole(run, "primary");
    connect(add, SIGNAL(clicked()), this, SLOT(onAdd()));
    connect(par, SIGNAL(clicked()), this, SLOT(onParallel()));
    connect(m_ifBtn, SIGNAL(clicked()), this, SLOT(onIf()));
    connect(m_loopBtn, SIGNAL(clicked()), this, SLOT(onLoop()));
    connect(m_branch, SIGNAL(clicked()), this, SLOT(onAddBranch()));
    connect(run, SIGNAL(clicked()), this, SLOT(onRun()));
    add->setEnabled(can);
    par->setEnabled(can);
    m_ifBtn->setEnabled(can);
    m_loopBtn->setEnabled(can);
    m_branch->setEnabled(false);

    m_up = new QPushButton(T("上移"), this);
    m_down = new QPushButton(T("下移"), this);
    m_rename = new QPushButton(T("重命名"), this);
    m_copy = new QPushButton(T("复制"), this);
    m_del = new QPushButton(T("删除"), this);
    connect(m_up, SIGNAL(clicked()), this, SLOT(onMoveUp()));
    connect(m_down, SIGNAL(clicked()), this, SLOT(onMoveDown()));
    connect(m_rename, SIGNAL(clicked()), this, SLOT(onRename()));
    connect(m_copy, SIGNAL(clicked()), this, SLOT(onCopy()));
    connect(m_del, SIGNAL(clicked()), this, SLOT(onDel()));

    QAction* actUp = new QAction(T("上移"), this);
    actUp->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Up));
    connect(actUp, SIGNAL(triggered()), this, SLOT(onMoveUp()));
    QAction* actDown = new QAction(T("下移"), this);
    actDown->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Down));
    connect(actDown, SIGNAL(triggered()), this, SLOT(onMoveDown()));
    QAction* actRename = new QAction(T("重命名"), this);
    actRename->setShortcut(QKeySequence(Qt::Key_F2));
    connect(actRename, SIGNAL(triggered()), this, SLOT(onRename()));
    QAction* actCopy = new QAction(T("复制"), this);
    actCopy->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
    connect(actCopy, SIGNAL(triggered()), this, SLOT(onCopy()));
    QAction* actDel = new QAction(T("删除"), this);
    actDel->setShortcut(QKeySequence::Delete);
    connect(actDel, SIGNAL(triggered()), this, SLOT(onDel()));
    addAction(actUp);
    addAction(actDown);
    addAction(actRename);
    addAction(actCopy);
    addAction(actDel);

    QWidget* leftPanel = new QWidget(this);
    leftPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    QVBoxLayout* left = new QVBoxLayout(leftPanel);
    left->setContentsMargins(0, 0, 0, 0);
    left->addWidget(new QLabel(T("工具库"), leftPanel));
    left->addWidget(m_lib, 1);

    QWidget* midPanel = new QWidget(this);
    midPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* mid = new QVBoxLayout(midPanel);
    mid->setContentsMargins(0, 0, 0, 0);
    QLabel* midTitle = new QLabel(T("流程画布"), midPanel);
    midTitle->setObjectName(QLatin1String("section"));
    mid->addWidget(midTitle);
    mid->addWidget(m_flowCanvas, 1);
    // 工具操作按钮网格
    QGridLayout* tb = new QGridLayout();
    tb->setHorizontalSpacing(8);
    tb->setVerticalSpacing(6);
    tb->addWidget(add, 0, 0);
    tb->addWidget(par, 0, 1);
    tb->addWidget(m_ifBtn, 1, 0);
    tb->addWidget(m_loopBtn, 1, 1);
    tb->addWidget(m_branch, 2, 0);
    tb->addWidget(run, 3, 0, 1, 2);
    mid->addLayout(tb);

    QWidget* rightPanel = new QWidget(this);
    rightPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    QVBoxLayout* right = new QVBoxLayout(rightPanel);
    right->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout* titleRow = new QHBoxLayout();
    QLabel* rTitle = new QLabel(T("流程结构"), rightPanel);
    rTitle->setObjectName(QLatin1String("section"));
    QLabel* rSync = new QLabel(T("与画布同步"), rightPanel);
    rSync->setObjectName(QLatin1String("hint"));
    titleRow->addWidget(rTitle);
    titleRow->addWidget(rSync, 1);
    QLabel* rHint = new QLabel(T("选中 Then/Else 分支或并行支路后再添加工具。条件组只执行命中的一侧。"), rightPanel);
    rHint->setObjectName(QLatin1String("hint"));
    rHint->setWordWrap(true);
    rHint->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rHint->setMaximumHeight(40);
    right->addLayout(titleRow);
    right->addWidget(rHint);

    m_condPanel = new QWidget(rightPanel);
    m_condSrc = new QComboBox(m_condPanel);
    m_condWant = new QComboBox(m_condPanel);
    m_condWant->addItem(T("结果为 OK 时走 Then"), CondThenOnOk);
    m_condWant->addItem(T("结果为 NG 时走 Then"), CondThenOnNg);
    connect(m_condSrc, SIGNAL(currentIndexChanged(int)), this, SLOT(onCondChanged()));
    connect(m_condWant, SIGNAL(currentIndexChanged(int)), this, SLOT(onCondChanged()));
    QFormLayout* condForm = new QFormLayout(m_condPanel);
    condForm->setContentsMargins(0, 0, 0, 8);
    condForm->addRow(T("条件来源"), m_condSrc);
    condForm->addRow(T("Then 条件"), m_condWant);
    m_condPanel->setVisible(false);
    right->addWidget(m_condPanel);

    m_loopPanel = new QWidget(rightPanel);
    m_loopCount = new QSpinBox(m_loopPanel);
    m_loopCount->setRange(1, 9999);
    m_loopCount->setValue(1);
    connect(m_loopCount, SIGNAL(valueChanged(int)), this, SLOT(onLoopCountChanged(int)));
    QFormLayout* loopForm = new QFormLayout(m_loopPanel);
    loopForm->setContentsMargins(0, 0, 0, 8);
    loopForm->addRow(T("循环次数"), m_loopCount);
    m_loopPanel->setVisible(false);
    right->addWidget(m_loopPanel);

    m_btnRstRules = new QPushButton(T("输出结果配置"), rightPanel);
    connect(m_btnRstRules, SIGNAL(clicked()), this, SLOT(onOpenRstRules()));
    m_btnRstRules->setEnabled(can);

    right->addWidget(m_tools, 1);
    right->addWidget(m_btnRstRules);
    QHBoxLayout* ops1 = new QHBoxLayout();
    ops1->addWidget(m_up);
    ops1->addWidget(m_down);
    QHBoxLayout* ops2 = new QHBoxLayout();
    ops2->addWidget(m_rename);
    ops2->addWidget(m_copy);
    ops2->addWidget(m_del);
    right->addLayout(ops1);
    right->addLayout(ops2);

    // 确定走保存，取消关闭
    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, rightPanel);
    box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(box, SIGNAL(accepted()), this, SLOT(onSave()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));
    QHBoxLayout* okRow = new QHBoxLayout();
    okRow->setContentsMargins(0, 8, 0, 0);
    okRow->addStretch();
    okRow->addWidget(box);
    right->addLayout(okRow);

    QHBoxLayout* cols = new QHBoxLayout();
    cols->setSpacing(12);
    cols->addWidget(leftPanel, 1);
    cols->addWidget(midPanel, 2);
    cols->addWidget(rightPanel, 1);

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(head);
    lay->addLayout(cols, 1);
    m_name->setReadOnly(!can);
    m_timeout->setEnabled(can);
    m_fail->setEnabled(can);
    m_fromRule->setEnabled(can);
    reloadTools();
}

// 是否允许改方案
bool JobDlg::canEdit() const
{
    return m_session->has(PermEditSolution);
}

// 当前编辑中的流程草稿
JobCfg* JobDlg::currentJob()
{
    return m_draftJob.id.isNull() ? 0 : &m_draftJob;
}

// 当前编辑中的流程草稿
const JobCfg* JobDlg::currentJob() const
{
    return m_draftJob.id.isNull() ? 0 : &m_draftJob;
}

// 当前树节点 id
QString JobDlg::currentNodeId() const
{
    QTreeWidgetItem* it = m_tools->currentItem();
    return it ? it->data(0, ToolListId).toString() : QString();
}

// 当前树节点种类
int JobDlg::currentKind() const
{
    QTreeWidgetItem* it = m_tools->currentItem();
    return it ? it->data(0, ToolListKind).toInt() : -1;
}

// 移动用的节点 id
ObjectId JobDlg::moveId() const
{
    const JobCfg* job = currentJob();
    const QString id = currentNodeId();
    if (!job || id.isEmpty()) {
        return ObjectId();
    }
    if (currentKind() == ListKindTool) {
        ToolGroup* leaf = const_cast<JobCfg*>(job)->findLeaf(oid(id));
        return leaf ? leaf->id : ObjectId();
    }
    return oid(id);
}

// 按 id 选中树节点
void JobDlg::selectInTree(const QString& id)
{
    if (id.isEmpty()) {
        return;
    }
    QTreeWidgetItemIterator it(m_tools);
    while (*it) {
        if ((*it)->data(0, ToolListId).toString() == id) {
            m_tools->setCurrentItem(*it);
            return;
        }
        ++it;
    }
}

// 按组递归填流程树
void JobDlg::fillTree(QTreeWidgetItem* parent, const ToolGroup& g, const JobCfg* job)
{
    if (!g.toolId.isNull() && g.children.empty()) {
        const ToolCfg* t = job->findTool(g.toolId);
        QString label = t ? qs(t->name) : qs(g.name);
        const bool checked = t ? t->enabled : true;
        const int iconKey = t ? toolTreeIconKeyForType(t->type) : ToolTreeIconCamera;
        const QString toolName = t ? qs(t->toolName) : QString();
        QTreeWidgetItem* toolItem = makeItem(parent, m_tools, label, qs(g.toolId), ListKindTool,
                                             iconKey, TreeBadgeNone, checked, toolName);
        if (t) {
            toolItem->setToolTip(0, qs(t->name) + QLatin1String(" (") + qs(t->toolName)
                                     + QLatin1Char(')'));
        }
        return;
    }
    if (g.kind == GroupParallel) {
        QTreeWidgetItem* it = makeItem(parent, m_tools, qs(g.name), qs(g.id), ListKindGroup,
                                       ToolTreeIconParallel, TreeBadgeParallel,
                                       groupToolsAllEnabled(job, g));
        for (int i = 0; i < g.children.size(); ++i) {
            fillTree(it, g.children[i], job);
        }
        return;
    }
    if (g.kind == GroupIf) {
        QTreeWidgetItem* it = makeItem(parent, m_tools, qs(g.name), qs(g.id), ListKindIf,
                                       ToolTreeIconIf, TreeBadgeIf, groupToolsAllEnabled(job, g));
        for (int i = 0; i < g.children.size(); ++i) {
            fillTree(it, g.children[i], job);
        }
        return;
    }
    if (g.kind == GroupLoop) {
        QTreeWidgetItem* it = makeItem(parent, m_tools, qs(g.name), qs(g.id), ListKindLoop,
                                       ToolTreeIconBranch, TreeBadgeLoop, groupToolsAllEnabled(job, g));
        for (int i = 0; i < g.children.size(); ++i) {
            fillTree(it, g.children[i], job);
        }
        return;
    }
    QTreeWidgetItem* it = makeItem(parent, m_tools, qs(g.name), qs(g.id), ListKindBranch,
                                   ToolTreeIconBranch, TreeBadgeNone, groupToolsAllEnabled(job, g));
    for (int i = 0; i < g.children.size(); ++i) {
        fillTree(it, g.children[i], job);
    }
}

// 按选中项刷新按钮
void JobDlg::updateToolButtons()
{
    JobCfg* job = currentJob();
    const ObjectId mid = moveId();
    const bool has = canEdit() && job && !currentNodeId().isEmpty();
    bool canUp = false;
    bool canDown = false;
    if (has && !mid.isNull()) {
        ToolGroup* p = job->findParent(mid);
        if (p) {
            int idx = -1;
            for (int i = 0; i < p->children.size(); ++i) {
                if (p->children[i].id == mid) {
                    idx = i;
                    break;
                }
            }
            canUp = idx > 0;
            canDown = idx >= 0 && idx < p->children.size() - 1;
            if (p->kind == GroupIf) {
                // If 的 Then/Else 不允许上下移
                canUp = false;
                canDown = false;
            }
        }
    }
    ToolGroup* armParent = nullptr;
    if (has && job && currentKind() == ListKindBranch) {
        armParent = job->findParent(oid(currentNodeId()));
    }
    const bool ifArm = armParent && armParent->kind == GroupIf;
    m_up->setEnabled(canUp);
    m_down->setEnabled(canDown);
    m_rename->setEnabled(has);
    m_copy->setEnabled(has && !ifArm);
    m_del->setEnabled(has && !ifArm);
    ToolGroup* selG = nullptr;
    if (job && currentKind() == ListKindGroup) {
        selG = job->findGroup(oid(currentNodeId()));
    }
    m_branch->setEnabled(canEdit() && selG && selG->kind == GroupParallel);
    fillCondUi();
    fillLoopUi();
}

// 重载流程树
void JobDlg::reloadTools(const QString& selectId)
{
    const QString keep = !selectId.isEmpty() ? selectId : currentNodeId();
    m_tools->clear();
    JobCfg* job = currentJob();
    if (!job) {
        updateToolButtons();
        return;
    }
    job->ensureTree();
    for (int i = 0; i < job->root.children.size(); ++i) {
        fillTree(0, job->root.children[i], job);
    }
    if (job->root.kind != GroupSequence && job->root.children.empty()) {
        fillTree(0, job->root, job);
    }
    m_tools->expandAll();
    m_flowCanvas->bindJob(job);
    m_flowCanvas->showJob(keep);
    selectInTree(keep);
    if (!m_tools->currentItem() && m_tools->topLevelItemCount() > 0) {
        m_tools->setCurrentItem(m_tools->topLevelItem(0));
    }
    updateToolButtons();
    job->normalizeRstRules();
    refreshToolRunStatus();
}

// 刷新树上的运行状态
void JobDlg::refreshToolRunStatus()
{
    QTreeWidgetItemIterator it(m_tools);
    while (*it) {
        QTreeWidgetItem* item = *it;
        const int kind = item->data(0, ToolListKind).toInt();
        int state = TreeRunNone;
        if (kind == ListKindTool) {
            if (m_job) {
                const ObjectId id = oid(item->data(0, ToolListId).toString());
                if (m_job->hasToolRun(id)) {
                    bool ok = false;
                    if (m_job->cachedToolOk(id, &ok) && ok) {
                        state = TreeRunOk;
                    } else {
                        state = TreeRunNg;
                    }
                }
            }
        } else if (m_job) {
            JobCfg* job = currentJob();
            ToolGroup* g = job ? job->findGroup(oid(item->data(0, ToolListId).toString())) : nullptr;
            if (job && g) {
                const std::vector<ObjectId> ids = g->toolIds();
                bool any = false;
                bool anyNg = false;
                for (size_t i = 0; i < ids.size(); ++i) {
                    if (!m_job->hasToolRun(ids[i])) {
                        continue;
                    }
                    any = true;
                    bool ok = false;
                    if (!(m_job->cachedToolOk(ids[i], &ok) && ok)) {
                        anyNg = true;
                    }
                }
                if (anyNg) {
                    state = TreeRunNg;
                } else if (any) {
                    state = TreeRunOk;
                }
            }
        }
        item->setData(0, ToolListRunState, state);
        ++it;
    }
    if (m_tools->viewport()) {
        m_tools->viewport()->update();
    }
}

// 把勾选写回工具使能
void JobDlg::applyTreeNodeEnabled(QTreeWidgetItem* item, bool enabled)
{
    JobCfg* job = currentJob();
    if (!job || !canEdit() || !item) {
        return;
    }
    const int kind = item->data(0, ToolListKind).toInt();
    const ObjectId id = oid(item->data(0, ToolListId).toString());
    if (kind == ListKindTool) {
        ToolCfg* t = job->findTool(id);
        if (!t) {
            return;
        }
        t->enabled = enabled;
    } else {
        ToolGroup* g = job->findGroup(id);
        if (!g) {
            return;
        }
        const std::vector<ObjectId> ids = g->toolIds();
        for (size_t i = 0; i < ids.size(); ++i) {
            ToolCfg* t = job->findTool(ids[i]);
            if (t) {
                t->enabled = enabled;
            }
        }
    }
    markDraftDirty();
    reloadTools(item->data(0, ToolListId).toString());
}

// 把页头写回草稿
void JobDlg::flushHeaderToDraft()
{
    JobCfg* job = currentJob();
    if (!job || !canEdit()) {
        return;
    }
    job->enabled = m_enabled->isChecked();
    job->name = ss(m_name->text().trimmed());
    job->timeoutMs = m_timeout->value();
    job->failPolicy = static_cast<FailPolicy>(m_fail->currentIndex());
    JobRstRulePanel::applyFromRuleCombo(m_fromRule, job);
}

// 刷新结果规则下拉
void JobDlg::refreshFromRuleCombo()
{
    JobRstRulePanel::populateFromRuleCombo(m_fromRule, currentJob());
}

// 标记草稿已修改
void JobDlg::markDraftDirty()
{
    flushHeaderToDraft();
    DraftRegistry::setJobDraftDirty(oid(m_jobId), !m_draftJob.equals(m_formalSnapshot));
    updateWindowTitle();
}

// 刷新窗口标题
void JobDlg::updateWindowTitle()
{
    QString title = T("流程配置");
    if (!m_draftJob.name.empty()) {
        title = qs(m_draftJob.name) + QLatin1Char(' ') + QString(QChar(0x00B7)) + QLatin1Char(' ') + title;
    }
    if (DraftRegistry::jobDraftBlocksFormalRun(oid(m_jobId))) {
        title += T(" · 未保存");
    }
    setWindowTitle(title);
    syncDialogTitleBar(this, title);
}

// 提交草稿为正式流程
bool JobDlg::trySave()
{
    if (!canEdit()) {
        return true;
    }
    flushHeaderToDraft();
    if (m_draftJob.equals(m_formalSnapshot)) {
        DraftRegistry::setJobDraftDirty(oid(m_jobId), false);
        return true;
    }
    if (!m_sol->commitJob(m_draftJob)) {
        QMessageBox::warning(this, T("保存失败"), T("流程未能保存。"));
        return false;
    }
    m_formalSnapshot = m_draftJob.clone();
    m_sol->dirty = true;
    DraftRegistry::setJobDraftDirty(oid(m_jobId), false);
    return true;
}

// 关闭前询问
int JobDlg::askCloseAction()
{
    if (!DraftRegistry::jobDraftBlocksFormalRun(oid(m_jobId))) {
        DraftRegistry::unregisterJobDraft(oid(m_jobId));
        return 1;
    }
    QMessageBox box(QMessageBox::Question, T("未保存的修改"),
                    T("有未保存的修改，是否保存？"),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, this);
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
        DraftRegistry::unregisterJobDraft(oid(m_jobId));
        return 2;
    }
    DraftRegistry::unregisterJobDraft(oid(m_jobId));
    return 1;
}

// 取消对话框
void JobDlg::reject()
{
    const int action = askCloseAction();
    if (action == 0) {
        return;
    }
    if (action == 2) {
        QDialog::accept();
        return;
    }
    QDialog::reject();
}

// 关闭窗口
void JobDlg::closeEvent(QCloseEvent* event)
{
    const int action = askCloseAction();
    if (action == 0) {
        event->ignore();
        return;
    }
    ThreadPool::shared().waitIdle();
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    event->accept();
    if (action == 2) {
        QDialog::accept();
    } else {
        QDialog::reject();
    }
}

// 页头被编辑
void JobDlg::onHeaderEdited()
{
    markDraftDirty();
}

// 结果来源变化
void JobDlg::onFromRuleChanged()
{
    markDraftDirty();
}

// 结果规则变化
void JobDlg::onRstRulesChanged()
{
    markDraftDirty();
}

// 打开结果规则
void JobDlg::onOpenRstRules()
{
    JobCfg* job = currentJob();
    if (!job) {
        return;
    }
    const std::vector<RstRule> rulesSnap = job->resultRules;
    const ObjectId fromSnap = job->resultFromRuleId;
    JobRstRuleDlg dlg(job, canEdit(), this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshFromRuleCombo();
        onRstRulesChanged();
    } else {
        job->resultRules = rulesSnap;
        job->resultFromRuleId = fromSnap;
    }
}

// 添加节点
void JobDlg::onAdd()
{
    if (!canEdit()) {
        return;
    }
    JobCfg* job = currentJob();
    QTreeWidgetItem* it = m_lib->currentItem();
    if (!job || !it) {
        QMessageBox::information(this, T("添加工具"), T("请先在左侧选择一个工具。"));
        return;
    }
    const QString tn = it->data(0, Qt::UserRole).toString();
    if (tn.isEmpty()) {
        QMessageBox::information(this, T("添加工具"), T("请选择具体工具，不要选分类。"));
        return;
    }
    if (tn == QLatin1String("group.parallel")) {
        onParallel();
        return;
    }
    if (tn == QLatin1String("group.if")) {
        onIf();
        return;
    }
    if (tn == QLatin1String("group.loop")) {
        onLoop();
        return;
    }
    // 新工具绑定工站第一台设备
    ObjectId dev;
    for (int i = 0; i < m_sol->stations.size(); ++i) {
        for (int j = 0; j < m_sol->stations[i].jobs.size(); ++j) {
            if (m_sol->stations[i].jobs[j].id == job->id && !m_sol->stations[i].boundDevices.empty()) {
                dev = m_sol->stations[i].boundDevices[0].deviceId;
            }
        }
    }
    const ToolCfg t = ToolCfg::make(ss(tn), dev);
    if (!job->insertTool(t, oid(currentNodeId()))) {
        return;
    }
    markDraftDirty();
    reloadTools(qs(t.id));
}

// 删除节点
void JobDlg::onDel()
{
    if (!canEdit()) {
        return;
    }
    JobCfg* job = currentJob();
    const QString id = currentNodeId();
    const int kind = currentKind();
    if (!job || id.isEmpty()) {
        return;
    }
    const QString msg = (kind == ListKindTool)
        ? T("删除此工具？")
        : T("删除此组及其内部工具？");
    if (QMessageBox::question(this, T("删除"), msg) != QMessageBox::Yes) {
        return;
    }
    bool ok = false;
    if (kind == ListKindTool) {
        ok = job->removeTool(oid(id));
    } else {
        ok = job->removeGroup(oid(id));
    }
    if (!ok) {
        return;
    }
    markDraftDirty();
    reloadTools();
}

// 上移
void JobDlg::onMoveUp()
{
    JobCfg* job = currentJob();
    if (!job || !canEdit()) {
        return;
    }
    const QString keep = currentNodeId();
    if (!job->moveTreeNode(moveId(), -1)) {
        return;
    }
    markDraftDirty();
    reloadTools(keep);
}

// 下移
void JobDlg::onMoveDown()
{
    JobCfg* job = currentJob();
    if (!job || !canEdit()) {
        return;
    }
    const QString keep = currentNodeId();
    if (!job->moveTreeNode(moveId(), 1)) {
        return;
    }
    markDraftDirty();
    reloadTools(keep);
}

// 重命名
void JobDlg::onRename()
{
    JobCfg* job = currentJob();
    const QString id = currentNodeId();
    const int kind = currentKind();
    if (!job || !canEdit() || id.isEmpty()) {
        return;
    }
    QString cur;
    if (kind == ListKindTool) {
        const ToolCfg* t = job->findTool(oid(id));
        if (!t) {
            return;
        }
        cur = qs(t->name);
    } else {
        ToolGroup* g = job->findGroup(oid(id));
        if (!g) {
            return;
        }
        cur = qs(g->name);
    }
    bool ok = false;
    const QString n = QInputDialog::getText(this, T("重命名"), T("名称"),
                                            QLineEdit::Normal, cur, &ok);
    if (!ok) {
        return;
    }
    if (kind == ListKindTool) {
        if (!job->renameTool(oid(id), ss(n))) {
            return;
        }
    } else if (!job->renameGroup(oid(id), ss(n))) {
        return;
    }
    markDraftDirty();
    reloadTools(id);
}

// 复制
void JobDlg::onCopy()
{
    JobCfg* job = currentJob();
    const QString id = currentNodeId();
    const int kind = currentKind();
    if (!job || !canEdit() || id.isEmpty()) {
        return;
    }
    ObjectId nid;
    if (kind == ListKindTool) {
        nid = job->duplicateTool(oid(id));
    } else {
        nid = job->duplicateGroup(oid(id));
    }
    if (nid.isNull()) {
        return;
    }
    markDraftDirty();
    if (kind == ListKindTool) {
        reloadTools(qs(nid));
    } else {
        reloadTools(qs(nid));
    }
}


// 切换工具使能
void JobDlg::onToggleToolEnabled()
{
    if (!canEdit() || currentKind() != ListKindTool) {
        return;
    }
    QTreeWidgetItem* it = m_tools->currentItem();
    if (!it) {
        return;
    }
    const bool cur = it->data(0, ToolListChecked).toBool();
    applyTreeNodeEnabled(it, !cur);
}
// 工具树右键菜单
void JobDlg::onToolMenu(const QPoint& pos)
{
    QTreeWidgetItem* it = m_tools->itemAt(pos);
    if (it) {
        m_tools->setCurrentItem(it);
    }
    updateToolButtons();
    popupFlowMenu(m_tools->mapToGlobal(pos));
}

// 弹出流程菜单
void JobDlg::popupFlowMenu(const QPoint& globalPos)
{
    QMenu menu(this);
    QAction* addTool = menu.addAction(T("在此处添加工具"));
    QAction* branch = menu.addAction(T("添加支路"));
    branch->setEnabled(m_branch->isEnabled());
    addTool->setEnabled(canEdit());
    menu.addSeparator();
    QAction* up = menu.addAction(T("上移"));
    QAction* down = menu.addAction(T("下移"));
    menu.addSeparator();
    QAction* rename = menu.addAction(T("重命名"));
    QAction* copy = menu.addAction(T("复制"));
    QAction* del = menu.addAction(T("删除"));
    up->setEnabled(m_up->isEnabled());
    down->setEnabled(m_down->isEnabled());
    rename->setEnabled(m_rename->isEnabled());
    copy->setEnabled(m_copy->isEnabled());
    del->setEnabled(m_del->isEnabled());
    menu.addSeparator();
    QAction* enAct = nullptr;
    if (canEdit() && m_tools->currentItem()) {
        QTreeWidgetItem* cur = m_tools->currentItem();
        const bool on = cur->data(0, ToolListChecked).toBool();
        const int kind = cur->data(0, ToolListKind).toInt();
        if (kind == ListKindTool) {
            enAct = menu.addAction(on ? T("禁用工具") : T("启用工具"));
        } else {
            enAct = menu.addAction(on ? T("禁用此组") : T("启用此组"));
        }
    }
    menu.addSeparator();
    QAction* autoLay = menu.addAction(T("自动布局"));
    autoLay->setEnabled(canEdit());
    QAction* zoom = menu.addAction(T("复位缩放"));
    QAction* chosen = menu.exec(globalPos);
    if (chosen == addTool) {
        onAdd();
    } else if (chosen == branch) {
        onAddBranch();
    } else if (chosen == up) {
        onMoveUp();
    } else if (chosen == down) {
        onMoveDown();
    } else if (chosen == rename) {
        onRename();
    } else if (chosen == copy) {
        onCopy();
    } else if (chosen == del) {
        onDel();
    } else if (enAct && chosen == enAct) {
        QTreeWidgetItem* cur = m_tools->currentItem();
        if (cur) {
            applyTreeNodeEnabled(cur, !cur->data(0, ToolListChecked).toBool());
        }
    } else if (chosen == autoLay) {
        JobCfg* job = currentJob();
        if (job && canEdit()) {
            job->autoLayoutCanvas();
            markDraftDirty();
            reloadTools(currentNodeId());
        }
    } else if (chosen == zoom) {
        m_flowCanvas->resetZoom();
    }
}

// 画布选中工具
void JobDlg::onCanvasTool(const QString& toolId)
{
    selectInTree(toolId);
    m_flowCanvas->showJob(toolId);
    updateToolButtons();
}

// 画布双击工具
void JobDlg::onCanvasToolDouble(const QString& toolId)
{
    openToolDebug(toolId);
}

// 树节点双击
void JobDlg::onTreeDouble(QTreeWidgetItem* item, int)
{
    if (!item) {
        return;
    }
    const int kind = item->data(0, ToolListKind).toInt();
    const QString id = item->data(0, ToolListId).toString();
    if (kind == ListKindTool) {
        openToolDebug(id);
        return;
    }
    onRename();
}

// 打开工具调试
void JobDlg::openToolDebug(const QString& toolId)
{
    if (toolId.isEmpty()) {
        return;
    }
    ToolCfg* t = m_draftJob.findTool(oid(toolId));
    if (!t) {
        return;
    }
    selectInTree(toolId);
    m_flowCanvas->showJob(toolId);
    updateToolButtons();
    if (m_debug) {
        m_debug->close();
        m_debug->deleteLater();
        m_debug = 0;
    }
    m_debugToolId = toolId;
    m_debug = new ToolDebugHost(t, m_session, m_sol, m_job, this);
    m_debug->setSolutionCommit(false);
    connect(m_debug, SIGNAL(roiChanged()), this, SLOT(onDebugRoiChanged()));
    connect(m_debug, SIGNAL(draftSaved()), this, SLOT(onDebugSaved()));
    connect(m_debug, SIGNAL(linksChanged()), this, SLOT(onHeaderEdited()));
    m_debug->show();
    m_debug->raise();
    m_debug->refreshView();
}

// 调试运行结束回写
void JobDlg::onDebugRun()
{
    if (!m_session->has(PermRunJobDebug)) {
        QMessageBox::warning(this, T("权限"), T("没有运行调试的权限。"));
        return;
    }
    if (m_debugToolId.isEmpty() || !m_debug) {
        return;
    }
    m_debug->applyParams();
    if (!m_job) {
        return;
    }
    const ToolOut out = m_job->runToolChain(m_sol, oid(m_debugToolId), m_debug->draftTool(),
                                            &m_draftJob);
    m_debug->refreshView(&out);
}

// 调试保存后刷新草稿
void JobDlg::onDebugSaved()
{
    markDraftDirty();
    reloadTools(m_debugToolId);
}

// 调试里 ROI 变化
void JobDlg::onDebugRoiChanged()
{
    markDraftDirty();
    if (m_debug) {
        m_debug->reloadDrawings();
    }
}

// 画布选中组
void JobDlg::onCanvasGroup(const QString& groupId)
{
    selectInTree(groupId);
    m_flowCanvas->showJob(groupId);
    updateToolButtons();
}

// 画布右键菜单
void JobDlg::onCanvasMenu(const QString& id, int kind, const QPoint& globalPos)
{
    if (kind == CanvasHitWire) {
        if (!canEdit()) {
            return;
        }
        QMenu menu(this);
        QAction* reset = menu.addAction(T("恢复自动路由"));
        QAction* del = menu.addAction(T("断开连线"));
        QAction* chosen = menu.exec(globalPos);
        if (!chosen) {
            return;
        }
        if (chosen == reset) {
            m_flowCanvas->resetLinkRoute(id.toInt());
        } else if (chosen == del) {
            m_flowCanvas->unlinkLink(id.toInt());
        }
        return;
    }
    if (!id.isEmpty()) {
        selectInTree(id);
        m_flowCanvas->showJob(id);
        updateToolButtons();
    }
    popupFlowMenu(globalPos);
}

// 添加条件组
void JobDlg::onIf()
{
    JobCfg* job = currentJob();
    if (!job || !canEdit()) {
        return;
    }
    const ObjectId nid = job->addIfGroup(oid(currentNodeId()));
    if (nid.isNull()) {
        return;
    }
    markDraftDirty();
    reloadTools(qs(nid));
}

// 刷新条件组界面
void JobDlg::fillCondUi()
{
    JobCfg* job = currentJob();
    ToolGroup* g = nullptr;
    if (job && currentKind() == ListKindIf) {
        g = job->findGroup(oid(currentNodeId()));
    }
    const bool show = g && g->kind == GroupIf && canEdit();
    m_condPanel->setVisible(show);
    if (!show) {
        return;
    }
    m_condSrc->blockSignals(true);
    m_condWant->blockSignals(true);
    m_condSrc->clear();
    m_condSrc->addItem(T("（未指定）"), QString());
    // 条件源不列出组内工具
    const std::vector<ObjectId> inside = g->toolIds();
    for (int i = 0; i < job->tools.size(); ++i) {
        bool skip = false;
        for (int k = 0; k < static_cast<int>(inside.size()); ++k) {
            if (inside[k] == job->tools[i].id) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        }
        m_condSrc->addItem(qs(job->tools[i].name), qs(job->tools[i].id));
    }
    int srcIdx = 0;
    if (!g->condSourceId.isNull()) {
        const int found = m_condSrc->findData(qs(g->condSourceId));
        if (found >= 0) {
            srcIdx = found;
        }
    }
    m_condSrc->setCurrentIndex(srcIdx);
    m_condWant->setCurrentIndex(g->condWant == CondThenOnNg ? 1 : 0);
    m_condSrc->blockSignals(false);
    m_condWant->blockSignals(false);
}

// 条件组参数变化
void JobDlg::onCondChanged()
{
    JobCfg* job = currentJob();
    if (!job || !canEdit() || currentKind() != ListKindIf) {
        return;
    }
    const ObjectId src = oid(m_condSrc->currentData().toString());
    const int want = m_condWant->currentIndex() == 1 ? CondThenOnNg : CondThenOnOk;
    if (!job->setIfCondition(oid(currentNodeId()), src, want)) {
        return;
    }
    markDraftDirty();
    m_flowCanvas->showJob(currentNodeId());
}



// 添加循环组
void JobDlg::onLoop()
{
    JobCfg* job = currentJob();
    if (!job || !canEdit()) {
        return;
    }
    const ObjectId nid = job->addLoopGroup(oid(currentNodeId()));
    if (nid.isNull()) {
        return;
    }
    markDraftDirty();
    reloadTools(qs(nid));
}

// 刷新循环组界面
void JobDlg::fillLoopUi()
{
    JobCfg* job = currentJob();
    ToolGroup* g = nullptr;
    if (job && currentKind() == ListKindLoop) {
        g = job->findGroup(oid(currentNodeId()));
    }
    const bool show = g && g->kind == GroupLoop && canEdit();
    m_loopPanel->setVisible(show);
    if (!show) {
        return;
    }
    m_loopCount->blockSignals(true);
    m_loopCount->setValue(g->loopCount < 1 ? 1 : g->loopCount);
    m_loopCount->blockSignals(false);
}

// 循环次数变化
void JobDlg::onLoopCountChanged(int value)
{
    JobCfg* job = currentJob();
    if (!job || !canEdit() || currentKind() != ListKindLoop) {
        return;
    }
    if (!job->setLoopCount(oid(currentNodeId()), value)) {
        return;
    }
    markDraftDirty();
    m_flowCanvas->showJob(currentNodeId());
}

void JobDlg::onParallel()
{
    JobCfg* job = currentJob();
    if (!job || !canEdit()) {
        return;
    }
    const ObjectId nid = job->addParallelGroup(oid(currentNodeId()));
    if (nid.isNull()) {
        return;
    }
    markDraftDirty();
    reloadTools(qs(nid));
}

// 添加分支
void JobDlg::onAddBranch()
{
    JobCfg* job = currentJob();
    if (!job || !canEdit() || currentKind() != ListKindGroup) {
        return;
    }
    const ObjectId nid = job->addSequenceBranch(oid(currentNodeId()));
    if (nid.isNull()) {
        return;
    }
    markDraftDirty();
    reloadTools(qs(nid));
}

// 保存流程草稿
void JobDlg::onSave()
{
    if (!canEdit()) {
        accept();
        return;
    }
    if (!trySave()) {
        return;
    }
    DraftRegistry::unregisterJobDraft(oid(m_jobId));
    accept();
}

// 运行流程
void JobDlg::onRun()
{
    if (!m_session->has(PermRunJobDebug)) {
        QMessageBox::warning(this, T("权限"), T("没有运行流程的权限。"));
        return;
    }
    flushHeaderToDraft();
    if (!m_job) {
        QMessageBox::warning(this, T("运行"), T("当前没有可运行的流程实例。"));
        return;
    }
    if (m_job->isRunning()) {
        QMessageBox::information(this, T("运行"), T("流程正在运行，请稍候。"));
        return;
    }
    JobCfg* draft = new JobCfg(m_draftJob);
    Job* job = m_job;
    Solution* sol = m_sol;
    ThreadPool::shared().enqueue([this, job, sol, draft]() {
        const ToolOut out = job->run(sol, draft);
        JobCfg* held = draft;
        if (held) {
            delete held;
            held = nullptr;
        }
        ToolOut* heap = new ToolOut(out);
        QMetaObject::invokeMethod(this, "onAsyncJobRunDone", Qt::QueuedConnection,
                                  Q_ARG(void*, heap));
    });
}

// 异步运行结束
void JobDlg::onAsyncJobRunDone(void* outPtr)
{
    ToolOut* out = static_cast<ToolOut*>(outPtr);
    if (!out) {
        return;
    }
    refreshToolRunStatus();
    if (m_session
        && m_session->settings().shouldPopupJobResult(out->ok, out->error)) {
        const QString title = out->error
                                  ? T("失败")
                                  : (out->ok ? T("OK") : T("NG"));
        if (out->error) {
            QMessageBox::warning(this, title, qs(out->message));
        } else {
            QMessageBox::information(this, title, qs(out->message));
        }
    }
    if (out) {
        delete out;
        out = nullptr;
    }
}

}

#include "moc_JobDlg.cpp"
