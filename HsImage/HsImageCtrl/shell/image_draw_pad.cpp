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
std::vector<HsCtrl::DrawTool> defaultTools()
{
    return {HsCtrl::ToolBrowse, HsCtrl::ToolCircle, HsCtrl::ToolRect1, HsCtrl::ToolRect2,
            HsCtrl::ToolEllipse, HsCtrl::ToolLine, HsCtrl::ToolContour, HsCtrl::ToolPolygon};
}
}

ImageDrawPad::ImageDrawPad(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("imageDrawPad"));
    setWindowTitle(QStringLiteral("????"));
    setWindowFlags(Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    buildStyleRow();
    tools_ = defaultTools();
    rebuild();
}

void ImageDrawPad::setImageWindow(HsCtrl::HsImageWindow* view)
{
    if (view_ == view)
        return;
    bindView(view);
}

HsCtrl::HsImageWindow* ImageDrawPad::imageWindow() const
{
    return view_;
}

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

void ImageDrawPad::popup(QWidget* anchor)
{
    place(anchor);
    startPreferredTool();
}

void ImageDrawPad::popupBrowse(QWidget* anchor)
{
    place(anchor);
    startBrowse();
}

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

void ImageDrawPad::onCommit()
{
    if (view_ != nullptr && view_->drawSessionActive())
        view_->commitDrawSession();
}

void ImageDrawPad::onCancel()
{
    if (view_ != nullptr && view_->drawSessionActive())
        view_->cancelDrawSession();
}

void ImageDrawPad::closeEvent(QCloseEvent* event)
{
    if (view_ != nullptr && view_->drawSessionActive())
        view_->cancelDrawSession();
    QWidget::closeEvent(event);
}

void ImageDrawPad::onSessionFinished(const HsCommData::Graphic& roi, bool accepted)
{
    if (group_ != nullptr && group_->button(HsCtrl::ToolBrowse) != nullptr)
        group_->button(HsCtrl::ToolBrowse)->setChecked(true);
    if (view_ != nullptr)
        view_->setDrawTool(HsCtrl::ToolBrowse);
    syncSessionButtons();
    emit drawingFinished(roi, accepted);
}

void ImageDrawPad::onViewDestroyed()
{
    view_ = nullptr;
    syncSessionButtons();
}

void ImageDrawPad::rebuild()
{
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
    if (commit_ == nullptr)
    {
        commit_ = new QToolButton(top_);
        commit_->setObjectName(QStringLiteral("drawPadCommit"));
        commit_->setText(QStringLiteral("???"));
        commit_->setMinimumHeight(28);
        connect(commit_, &QToolButton::clicked, this, &ImageDrawPad::onCommit);
        cancel_ = new QToolButton(top_);
        cancel_->setText(QStringLiteral("???"));
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
    int col = 0;
    grid->addWidget(new QLabel(QStringLiteral("???"), style), 0, col++);
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
    grid->addWidget(new QLabel(QStringLiteral("???"), style), 1, 0);
    width_ = new QSlider(Qt::Horizontal, style);
    width_->setRange(1, 8);
    width_->setValue(2);
    width_->setFixedWidth(148);
    connect(width_, &QSlider::valueChanged, this, &ImageDrawPad::onPenChanged);
    grid->addWidget(width_, 1, 1, 1, 6);
    grid->addWidget(new QLabel(QStringLiteral("????"), style), 2, 0);
    QToolButton* solid = new QToolButton(style);
    solid->setText(QStringLiteral("???"));
    solid->setCheckable(true);
    solid->setChecked(true);
    solid->setProperty("penDash", false);
    QToolButton* dash = new QToolButton(style);
    dash->setText(QStringLiteral("????"));
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
    grid->addWidget(new QLabel(QStringLiteral("???"), style), 3, 0);
    QToolButton* stroke = new QToolButton(style);
    stroke->setText(QStringLiteral("???"));
    stroke->setCheckable(true);
    stroke->setChecked(true);
    stroke->setProperty("penFill", false);
    QToolButton* fill = new QToolButton(style);
    fill->setText(QStringLiteral("???"));
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

void ImageDrawPad::onPickColor()
{
    const QColor picked = QColorDialog::getColor(color_, this);
    if (!picked.isValid())
        return;
    color_ = picked;
    applyPen();
}

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

void ImageDrawPad::armDrag()
{
    installEventFilter(this);
    const QList<QWidget*> kids = findChildren<QWidget*>();
    for (QWidget* kid : kids)
        kid->installEventFilter(this);
}

QString ImageDrawPad::toolTitle(HsCtrl::DrawTool tool)
{
    switch (tool)
    {
    case HsCtrl::ToolBrowse: return QStringLiteral("???");
    case HsCtrl::ToolCircle: return QStringLiteral("?");
    case HsCtrl::ToolRect1: return QStringLiteral("????");
    case HsCtrl::ToolRect2: return QStringLiteral("??????");
    case HsCtrl::ToolEllipse: return QStringLiteral("???");
    case HsCtrl::ToolLine: return QStringLiteral("???");
    case HsCtrl::ToolContour: return QStringLiteral("????");
    case HsCtrl::ToolPolygon: return QStringLiteral("?????");
    case HsCtrl::ToolText: return QStringLiteral("???");
    case HsCtrl::ToolBrush: return QStringLiteral("????");
    default: return QStringLiteral("???");
    }
}

}

#ifdef HSIMAGE_MOC_FROM_INTDIR
#include "moc_image_draw_pad.cpp"
#endif
