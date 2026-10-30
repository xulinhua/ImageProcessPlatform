/**
 * @file RemoteWidgets.cpp
 * @brief 小控件绘制
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteWidgets.h"
#include "RemoteTheme.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>

#include <cstring>

namespace Vision {

SwitchButton::SwitchButton(QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(34, 18);
}

QSize SwitchButton::sizeHint() const
{
    return QSize(34, 18);
}

void SwitchButton::paintEvent(QPaintEvent* event)
{
    (void)event;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor track = isChecked() ? QColor(RemoteTheme::kPrimary) : QColor("#BFBFBF");
    if (!isEnabled()) {
        track = isChecked() ? QColor("#91CAFF") : QColor("#E8E8E8");
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(track);
    painter.drawRoundedRect(QRectF(0, 0, 34, 18), 9, 9);
    painter.setBrush(QColor("#FFFFFF"));
    const qreal x = isChecked() ? 18 : 2;
    painter.drawEllipse(QRectF(x, 2, 14, 14));
}

StatusDot::StatusDot(const char* tone, QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(8, 8);
    setTone(tone);
}

void StatusDot::setTone(const char* tone)
{
    QColor color(RemoteTheme::kSuccess);
    if (std::strcmp(tone, "gray") == 0) {
        color = QColor("#BFBFBF");
    } else if (std::strcmp(tone, "orange") == 0) {
        color = QColor(RemoteTheme::kWarning);
    } else if (std::strcmp(tone, "red") == 0) {
        color = QColor(RemoteTheme::kDanger);
    } else if (std::strcmp(tone, "blue") == 0) {
        color = QColor(RemoteTheme::kPrimary);
    }
    if (color != m_color) {
        m_color = color;
        update();
    }
}

QSize StatusDot::sizeHint() const
{
    return QSize(8, 8);
}

void StatusDot::paintEvent(QPaintEvent* event)
{
    (void)event;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_color);
    painter.drawEllipse(rect());
}

TagLabel::TagLabel(const QString& text, const char* tone, QWidget* parent)
    : QLabel(text, parent)
{
    setProperty("tone", QString::fromLatin1(tone));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void TagLabel::setTone(const char* tone)
{
    if (property("tone").toString() == QLatin1String(tone)) {
        return;
    }
    RemoteTheme::setProp(this, "tone", QString::fromLatin1(tone));
}

SegmentBar::SegmentBar(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
    , m_group(new QButtonGroup(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_group->setExclusive(true);
    QObject::connect(m_group, &QButtonGroup::idClicked, [this](int id) {
        if (m_changed) {
            m_changed(id);
        }
    });
}

int SegmentBar::addButton(const QString& text, const char* iconName)
{
    QPushButton* button = RemoteTheme::button(text, iconName, 0, this);
    button->setCheckable(true);
    if (text.isEmpty()) {
        button->setProperty("kind", "icon");
    }
    const int id = static_cast<int>(m_buttons.size());
    m_group->addButton(button, id);
    m_layout->addWidget(button);
    m_buttons.push_back(button);
    restyle();
    return id;
}

void SegmentBar::clear()
{
    for (size_t i = 0; i < m_buttons.size(); ++i) {
        m_group->removeButton(m_buttons[i]);
        delete m_buttons[i];
    }
    m_buttons.clear();
}

int SegmentBar::count() const
{
    return static_cast<int>(m_buttons.size());
}

int SegmentBar::current() const
{
    return m_group->checkedId();
}

void SegmentBar::setCurrent(int index)
{
    if (index < 0 || index >= count()) {
        QAbstractButton* checked = m_group->checkedButton();
        if (checked) {
            m_group->setExclusive(false);
            checked->setChecked(false);
            m_group->setExclusive(true);
        }
        return;
    }
    m_buttons[static_cast<size_t>(index)]->setChecked(true);
}

void SegmentBar::setButtonEnabled(int index, bool enabled, const QString& tip)
{
    if (index < 0 || index >= count()) {
        return;
    }
    m_buttons[static_cast<size_t>(index)]->setEnabled(enabled);
    m_buttons[static_cast<size_t>(index)]->setToolTip(tip);
}

void SegmentBar::setOnChanged(const std::function<void(int)>& changed)
{
    m_changed = changed;
}

void SegmentBar::restyle()
{
    const size_t n = m_buttons.size();
    for (size_t i = 0; i < n; ++i) {
        const char* seg = "mid";
        if (n == 1) {
            seg = "";
        } else if (i == 0) {
            seg = "first";
        } else if (i + 1 == n) {
            seg = "last";
        }
        RemoteTheme::setProp(m_buttons[i], "seg", QString::fromLatin1(seg));
    }
}

ToolSeparator::ToolSeparator(QWidget* parent)
    : QFrame(parent)
{
    setObjectName("sep");
    setFixedSize(1, 22);
}

AvatarLabel::AvatarLabel(const QString& name, int index, int side, QWidget* parent)
    : QLabel(name.left(1), parent)
{
    static const char* const kColors[] = { "#1890FF", "#13C2C2", "#FA8C16", "#722ED1", "#52C41A" };
    const char* color = kColors[(index < 0 ? 0 : index) % 5];
    setFixedSize(side, side);
    setAlignment(Qt::AlignCenter);
    setStyleSheet(QString("background:%1; color:#FFFFFF; border-radius:%2px; font-size:12px;").arg(color).arg(side / 2));
}

}
