/**
 * @file DeskView.h
 * @brief 远程自己的画面窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DESK_VIEW_H
#define VISION_DESK_VIEW_H

#include <QWidget>
#include <stdint.h>

class QLabel;
class QTimer;

namespace Vision {

class ControlSession;

/**
 * @class DeskView
 * @brief 看一台远端电脑的画面，键鼠从这里发出
 */
class DeskView : public QWidget {
public:
    /**
     * @brief 绑定已经在听的会话
     * @param deviceId 对方设备号，空格已去掉
     * @param code 验证码，可空
     */
    DeskView(ControlSession* session, const QString& deviceId, const QString& code, QWidget* parent = 0);

protected:
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void wheelEvent(QWheelEvent* event);
    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);

private:
    void tick();
    void sendMouse(const QPoint& point, uint8_t action, uint8_t buttons, int16_t wheel);
    bool mapPoint(const QPoint& point, uint16_t* x, uint16_t* y) const;
    void sendKey(QKeyEvent* event, bool down);

    ControlSession* m_session;
    QString m_deviceId;
    QString m_code;
    QString m_peerId;
    QLabel* m_picture;
    QLabel* m_status;
    QTimer* m_timer;
    uint64_t m_seen;
    uint8_t m_buttons;
    bool m_asked;
    int m_imageW;
    int m_imageH;
};

}

#endif
