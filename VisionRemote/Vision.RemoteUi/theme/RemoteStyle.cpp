/**
 * @file RemoteStyle.cpp
 * @brief 14 像素方框和圆框，选中为主色
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteStyle.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>

namespace Vision {

RemoteStyle::RemoteStyle(QStyle* base)
    : QProxyStyle(base)
{
}

int RemoteStyle::pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const
{
    switch (metric) {
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return 14;
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return 6;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

int RemoteStyle::styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
    QStyleHintReturn* returnData) const
{
    if (hint == SH_ItemView_ShowDecorationSelected) {
        return 1;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void RemoteStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
    const QWidget* widget) const
{
    if (element == PE_IndicatorCheckBox || element == PE_IndicatorItemViewItemCheck) {
        drawCheck(option, painter);
        return;
    }
    if (element == PE_IndicatorRadioButton) {
        drawRadio(option, painter);
        return;
    }
    if (element == PE_FrameFocusRect) {
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void RemoteStyle::drawCheck(const QStyleOption* option, QPainter* painter) const
{
    const bool enabled = option->state & State_Enabled;
    const bool on = option->state & State_On;
    const bool partial = option->state & State_NoChange;
    const bool hover = option->state & State_MouseOver;
    QRectF box(0, 0, 14, 14);
    box.moveCenter(QRectF(option->rect).center());
    box.adjust(0.5, 0.5, -0.5, -0.5);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QColor fill("#FFFFFF");
    QColor border(hover && enabled ? "#1890FF" : "#BFBFBF");
    if (on || partial) {
        fill = enabled ? QColor("#1890FF") : QColor("#D9D9D9");
        border = fill;
    } else if (!enabled) {
        fill = QColor("#F5F5F5");
        border = QColor("#D9D9D9");
    }
    painter->setPen(QPen(border, 1));
    painter->setBrush(fill);
    painter->drawRoundedRect(box, 3, 3);
    QPen mark(QColor("#FFFFFF"), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(mark);
    if (on) {
        QPainterPath path;
        path.moveTo(box.left() + 3.2, box.center().y() + 0.2);
        path.lineTo(box.left() + 5.8, box.bottom() - 3.6);
        path.lineTo(box.right() - 3.0, box.top() + 3.6);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    } else if (partial) {
        painter->drawLine(QPointF(box.left() + 3.5, box.center().y()), QPointF(box.right() - 3.5, box.center().y()));
    }
    painter->restore();
}

void RemoteStyle::drawRadio(const QStyleOption* option, QPainter* painter) const
{
    const bool enabled = option->state & State_Enabled;
    const bool on = option->state & State_On;
    const bool hover = option->state & State_MouseOver;
    QRectF box(0, 0, 14, 14);
    box.moveCenter(QRectF(option->rect).center());
    box.adjust(0.5, 0.5, -0.5, -0.5);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QColor border(on || (hover && enabled) ? "#1890FF" : "#BFBFBF");
    if (!enabled) {
        border = QColor("#D9D9D9");
    }
    painter->setPen(QPen(border, 1));
    painter->setBrush(enabled ? QColor("#FFFFFF") : QColor("#F5F5F5"));
    painter->drawEllipse(box);
    if (on) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(enabled ? QColor("#1890FF") : QColor("#BFBFBF"));
        QRectF dot(0, 0, 6, 6);
        dot.moveCenter(box.center());
        painter->drawEllipse(dot);
    }
    painter->restore();
}

}
