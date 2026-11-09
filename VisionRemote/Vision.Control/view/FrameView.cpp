/**
 * @file FrameView.cpp
 * @brief 画面控件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FrameView.h"

#include <vector>
#include "RemoteLog.h"
#include "RemoteTheme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cstring>

namespace Vision {

FrameView::FrameView(QWidget* parent)
    : QWidget(parent)
    , m_imageW(0)
    , m_imageH(0)
    , m_cursorX(0)
    , m_cursorY(0)
    , m_cursorOn(false)
    , m_cursorHotX(0)
    , m_cursorHotY(0)
    , m_lastCodec(0)
    , m_input(false)
    , m_fit(true)
    , m_operating(false)
    , m_buttons(0)
    , m_lastX(0)
    , m_lastY(0)
    , m_watermarkOn(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(480, 270);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void FrameView::setRemoteCursor(int x, int y, bool visible)
{
    if (m_cursorX == x && m_cursorY == y && m_cursorOn == visible) {
        return;
    }
    m_cursorX = x;
    m_cursorY = y;
    m_cursorOn = visible;
    update();
}

void FrameView::setCursorShape(const CommBytes& png, int hotX, int hotY)
{
    m_cursorHotX = hotX;
    m_cursorHotY = hotY;
    m_cursorImage = png.empty() ? QImage() : QImage::fromData(png.data(), static_cast<int>(png.size()), "PNG");
    update();
}

void FrameView::setImage(const CommBytes& jpeg, uint32_t imageW, uint32_t imageH, uint8_t codec)
{
    const uint64_t t0 = RemoteLog::tickMs();
    m_imageW = imageW;
    m_imageH = imageH;
    const char* pathName = "空";
    bool ok = false;
    const uint8_t useCodec = codec == 0 ? 1 : codec;
    // 编码切换时丢掉上一模式的静帧与解码器状态，避免原图→H.264 一直卡旧图
    if (m_lastCodec != 0 && m_lastCodec != useCodec) {
        m_h264.closeDecoder();
        m_image = QImage();
    }
    m_lastCodec = useCodec;
    if (jpeg.empty()) {
        m_image = QImage();
        pathName = "空包";
    } else if (useCodec == 2) {
        std::vector<unsigned char> bgra;
        int w = 0;
        int h = 0;
        if (m_h264.decode(jpeg.data(), static_cast<int>(jpeg.size()), &bgra, &w, &h) && w > 0 && h > 0) {
            m_image = RemoteTheme::imageFromBgra(bgra.data(), static_cast<int>(bgra.size()), w, h);
            m_imageW = static_cast<uint32_t>(w);
            m_imageH = static_cast<uint32_t>(h);
            pathName = "H264";
            ok = !m_image.isNull();
        } else {
            // 非关键帧 / 还需更多 NAL 时保留解码器状态；只有编码切换才 closeDecoder
            pathName = "H264等待";
        }
    } else if (useCodec == 3) {
        m_h264.closeDecoder();
        m_image = RemoteTheme::imageFromBgra(jpeg.data(), static_cast<int>(jpeg.size()),
            static_cast<int>(imageW), static_cast<int>(imageH));
        pathName = m_image.isNull() ? "原图失败" : "原图";
        ok = !m_image.isNull();
    } else {
        m_h264.closeDecoder();
        m_image = RemoteTheme::imageFromJpeg(jpeg.data(), static_cast<int>(jpeg.size()));
        if (!m_image.isNull() && (m_imageW == 0 || m_imageH == 0)) {
            m_imageW = static_cast<uint32_t>(m_image.width());
            m_imageH = static_cast<uint32_t>(m_image.height());
        }
        pathName = "JPEG";
        ok = !m_image.isNull();
    }
    const uint64_t decodeMs = RemoteLog::tickMs() - t0;
    RemoteLog::writef(ok ? RemoteLog::Info : RemoteLog::Warn, "解码",
        "%s %llums %zuB → %ux%u",
        pathName,
        static_cast<unsigned long long>(decodeMs),
        jpeg.size(),
        m_imageW,
        m_imageH);
    update();
}

const QImage& FrameView::image() const
{
    return m_image;
}

void FrameView::setInputEnabled(bool enabled)
{
    if (m_input && !enabled) {
        releaseMouse();
        releaseKeys();
    }
    m_input = enabled;
    setCursor(Qt::ArrowCursor);
}

void FrameView::setFit(bool fit)
{
    m_fit = fit;
    update();
}

void FrameView::setOperating(bool operating)
{
    if (m_operating != operating) {
        m_operating = operating;
        update();
    }
}

void FrameView::setWatermark(bool on, const QString& text)
{
    if (m_watermarkOn == on && m_watermarkText == text) {
        return;
    }
    m_watermarkOn = on;
    m_watermarkText = text;
    update();
}

void FrameView::setNotice(const QString& text, const char* tone)
{
    const QColor color = std::strcmp(tone, "orange") == 0 ? QColor(250, 140, 22, 242) : QColor(24, 144, 255, 242);
    if (text == m_notice && color == m_noticeColor) {
        return;
    }
    m_notice = text;
    m_noticeColor = color;
    update();
}

void FrameView::setToggle(const std::function<void()>& toggle)
{
    m_toggle = toggle;
}

void FrameView::setSender(const std::function<void(const InputMsg&)>& sender)
{
    m_sender = sender;
}

QRect FrameView::imageRect() const
{
    if (m_imageW == 0 || m_imageH == 0) {
        return QRect();
    }
    const QRect area = rect().adjusted(16, 16, -16, -16);
    const double scale = m_fit
        ? std::min(area.width() / static_cast<double>(m_imageW), area.height() / static_cast<double>(m_imageH))
        : 1.0;
    if (scale <= 0) {
        return QRect();
    }
    const int drawW = static_cast<int>(m_imageW * scale);
    const int drawH = static_cast<int>(m_imageH * scale);
    return QRect((width() - drawW) / 2, (height() - drawH) / 2, drawW, drawH);
}

void FrameView::paintEvent(QPaintEvent* event)
{
    (void)event;
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#15181D"));
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRect target = imageRect();
    if (!m_image.isNull() && target.isValid()) {
        painter.drawImage(target, m_image);
        if (m_cursorOn && m_imageW > 0 && m_imageH > 0) {
            const double sx = target.width() / static_cast<double>(m_imageW);
            const double sy = target.height() / static_cast<double>(m_imageH);
            const QPointF tip(target.left() + m_cursorX * sx, target.top() + m_cursorY * sy);
            if (!m_cursorImage.isNull()) {
                const QRectF box(tip.x() - m_cursorHotX * sx, tip.y() - m_cursorHotY * sy,
                    m_cursorImage.width() * sx, m_cursorImage.height() * sy);
                painter.drawImage(box, m_cursorImage);
            } else {
                QPainterPath arrow;
                arrow.moveTo(tip);
                arrow.lineTo(tip + QPointF(0, 16));
                arrow.lineTo(tip + QPointF(4, 12));
                arrow.lineTo(tip + QPointF(8, 18));
                arrow.lineTo(tip + QPointF(11, 16));
                arrow.lineTo(tip + QPointF(7, 10));
                arrow.lineTo(tip + QPointF(13, 10));
                arrow.closeSubpath();
                painter.setPen(QPen(QColor("#000000"), 1));
                painter.setBrush(QColor("#FFFFFF"));
                painter.drawPath(arrow);
            }
        }
        if (m_operating) {
            painter.setPen(QPen(QColor(RemoteTheme::kPrimary), 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(target).adjusted(-1, -1, 1, 1));
        }
        if (m_watermarkOn && !m_watermarkText.isEmpty()) {
            painter.save();
            painter.setPen(QColor(255, 255, 255, 70));
            QFont wm = painter.font();
            wm.setPixelSize(22);
            wm.setBold(true);
            painter.setFont(wm);
            painter.translate(target.center());
            painter.rotate(-24);
            painter.drawText(QRect(-target.width(), -target.height() / 2, target.width() * 2, target.height()),
                Qt::AlignCenter, m_watermarkText);
            painter.restore();
        }
    } else {
        painter.setPen(QColor("#8C8C8C"));
        painter.drawText(rect(), Qt::AlignCenter, QString::fromUtf8("等待画面…"));
    }
    if (!m_notice.isEmpty()) {
        painter.setRenderHint(QPainter::Antialiasing);
        QFont font = painter.font();
        font.setPixelSize(12);
        painter.setFont(font);
        const int textW = QFontMetrics(font).horizontalAdvance(m_notice);
        const QRectF pill((width() - textW - 44) / 2.0, 12, textW + 44, 26);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_noticeColor);
        painter.drawRoundedRect(pill, 13, 13);
        painter.drawPixmap(QPointF(pill.left() + 14, pill.top() + 6.5),
            RemoteTheme::pixmap("mouse", QColor("#FFFFFF"), 13, devicePixelRatioF()));
        painter.setPen(QColor("#FFFFFF"));
        painter.drawText(pill.adjusted(32, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, m_notice);
    }
}

bool FrameView::mapPoint(const QPoint& point, bool clamp, uint16_t* x, uint16_t* y) const
{
    const QRect target = imageRect();
    if (!target.isValid() || target.width() <= 0 || target.height() <= 0) {
        return false;
    }
    const double scale = target.width() / static_cast<double>(m_imageW);
    double ix = (point.x() - target.left()) / scale;
    double iy = (point.y() - target.top()) / scale;
    if (ix < 0 || iy < 0 || ix >= m_imageW || iy >= m_imageH) {
        if (!clamp) {
            return false;
        }
        ix = std::min(std::max(ix, 0.0), m_imageW - 1.0);
        iy = std::min(std::max(iy, 0.0), m_imageH - 1.0);
    }
    *x = static_cast<uint16_t>(ix);
    *y = static_cast<uint16_t>(iy);
    return true;
}

uint8_t FrameView::buttonBit(Qt::MouseButton button)
{
    if (button == Qt::RightButton) {
        return 2;
    }
    if (button == Qt::MiddleButton) {
        return 4;
    }
    return 1;
}

void FrameView::sendMouse(const QPoint& point, uint8_t action, uint8_t buttons, int16_t wheel)
{
    if (!m_sender || !m_input) {
        return;
    }
    // 只为已发过按下的按钮发抬起；按住键期间的移动与抬起在画面外时钳到边界，抬起连画面都没有时用最后位置
    const bool release = action == InputMsg::ActionRelease;
    if (release && (m_buttons & buttons) == 0) {
        return;
    }
    const bool held = release || (action == InputMsg::ActionMove && wheel == 0 && m_buttons != 0);
    InputMsg msg;
    if (!mapPoint(point, held, &msg.x, &msg.y)) {
        if (!release) {
            return;
        }
        msg.x = m_lastX;
        msg.y = m_lastY;
    }
    msg.action = action;
    msg.buttons = buttons;
    msg.down = action == InputMsg::ActionPress ? 1 : 0;
    msg.wheel = wheel;
    m_sender(msg);
    m_lastX = msg.x;
    m_lastY = msg.y;
    if (action == InputMsg::ActionPress) {
        m_buttons = static_cast<uint8_t>(m_buttons | buttons);
    } else if (release) {
        m_buttons = static_cast<uint8_t>(m_buttons & ~buttons);
    }
}

void FrameView::releaseMouse()
{
    const uint8_t bits[] = { 1, 2, 4 };
    for (int i = 0; i < 3; ++i) {
        if (!(m_buttons & bits[i])) {
            continue;
        }
        if (m_sender && m_input) {
            InputMsg msg;
            msg.x = m_lastX;
            msg.y = m_lastY;
            msg.action = InputMsg::ActionRelease;
            msg.buttons = bits[i];
            m_sender(msg);
        }
    }
    m_buttons = 0;
}

void FrameView::sendKey(uint16_t vk, bool down)
{
    if (!m_sender || !m_input || vk == 0) {
        return;
    }
    InputMsg msg;
    msg.vk = vk;
    msg.down = down ? 1 : 0;
    m_sender(msg);
    if (down) {
        m_down.insert(vk);
    } else {
        m_down.erase(vk);
    }
}

void FrameView::releaseKeys()
{
    const std::set<uint16_t> held = m_down;
    for (std::set<uint16_t>::const_iterator it = held.begin(); it != held.end(); ++it) {
        sendKey(*it, false);
    }
    m_down.clear();
}

void FrameView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    sendMouse(event->pos(), InputMsg::ActionPress, buttonBit(event->button()), 0);
}

void FrameView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_input) {
        sendMouse(event->pos(), InputMsg::ActionPress, buttonBit(event->button()), 0);
        return;
    }
    if (m_toggle) {
        m_toggle();
    }
}

void FrameView::mouseReleaseEvent(QMouseEvent* event)
{
    sendMouse(event->pos(), InputMsg::ActionRelease, buttonBit(event->button()), 0);
}

void FrameView::mouseMoveEvent(QMouseEvent* event)
{
    // 按钮位用被控端认为按着的那些，失焦补发过抬起后不再带
    sendMouse(event->pos(), InputMsg::ActionMove, m_buttons, 0);
}

void FrameView::wheelEvent(QWheelEvent* event)
{
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0) {
        return;
    }
    sendMouse(event->position().toPoint(), InputMsg::ActionMove, 0, static_cast<int16_t>(notches));
}

void FrameView::keyPressEvent(QKeyEvent* event)
{
    if (!m_input || event->isAutoRepeat()) {
        return;
    }
    sendKey(static_cast<uint16_t>(event->nativeVirtualKey()), true);
}

void FrameView::keyReleaseEvent(QKeyEvent* event)
{
    if (!m_input || event->isAutoRepeat()) {
        return;
    }
    sendKey(static_cast<uint16_t>(event->nativeVirtualKey()), false);
}

void FrameView::focusOutEvent(QFocusEvent* event)
{
    releaseMouse();
    releaseKeys();
    QWidget::focusOutEvent(event);
}

void FrameView::hideEvent(QHideEvent* event)
{
    releaseMouse();
    releaseKeys();
    QWidget::hideEvent(event);
}

bool FrameView::focusNextPrevChild(bool next)
{
    if (m_input) {
        return false;
    }
    return QWidget::focusNextPrevChild(next);
}

}
