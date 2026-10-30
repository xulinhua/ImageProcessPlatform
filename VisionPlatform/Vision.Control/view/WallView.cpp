/**
 * @file WallView.cpp
 * @brief 监控墙绘制与交互
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "WallView.h"
#include "RemoteTheme.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace Vision {

const char* const WallView::kMime = "application/x-vision-remote-device";

WallView::WallView(QWidget* parent)
    : QWidget(parent)
    , m_rows(3)
    , m_cols(3)
    , m_hover(-1)
    , m_hoverAction(-1)
    , m_drop(-1)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void WallView::setLayoutCount(int rows, int cols)
{
    m_rows = rows < 1 ? 1 : rows;
    m_cols = cols < 1 ? 1 : cols;
    update();
}

void WallView::setCells(const std::vector<Cell>& cells)
{
    m_cells = cells;
    update();
}

void WallView::setOnAction(const std::function<void(const QString&, int)>& action)
{
    m_action = action;
}

void WallView::setOnDrop(const std::function<void(const QString&, int)>& drop)
{
    m_dropTo = drop;
}

void WallView::setOnEmpty(const std::function<void()>& empty)
{
    m_empty = empty;
}

void WallView::setOnContext(const std::function<void(const QString&, const QPoint&)>& context)
{
    m_context = context;
}

QRect WallView::cellRect(int index) const
{
    const int pad = 12;
    const int gap = 10;
    const int cellW = (width() - pad * 2 - gap * (m_cols - 1)) / m_cols;
    const int cellH = (height() - pad * 2 - gap * (m_rows - 1)) / m_rows;
    const int row = index / m_cols;
    const int col = index % m_cols;
    return QRect(pad + col * (cellW + gap), pad + row * (cellH + gap), cellW, cellH);
}

QRect WallView::bodyRect(const QRect& cell) const
{
    return QRect(cell.left() + 1, cell.top() + 31, cell.width() - 2, cell.height() - 32);
}

QRect WallView::actionRect(const QRect& cell, int action) const
{
    const QRect body = bodyRect(cell);
    const int side = 28;
    const int right = body.right() - 8;
    return QRect(right - (4 - action) * (side + 4) + 5, body.top() + 8, side, side);
}

int WallView::cellAt(const QPoint& point) const
{
    const int count = m_rows * m_cols;
    for (int i = 0; i < count; ++i) {
        if (cellRect(i).contains(point)) {
            return i;
        }
    }
    return -1;
}

int WallView::actionAt(int cell, const QPoint& point) const
{
    if (cell < 0 || cell >= static_cast<int>(m_cells.size()) || !m_cells[static_cast<size_t>(cell)].online) {
        return -1;
    }
    const QRect box = cellRect(cell);
    for (int a = 0; a < 4; ++a) {
        if (actionRect(box, a).contains(point)) {
            return a;
        }
    }
    return -1;
}

void WallView::paintEvent(QPaintEvent* event)
{
    (void)event;
    QPainter painter(this);
    painter.fillRect(rect(), QColor(RemoteTheme::kPage));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const int count = m_rows * m_cols;
    for (int i = 0; i < count; ++i) {
        if (i < static_cast<int>(m_cells.size()) && !m_cells[static_cast<size_t>(i)].key.isEmpty()) {
            paintCell(painter, i);
        } else {
            paintEmpty(painter, cellRect(i), i == m_drop);
        }
    }
}

void WallView::paintEmpty(QPainter& painter, const QRect& box, bool drop)
{
    QPen pen(drop ? QColor(RemoteTheme::kPrimary) : QColor("#BFC7D2"), drop ? 2 : 1, Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(drop ? QColor("#F0F8FF") : QColor("#FAFBFC"));
    painter.drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
    const QPixmap plus = RemoteTheme::pixmap("plus", QColor("#BFBFBF"), 26, devicePixelRatioF());
    const QPoint center = box.center();
    painter.drawPixmap(center.x() - 13, center.y() - 24, plus);
    painter.setPen(QColor(RemoteTheme::kMuted));
    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);
    painter.drawText(QRect(box.left(), center.y() + 8, box.width(), 20), Qt::AlignCenter,
        QString::fromUtf8("拖入设备，或双击添加"));
}

void WallView::paintCell(QPainter& painter, int index)
{
    const Cell& cell = m_cells[static_cast<size_t>(index)];
    const QRect box = cellRect(index);
    const QRectF outer = QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath clip;
    clip.addRoundedRect(outer, 6, 6);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#FFFFFF"));
    painter.drawPath(clip);

    // 画面区：等比居中；离线时盖半透明黑底和最后在线时间
    const QRect body = bodyRect(box);
    painter.save();
    painter.setClipPath(clip);
    painter.fillRect(body, QColor("#0E1116"));
    if (!cell.image.isNull()) {
        const double sx = body.width() / static_cast<double>(cell.image.width());
        const double sy = body.height() / static_cast<double>(cell.image.height());
        const double scale = sx < sy ? sx : sy;
        const int w = static_cast<int>(cell.image.width() * scale);
        const int h = static_cast<int>(cell.image.height() * scale);
        const QRect target(body.left() + (body.width() - w) / 2, body.top() + (body.height() - h) / 2, w, h);
        painter.drawImage(target, cell.image);
    }
    if (!cell.online) {
        painter.fillRect(body, QColor(0, 0, 0, 140));
        painter.setPen(QColor("#FFFFFF"));
        QFont big = painter.font();
        big.setPixelSize(14);
        painter.setFont(big);
        painter.drawText(QRect(body.left(), body.center().y() - 22, body.width(), 22), Qt::AlignCenter,
            QString::fromUtf8("设备离线，等待重连…"));
        QFont small = painter.font();
        small.setPixelSize(12);
        painter.setFont(small);
        painter.setPen(QColor("#D9D9D9"));
        if (!cell.lastSeen.isEmpty()) {
            painter.drawText(QRect(body.left(), body.center().y() + 2, body.width(), 20), Qt::AlignCenter,
                QString::fromUtf8("最后在线 %1").arg(cell.lastSeen));
        }
    }
    if (cell.online && cell.alarm) {
        painter.setPen(QColor("#FFFFFF"));
        painter.setBrush(QColor("#CF1322"));
        const QRect badge(body.right() - 54, body.bottom() - 26, 46, 18);
        painter.drawRoundedRect(badge, 3, 3);
        painter.drawText(badge, Qt::AlignCenter, QString::fromUtf8("NG"));
    }
    painter.restore();

    // 标题栏：状态点、名称、共享种类标签、右侧帧率
    painter.setPen(QColor("#F0F0F0"));
    painter.drawLine(box.left() + 1, box.top() + 30, box.right() - 1, box.top() + 30);

    QFont font = painter.font();
    font.setPixelSize(13);
    const QColor dot = cell.online ? QColor(RemoteTheme::kSuccess) : QColor(RemoteTheme::kDanger);
    painter.setPen(Qt::NoPen);
    painter.setBrush(dot);
    painter.drawEllipse(QRectF(box.left() + 10, box.top() + 11, 8, 8));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(RemoteTheme::kText));
    const QFontMetrics boldMetrics(font);
    const int nameW = boldMetrics.horizontalAdvance(cell.name);
    painter.drawText(QRect(box.left() + 26, box.top(), nameW + 2, 30), Qt::AlignVCenter | Qt::AlignLeft, cell.name);

    font.setBold(false);
    font.setPixelSize(11);
    painter.setFont(font);
    const QString tag = cell.online ? cell.share : QString::fromUtf8("离线");
    const QFontMetrics small(font);
    const QRect tagRect(box.left() + 26 + nameW + 8, box.top() + 5, small.horizontalAdvance(tag) + 12, 20);
    painter.setPen(Qt::NoPen);
    painter.setBrush(cell.online ? QColor("#F0F0F0") : QColor("#FFF1F0"));
    painter.drawRoundedRect(tagRect, 3, 3);
    painter.setPen(cell.online ? QColor("#595959") : QColor("#CF1322"));
    painter.drawText(tagRect, Qt::AlignCenter, tag);
    painter.setPen(QColor(RemoteTheme::kMuted));
    painter.drawText(QRect(box.right() - 90, box.top(), 80, 30), Qt::AlignVCenter | Qt::AlignRight,
        cell.online ? cell.fps : QString::fromUtf8("—"));

    // 左下角操作中徽标
    if (cell.online && !cell.badge.isEmpty()) {
        font.setPixelSize(12);
        painter.setFont(font);
        const QFontMetrics badgeMetrics(font);
        const int w = badgeMetrics.horizontalAdvance(cell.badge) + 38;
        const QRectF pill(body.left() + 10, body.bottom() - 34, w, 24);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(24, 144, 255, 235));
        painter.drawRoundedRect(pill, 12, 12);
        painter.drawPixmap(QPointF(pill.left() + 10, pill.top() + 6), RemoteTheme::pixmap("lock", QColor("#FFFFFF"), 12, devicePixelRatioF()));
        painter.setPen(QColor("#FFFFFF"));
        painter.drawText(pill.adjusted(26, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, cell.badge);
    }

    // 悬停按钮；文件按钮始终灰色不高亮
    if (cell.online && index == m_hover) {
        static const char* const kIcons[] = { "eye", "mouse", "folder", "full" };
        for (int a = 0; a < 4; ++a) {
            const QRect r = actionRect(box, a);
            painter.setPen(Qt::NoPen);
            painter.setBrush(a == m_hoverAction ? QColor(24, 144, 255, 230) : QColor(0, 0, 0, 158));
            painter.drawRoundedRect(r, 4, 4);
            const QColor color = QColor("#FFFFFF");
            painter.drawPixmap(r.left() + 7, r.top() + 7, RemoteTheme::pixmap(kIcons[a], color, 14, devicePixelRatioF()));
        }
    }

    // 边框优先级：拖放目标 > 离线红 > 操作中蓝 > 悬停浅蓝 > 默认灰
    QColor border("#E1E5EB");
    qreal width = 1;
    if (!cell.online) {
        border = QColor(RemoteTheme::kDanger);
        width = 2;
    } else if (cell.operating) {
        border = QColor(RemoteTheme::kPrimary);
        width = 2;
    } else if (index == m_hover) {
        border = QColor("#91CAFF");
        width = 2;
    }
    if (index == m_drop) {
        border = QColor(RemoteTheme::kPrimary);
        width = 2;
    }
    painter.setPen(QPen(border, width));
    painter.setBrush(Qt::NoBrush);
    const qreal inset = width / 2.0;
    painter.drawRoundedRect(QRectF(box).adjusted(inset, inset, -inset, -inset), 6, 6);
}

void WallView::mouseMoveEvent(QMouseEvent* event)
{
    const int hover = cellAt(event->pos());
    const int action = actionAt(hover, event->pos());
    if (hover != m_hover || action != m_hoverAction) {
        m_hover = hover;
        m_hoverAction = action;
        setCursor(action >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (action >= 0) {
            static const char* const kTips[] = { "只看", "操作", "文件", "全屏" };
            setToolTip(QString::fromUtf8(kTips[action]));
        } else {
            setToolTip(QString());
        }
        update();
    }
}

void WallView::leaveEvent(QEvent* event)
{
    (void)event;
    m_hover = -1;
    m_hoverAction = -1;
    update();
}

void WallView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const int cell = cellAt(event->pos());
    const int action = actionAt(cell, event->pos());
    if (action < 0 || !m_action) {
        return;
    }
    m_action(m_cells[static_cast<size_t>(cell)].key, action);
}

void WallView::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int cell = cellAt(event->pos());
    if (cell < 0) {
        return;
    }
    if (actionAt(cell, event->pos()) >= 0) {
        return;
    }
    if (cell >= static_cast<int>(m_cells.size()) || m_cells[static_cast<size_t>(cell)].key.isEmpty()) {
        if (m_empty) {
            m_empty();
        }
        return;
    }
    if (m_action && m_cells[static_cast<size_t>(cell)].online) {
        m_action(m_cells[static_cast<size_t>(cell)].key, ActView);
    }
}

void WallView::contextMenuEvent(QContextMenuEvent* event)
{
    const int cell = cellAt(event->pos());
    if (cell < 0 || cell >= static_cast<int>(m_cells.size()) || m_cells[static_cast<size_t>(cell)].key.isEmpty()) {
        return;
    }
    if (m_context) {
        m_context(m_cells[static_cast<size_t>(cell)].key, event->globalPos());
    }
}

void WallView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(kMime)) {
        event->acceptProposedAction();
    }
}

void WallView::dragMoveEvent(QDragMoveEvent* event)
{
    if (!event->mimeData()->hasFormat(kMime)) {
        return;
    }
    const int cell = cellAt(event->pos());
    if (cell != m_drop) {
        m_drop = cell;
        update();
    }
    event->acceptProposedAction();
}

void WallView::dragLeaveEvent(QDragLeaveEvent* event)
{
    (void)event;
    m_drop = -1;
    update();
}

void WallView::dropEvent(QDropEvent* event)
{
    const int cell = cellAt(event->pos());
    m_drop = -1;
    update();
    if (cell < 0 || !event->mimeData()->hasFormat(kMime) || !m_dropTo) {
        return;
    }
    m_dropTo(QString::fromUtf8(event->mimeData()->data(kMime)), cell);
    event->acceptProposedAction();
}

}
