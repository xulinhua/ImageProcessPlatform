/**
 * @file image_draw_pad.cpp
 * @brief 绘制面板实现：工具按钮、画笔样式、面板摆放与拖动
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "image_widget/image_draw_pad.h"

#include "image_widget/image_widget_api.hpp"

#include <HsCommData/Style.h>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QApplication>
#include <QDesktopWidget>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

namespace image_widget
{

namespace
{
// 默认工具列表，浏览在最前
std::vector<HsCtrl::DrawTool> defaultTools()
{
    return {HsCtrl::ToolBrowse, HsCtrl::ToolCircle, HsCtrl::ToolRect1, HsCtrl::ToolRect2,
            HsCtrl::ToolEllipse, HsCtrl::ToolLine, HsCtrl::ToolContour, HsCtrl::ToolPolygon};
}
}

// 工具窗口，显示时不抢焦点
ImageDrawPad::ImageDrawPad(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("imageDrawPad"));
    setWindowTitle(QStringLiteral("绘制"));
    setWindowFlags(Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    buildStyleRow();
    tools_ = defaultTools();
    rebuild();
}

// 绑定画布，同一个则忽略
void ImageDrawPad::setImageWindow(HsCtrl::HsImageWindow* view)
{
    if (view_ == view)
        return;
    bindView(view);
}

// 当前绑定的画布
HsCtrl::HsImageWindow* ImageDrawPad::imageWindow() const
{
    return view_;
}

// 设置工具列表，缺浏览时补在最前
void ImageDrawPad::setTools(const std::vector<HsCtrl::DrawTool>& tools)
{
    tools_ = tools.empty() ? defaultTools() : tools;
    bool hasBrowse = false;
    for (HsCtrl::DrawTool tool : tools_)
    {
        if (tool == HsCtrl::ToolBrowse)
            hasBrowse = true;
    }
    if (!hasBrowse)
        tools_.insert(tools_.begin(), HsCtrl::ToolBrowse);
    rebuild();
}

// 显示面板；用户没拖动过时依次试 anchor 左、右、下、上，都放不下则贴左并夹在可用区内
void ImageDrawPad::place(QWidget* anchor)
{
    show();
    adjustSize();
    if (!property("userMoved").toBool() && anchor != nullptr)
    {
        const QSize pad = size();
        const QRect image(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
        const QRect avail = QApplication::desktop()->availableGeometry(anchor);
        const int gap = 8;
        auto fits = [&](const QPoint& at) {
            const QRect box(at, pad);
            return avail.contains(box) && !box.intersects(image);
        };
        QPoint at(image.left() - pad.width() - gap, image.top());
        if (!fits(at))
            at = QPoint(image.right() + gap, image.top());
        if (!fits(at))
            at = QPoint(image.left(), image.bottom() + gap);
        if (!fits(at))
            at = QPoint(image.left(), image.top() - pad.height() - gap);
        if (!fits(at))
        {
            at = QPoint(qMax(avail.left(), image.left() - pad.width() - gap),
                        qBound(avail.top(), image.top(), qMax(avail.top(), avail.bottom() - pad.height())));
        }
        move(at);
    }
    raise();
}

// 弹出并开始首选工具
void ImageDrawPad::popup(QWidget* anchor)
{
    place(anchor);
    startPreferredTool();
}

// 弹出并停在浏览工具
void ImageDrawPad::popupBrowse(QWidget* anchor)
{
    place(anchor);
    startBrowse();
}

// 选中浏览按钮（不重复触发按钮组信号），再按浏览处理
void ImageDrawPad::startBrowse()
{
    if (group_ == nullptr || view_ == nullptr)
        return;
    QAbstractButton* button = group_->button(static_cast<int>(HsCtrl::ToolBrowse));
    if (button == nullptr)
        return;
    group_->blockSignals(true);
    button->setChecked(true);
    group_->blockSignals(false);
    onTool(static_cast<int>(HsCtrl::ToolBrowse));
}

// 首选轴对齐矩形，没有则取第一个非浏览工具
void ImageDrawPad::startPreferredTool()
{
    if (group_ == nullptr || view_ == nullptr)
        return;
    int pick = static_cast<int>(HsCtrl::ToolRect1);
    if (group_->button(pick) == nullptr)
    {
        pick = -1;
        for (HsCtrl::DrawTool tool : tools_)
        {
            if (tool == HsCtrl::ToolBrowse)
                continue;
            pick = static_cast<int>(tool);
            break;
        }
    }
    if (pick < 0 || group_->button(pick) == nullptr)
        return;
    group_->blockSignals(true);
    group_->button(pick)->setChecked(true);
    group_->blockSignals(false);
    onTool(pick);
}

// 选中按钮（若有）并启动工具
void ImageDrawPad::startTool(HsCtrl::DrawTool tool)
{
    if (group_ != nullptr)
    {
        QAbstractButton* button = group_->button(static_cast<int>(tool));
        if (button != nullptr)
        {
            group_->blockSignals(true);
            button->setChecked(true);
            group_->blockSignals(false);
        }
    }
    onTool(static_cast<int>(tool));
}

// 浏览：取消会话回浏览；其他：切设计态，无会话时先清旧 Drawing，开会话失败则只切工具
void ImageDrawPad::onTool(int id)
{
    if (view_ == nullptr)
        return;
    const HsCtrl::DrawTool tool = static_cast<HsCtrl::DrawTool>(id);
    if (tool == HsCtrl::ToolBrowse)
    {
        if (view_->drawSessionActive())
            view_->cancelDrawSession();
        view_->setDrawTool(HsCtrl::ToolBrowse);
        syncSessionButtons();
        return;
    }
    view_->setWindowMode(HsCtrl::WindowDesign);
    if (!view_->drawSessionActive())
        view_->clearDrawings();
    if (!view_->beginDrawSession(tool))
        view_->setDrawTool(tool);
    syncSessionButtons();
}

// 提交会话
void ImageDrawPad::onCommit()
{
    if (view_ != nullptr && view_->drawSessionActive())
        view_->commitDrawSession();
}

// 取消会话
void ImageDrawPad::onCancel()
{
    if (view_ != nullptr && view_->drawSessionActive())
        view_->cancelDrawSession();
}

// 关面板时取消进行中的会话
void ImageDrawPad::closeEvent(QCloseEvent* event)
{
    if (view_ != nullptr && view_->drawSessionActive())
        view_->cancelDrawSession();
    QWidget::closeEvent(event);
}

// 会话结束：按钮与画布都回浏览，再转发结果
void ImageDrawPad::onSessionFinished(const HsCommData::Graphic& roi, bool accepted)
{
    if (group_ != nullptr && group_->button(HsCtrl::ToolBrowse) != nullptr)
        group_->button(HsCtrl::ToolBrowse)->setChecked(true);
    if (view_ != nullptr)
        view_->setDrawTool(HsCtrl::ToolBrowse);
    syncSessionButtons();
    emit drawingFinished(roi, accepted);
}

// 画布已销毁，解绑
void ImageDrawPad::onViewDestroyed()
{
    view_ = nullptr;
    syncSessionButtons();
}

// 重建工具按钮区：每行 4 个，提交 / 取消排在工具后面
void ImageDrawPad::rebuild()
{
    // 首次调用时建按钮区，插在样式区上方
    if (top_ == nullptr)
    {
        top_ = new QWidget(this);
        QGridLayout* topLay = new QGridLayout(top_);
        topLay->setContentsMargins(0, 0, 0, 0);
        topLay->setHorizontalSpacing(4);
        topLay->setVerticalSpacing(4);
        QVBoxLayout* root = qobject_cast<QVBoxLayout*>(layout());
        if (root == nullptr)
        {
            root = new QVBoxLayout(this);
            root->setContentsMargins(8, 8, 8, 8);
            root->setSpacing(8);
        }
        root->insertWidget(0, top_);
    }
    // 删掉旧工具按钮，提交 / 取消保留复用
    QGridLayout* lay = qobject_cast<QGridLayout*>(top_->layout());
    if (lay != nullptr)
    {
        QLayoutItem* item = nullptr;
        while ((item = lay->takeAt(0)) != nullptr)
        {
            if (item->widget() != nullptr && item->widget() != commit_ && item->widget() != cancel_)
                delete item->widget();
            delete item;
        }
    }
    // 按 tools_ 新建互斥工具按钮，按钮 id 即 DrawTool
    delete group_;
    group_ = new QButtonGroup(this);
    group_->setExclusive(true);
    for (size_t i = 0; i < tools_.size(); ++i)
    {
        const HsCtrl::DrawTool tool = tools_[i];
        QToolButton* btn = new QToolButton(top_);
        btn->setText(toolTitle(tool));
        btn->setCheckable(true);
        btn->setAutoRaise(false);
        btn->setMinimumHeight(28);
        btn->setMinimumWidth(68);
        group_->addButton(btn, static_cast<int>(tool));
        const int cell = static_cast<int>(i);
        lay->addWidget(btn, cell / 4, cell % 4);
    }
    // 提交 / 取消只建一次
    if (commit_ == nullptr)
    {
        commit_ = new QToolButton(top_);
        commit_->setObjectName(QStringLiteral("drawPadCommit"));
        commit_->setText(QStringLiteral("完成"));
        commit_->setMinimumHeight(28);
        connect(commit_, &QToolButton::clicked, this, &ImageDrawPad::onCommit);
        cancel_ = new QToolButton(top_);
        cancel_->setText(QStringLiteral("取消"));
        cancel_->setMinimumHeight(28);
        connect(cancel_, &QToolButton::clicked, this, &ImageDrawPad::onCancel);
    }
    const int commitCell = static_cast<int>(tools_.size());
    lay->addWidget(commit_, commitCell / 4, commitCell % 4);
    lay->addWidget(cancel_, (commitCell + 1) / 4, (commitCell + 1) % 4);
    if (group_->button(HsCtrl::ToolBrowse) != nullptr)
        group_->button(HsCtrl::ToolBrowse)->setChecked(true);
    connect(group_, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
            this, &ImageDrawPad::onTool);
    setStyleSheet(QStringLiteral(
        "QToolButton { padding: 2px 8px; border: 1px solid #D9D9D9; border-radius: 4px; background: #FFFFFF; }"
        "QToolButton:checked { color: #1677FF; border: 1px solid #1677FF; background: #E6F4FF; }"
        "QToolButton#drawPadCommit { color: #FFFFFF; background: #1677FF; border: 1px solid #1677FF; }"
        "QToolButton#drawPadCommit:disabled { color: #FFFFFF; background: #91CAFF; border: 1px solid #91CAFF; }"));
    syncSessionButtons();
    armDrag();
}

// 断开旧画布的信号，接上新画布并同步画笔
void ImageDrawPad::bindView(HsCtrl::HsImageWindow* view)
{
    if (view_ != nullptr)
    {
        disconnect(view_, &HsCtrl::HsImageWindow::drawSessionFinished, this, &ImageDrawPad::onSessionFinished);
        disconnect(view_, &QObject::destroyed, this, &ImageDrawPad::onViewDestroyed);
    }
    view_ = view;
    if (view_ != nullptr)
    {
        connect(view_, &HsCtrl::HsImageWindow::drawSessionFinished, this, &ImageDrawPad::onSessionFinished);
        connect(view_, &QObject::destroyed, this, &ImageDrawPad::onViewDestroyed);
        applyPen();
    }
    syncSessionButtons();
}

// 会话中才能提交 / 取消；没绑画布时工具按钮全禁用
void ImageDrawPad::syncSessionButtons()
{
    const bool busy = view_ != nullptr && view_->drawSessionActive();
    if (commit_ != nullptr)
        commit_->setEnabled(busy);
    if (cancel_ != nullptr)
        cancel_->setEnabled(busy);
    if (group_ != nullptr)
    {
        const QList<QAbstractButton*> buttons = group_->buttons();
        for (QAbstractButton* button : buttons)
            button->setEnabled(view_ != nullptr);
    }
}

// 样式区：颜色、线宽、线型、填充四行
void ImageDrawPad::buildStyleRow()
{
    QVBoxLayout* root = qobject_cast<QVBoxLayout*>(layout());
    if (root == nullptr)
    {
        root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(8);
    }
    QWidget* style = new QWidget(this);
    QGridLayout* grid = new QGridLayout(style);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(4);
    // 第 0 行：6 个预设色圆点 + 自定义色按钮
    int col = 0;
    grid->addWidget(new QLabel(QStringLiteral("颜色"), style), 0, col++);
    const QColor colors[6] = {
        QColor(255, 77, 79), QColor(250, 173, 20), QColor(82, 196, 26),
        QColor(22, 119, 255), QColor(255, 255, 255), QColor(0, 0, 0)};
    QButtonGroup* colorsGroup = new QButtonGroup(this);
    colorsGroup->setExclusive(true);
    for (int i = 0; i < 6; ++i)
    {
        QToolButton* dot = new QToolButton(style);
        dot->setCheckable(true);
        dot->setFixedSize(18, 18);
        dot->setProperty("penColor", colors[i]);
        dot->setStyleSheet(QStringLiteral(
            "QToolButton { background:%1; border:1px solid #D0D0D0; border-radius:9px; }"
            "QToolButton:checked { border:2px solid #1677FF; }").arg(colors[i].name()));
        if (colors[i] == color_)
            dot->setChecked(true);
        colorsGroup->addButton(dot);
        connect(dot, &QToolButton::clicked, this, &ImageDrawPad::onPenChanged);
        grid->addWidget(dot, 0, col++);
    }
    QToolButton* more = new QToolButton(style);
    more->setText(QStringLiteral("+"));
    more->setFixedSize(18, 18);
    connect(more, &QToolButton::clicked, this, &ImageDrawPad::onPickColor);
    grid->addWidget(more, 0, col++);
    // 第 1 行：线宽 1..8，默认 2
    grid->addWidget(new QLabel(QStringLiteral("线宽"), style), 1, 0);
    width_ = new QSlider(Qt::Horizontal, style);
    width_->setRange(1, 8);
    width_->setValue(2);
    width_->setFixedWidth(148);
    connect(width_, &QSlider::valueChanged, this, &ImageDrawPad::onPenChanged);
    grid->addWidget(width_, 1, 1, 1, 6);
    // 第 2 行：实线 / 虚线
    grid->addWidget(new QLabel(QStringLiteral("线型"), style), 2, 0);
    QToolButton* solid = new QToolButton(style);
    solid->setText(QStringLiteral("实线"));
    solid->setCheckable(true);
    solid->setChecked(true);
    solid->setProperty("penDash", false);
    QToolButton* dash = new QToolButton(style);
    dash->setText(QStringLiteral("虚线"));
    dash->setCheckable(true);
    dash->setProperty("penDash", true);
    QButtonGroup* lines = new QButtonGroup(this);
    lines->setExclusive(true);
    lines->addButton(solid);
    lines->addButton(dash);
    connect(solid, &QToolButton::clicked, this, &ImageDrawPad::onPenChanged);
    connect(dash, &QToolButton::clicked, this, &ImageDrawPad::onPenChanged);
    grid->addWidget(solid, 2, 1);
    grid->addWidget(dash, 2, 2);
    // 第 3 行：描边 / 填充
    grid->addWidget(new QLabel(QStringLiteral("填充"), style), 3, 0);
    QToolButton* stroke = new QToolButton(style);
    stroke->setText(QStringLiteral("描边"));
    stroke->setCheckable(true);
    stroke->setChecked(true);
    stroke->setProperty("penFill", false);
    QToolButton* fill = new QToolButton(style);
    fill->setText(QStringLiteral("填充"));
    fill->setCheckable(true);
    fill->setProperty("penFill", true);
    QButtonGroup* fills = new QButtonGroup(this);
    fills->setExclusive(true);
    fills->addButton(stroke);
    fills->addButton(fill);
    connect(stroke, &QToolButton::clicked, this, &ImageDrawPad::onPenChanged);
    connect(fill, &QToolButton::clicked, this, &ImageDrawPad::onPenChanged);
    grid->addWidget(stroke, 3, 1);
    grid->addWidget(fill, 3, 2);
    root->addWidget(style);
}

// 写入画布 Drawing 样式：屏幕线宽；填充时用同色 alpha 64 填充，否则只描边
void ImageDrawPad::applyPen()
{
    if (view_ == nullptr)
        return;
    HsCommData::GeomStyle style = view_->drawingStyle();
    style.stroke = HsCommData::Color4(static_cast<unsigned char>(color_.red()),
                                       static_cast<unsigned char>(color_.green()),
                                       static_cast<unsigned char>(color_.blue()), 255);
    style.widthPx = width_ != nullptr ? width_->value() : 2.0;
    style.line = dash_ ? HsCommData::LineDash : HsCommData::LineSolid;
    style.cosmetic = true;
    if (fill_)
    {
        style.draw = HsCommData::DrawFill;
        style.fill = HsCommData::Color4(static_cast<unsigned char>(color_.red()),
                                         static_cast<unsigned char>(color_.green()),
                                         static_cast<unsigned char>(color_.blue()), 64);
    }
    else
    {
        style.draw = HsCommData::DrawMargin;
        style.fill = HsCommData::Color4();
    }
    view_->setDrawingStyle(style);
}

// 从发送按钮的属性读颜色 / 线型 / 填充，线宽直接读滑块
void ImageDrawPad::onPenChanged()
{
    QToolButton* btn = qobject_cast<QToolButton*>(sender());
    if (btn != nullptr && btn->property("penColor").isValid())
        color_ = btn->property("penColor").value<QColor>();
    if (btn != nullptr && btn->property("penDash").isValid())
        dash_ = btn->property("penDash").toBool();
    if (btn != nullptr && btn->property("penFill").isValid())
        fill_ = btn->property("penFill").toBool();
    applyPen();
}

// 取色对话框，取消则不变
void ImageDrawPad::onPickColor()
{
    const QColor picked = QColorDialog::getColor(color_, this);
    if (!picked.isValid())
        return;
    color_ = picked;
    applyPen();
}

// 工具窗口下，在按钮和滑块以外按住左键拖动面板，并记下用户已移动
bool ImageDrawPad::eventFilter(QObject* watched, QEvent* event)
{
    QWidget* widget = qobject_cast<QWidget*>(watched);
    if (widget == nullptr || qobject_cast<QAbstractButton*>(widget) != nullptr ||
        qobject_cast<QSlider*>(widget) != nullptr)
        return false;
    if (!(windowFlags() & Qt::Tool))
        return false;
    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton)
        {
            dragging_ = true;
            dragPos_ = mouse->globalPos() - pos();
        }
        return false;
    }
    if (event->type() == QEvent::MouseMove && dragging_)
    {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->buttons() & Qt::LeftButton)
        {
            move(mouse->globalPos() - dragPos_);
            setProperty("userMoved", true);
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonRelease)
        dragging_ = false;
    return false;
}

// 面板与所有子控件都装上拖动过滤
void ImageDrawPad::armDrag()
{
    installEventFilter(this);
    const QList<QWidget*> kids = findChildren<QWidget*>();
    for (QWidget* kid : kids)
        kid->installEventFilter(this);
}

// 工具按钮文字
QString ImageDrawPad::toolTitle(HsCtrl::DrawTool tool)
{
    switch (tool)
    {
    case HsCtrl::ToolBrowse: return QStringLiteral("浏览");
    case HsCtrl::ToolCircle: return QStringLiteral("圆");
    case HsCtrl::ToolRect1: return QStringLiteral("矩形");
    case HsCtrl::ToolRect2: return QStringLiteral("角度矩形");
    case HsCtrl::ToolEllipse: return QStringLiteral("椭圆");
    case HsCtrl::ToolLine: return QStringLiteral("直线");
    case HsCtrl::ToolContour: return QStringLiteral("轮廓");
    case HsCtrl::ToolPolygon: return QStringLiteral("多边形");
    case HsCtrl::ToolText: return QStringLiteral("文本");
    case HsCtrl::ToolBrush: return QStringLiteral("区域");
    default: return QStringLiteral("浏览");
    }
}

}

#ifdef HSIMAGE_MOC_FROM_INTDIR
#include "moc_image_draw_pad.cpp"
#endif
