/**
 * @file DeskView.cpp
 * @brief 远程自己的画面窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeskView.h"
#include "ControlSession.h"
#include "Messages.h"
#include "RemotePorts.h"
#include "RemoteTheme.h"

#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace Vision {

DeskView::DeskView(ControlSession* session, const QString& deviceId, const QString& code, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
    , m_deviceId(deviceId)
    , m_code(code)
    , m_picture(0)
    , m_status(0)
    , m_timer(0)
    , m_seen(0)
    , m_buttons(0)
    , m_asked(false)
    , m_imageW(0)
    , m_imageH(0)
{
    setWindowTitle(QString::fromUtf8("远程画面"));
    resize(1100, 700);
    setFocusPolicy(Qt::StrongFocus);
    QVBoxLayout* col = new QVBoxLayout(this);
    col->setContentsMargins(8, 8, 8, 8);
    col->setSpacing(6);
    m_status = new QLabel(QString::fromUtf8("正在连接 %1").arg(deviceId), this);
    m_status->setStyleSheet("color:#595959;");
    m_picture = new QLabel(this);
    m_picture->setAlignment(Qt::AlignCenter);
    m_picture->setStyleSheet("background:#1F1F1F;");
    m_picture->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_picture->setMouseTracking(true);
    col->addWidget(m_status);
    col->addWidget(m_picture, 1);
    m_timer = new QTimer(this);
    QObject::connect(m_timer, &QTimer::timeout, [this]() { tick(); });
    m_timer->start(80);
}

void DeskView::tick()
{
    if (!m_session) {
        return;
    }
    const std::vector<ControlSession::Tile> tiles = m_session->tiles();
    const ControlSession::Tile* tile = 0;
    for (size_t i = 0; i < tiles.size(); ++i) {
        QString id = QString::fromUtf8(tiles[i].deviceId.c_str());
        id.remove(' ');
        if (tiles[i].online && id == m_deviceId) {
            tile = &tiles[i];
            break;
        }
    }
    if (!tile) {
        m_status->setText(QString::fromUtf8("等待 %1 上线").arg(m_deviceId));
        return;
    }
    if (m_peerId != QString::fromUtf8(tile->peerId.c_str())) {
        m_peerId = QString::fromUtf8(tile->peerId.c_str());
        SubscribeMsg full;
        full.stream = 1;
        full.enable = 1;
        full.maxFps = 30;
        full.jpegQuality = 82;
        full.longEdge = static_cast<uint16_t>(RemoteConst::kNativeEdge);
        full.edgeLock = static_cast<uint8_t>(RemoteConst::kEdgeLockFixed);
        full.codec = 4;
        m_session->subscribe(tile->peerId, full);
        m_asked = false;
    }
    if (!m_asked && !tile->operating) {
        m_session->requestOperate(tile->peerId, m_code.isEmpty() ? AuthMethodTrusted : AuthMethodCode,
            m_code.toUtf8().constData());
        m_asked = true;
    }
    m_status->setText(QString::fromUtf8("%1 · %2")
        .arg(QString::fromUtf8(tile->name.c_str()))
        .arg(tile->operating ? QString::fromUtf8("操作中") : QString::fromUtf8(tile->hint.c_str())));
    if (!tile->pixels || tile->pixels->empty() || tile->pixelsSeq == m_seen) {
        return;
    }
    m_seen = tile->pixelsSeq;
    m_imageW = static_cast<int>(tile->pixelsW);
    m_imageH = static_cast<int>(tile->pixelsH);
    const QImage image = RemoteTheme::imageFromBgra(tile->pixels->data(), static_cast<int>(tile->pixels->size()),
        m_imageW, m_imageH);
    if (image.isNull()) {
        return;
    }
    m_picture->setPixmap(QPixmap::fromImage(image).scaled(m_picture->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

bool DeskView::mapPoint(const QPoint& point, uint16_t* x, uint16_t* y) const
{
    if (!x || !y || m_imageW < 2 || m_imageH < 2 || m_picture->width() < 2 || m_picture->height() < 2) {
        return false;
    }
    const QPixmap* pix = m_picture->pixmap();
    if (!pix || pix->isNull()) {
        return false;
    }
    const QSize shown = pix->size();
    const int left = (m_picture->width() - shown.width()) / 2;
    const int top = (m_picture->height() - shown.height()) / 2;
    const QPoint local = m_picture->mapFrom(this, point);
    const int px = local.x() - left;
    const int py = local.y() - top;
    if (px < 0 || py < 0 || px >= shown.width() || py >= shown.height()) {
        return false;
    }
    *x = static_cast<uint16_t>(px * m_imageW / shown.width());
    *y = static_cast<uint16_t>(py * m_imageH / shown.height());
    return true;
}

void DeskView::sendMouse(const QPoint& point, uint8_t action, uint8_t buttons, int16_t wheel)
{
    if (m_peerId.isEmpty() || !m_session) {
        return;
    }
    InputMsg msg;
    if (!mapPoint(point, &msg.x, &msg.y)) {
        return;
    }
    msg.action = action;
    msg.buttons = buttons;
    msg.down = action == InputMsg::ActionPress ? 1 : 0;
    msg.wheel = wheel;
    m_session->sendInput(m_peerId.toUtf8().constData(), msg);
    if (action == InputMsg::ActionPress) {
        m_buttons = static_cast<uint8_t>(m_buttons | buttons);
    } else if (action == InputMsg::ActionRelease) {
        m_buttons = static_cast<uint8_t>(m_buttons & ~buttons);
    }
}

void DeskView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    uint8_t bit = 1;
    if (event->button() == Qt::RightButton) {
        bit = 2;
    } else if (event->button() == Qt::MiddleButton) {
        bit = 4;
    }
    sendMouse(event->pos(), InputMsg::ActionPress, bit, 0);
}

void DeskView::mouseReleaseEvent(QMouseEvent* event)
{
    uint8_t bit = 1;
    if (event->button() == Qt::RightButton) {
        bit = 2;
    } else if (event->button() == Qt::MiddleButton) {
        bit = 4;
    }
    sendMouse(event->pos(), InputMsg::ActionRelease, bit, 0);
}

void DeskView::mouseMoveEvent(QMouseEvent* event)
{
    sendMouse(event->pos(), InputMsg::ActionMove, m_buttons, 0);
}

void DeskView::wheelEvent(QWheelEvent* event)
{
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0) {
        return;
    }
    sendMouse(event->pos(), InputMsg::ActionMove, 0, static_cast<int16_t>(notches));
}

void DeskView::sendKey(QKeyEvent* event, bool down)
{
    if (event->isAutoRepeat() || m_peerId.isEmpty() || !m_session) {
        return;
    }
    InputMsg msg;
    msg.action = down ? InputMsg::ActionPress : InputMsg::ActionRelease;
    msg.vk = static_cast<uint16_t>(event->nativeVirtualKey());
    msg.down = down ? 1 : 0;
    m_session->sendInput(m_peerId.toUtf8().constData(), msg);
}

void DeskView::keyPressEvent(QKeyEvent* event)
{
    sendKey(event, true);
}

void DeskView::keyReleaseEvent(QKeyEvent* event)
{
    sendKey(event, false);
}

}
