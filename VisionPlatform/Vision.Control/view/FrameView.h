/**
 * @file FrameView.h
 * @brief 显示最新 JPEG，把鼠标和按键换成图像坐标。操作时画蓝框，顶部可显示提示
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_FRAME_VIEW_H
#define VISION_CONTROL_FRAME_VIEW_H

#include "H264Codec.h"
#include "Messages.h"
#include <QImage>
#include <QString>
#include <QWidget>
#include <functional>
#include <set>

namespace Vision {

/**
 * @class FrameView
 * @brief 会话窗口的画面区：居中绘制画面，输入开启时把键鼠转成 InputMsg 交给发送回调
 * @note 输入开启时 Tab 不切焦点；失焦、隐藏或关掉输入时补发所有未松开的键和鼠标按钮
 */
class FrameView : public QWidget {
public:
    /** @brief 强焦点、跟踪鼠标，最小 480x270 */
    explicit FrameView(QWidget* parent = 0);

    /************************************************** 画面 **************************************************/
    /**
     * @brief 解码并显示一帧
     * @param jpeg 为空时清掉画面
     * @param imageW 画面原始宽，用于坐标换算
     * @param imageH 画面原始高
     */
    void setImage(const CommBytes& jpeg, uint32_t imageW, uint32_t imageH, uint8_t codec = 1);
    /** @brief 被控端光标在图像坐标中的位置。visible 为 false 时不画 */
    void setRemoteCursor(int x, int y, bool visible);
    /** @brief 光标 PNG。空则继续画箭头 */
    void setCursorShape(const CommBytes& png, int hotX, int hotY);
    /** @brief 当前解码后的画面，可能为空图 */
    const QImage& image() const;
    /** @brief 输入消息的发送回调 */
    void setSender(const std::function<void(const InputMsg&)>& sender);

    /** @brief 关掉输入时补发所有没松开的键和鼠标按钮 */
    void setInputEnabled(bool enabled);
    /** @brief true 按控件大小等比缩放，false 原始 1:1 居中 */
    void setFit(bool fit);
    /** @brief 是否持有操作权，持有时画面外画蓝框 */
    void setOperating(bool operating);

    /** @brief tone 为 blue 或 orange，text 为空时不显示 */
    void setNotice(const QString& text, const char* tone);

    /** @brief 只看时双击回调 */
    void setToggle(const std::function<void()>& toggle);

    /**
     * @brief 工具条上的组合键
     * @param vk Windows 虚拟键码，0 忽略
     * @param down true 按下，false 抬起
     */
    void sendKey(uint16_t vk, bool down);

protected:
    /************************************************** 事件 **************************************************/
    /** @brief 画背景、画面（或「等待画面」）、操作蓝框和顶部提示 */
    void paintEvent(QPaintEvent* event) override;
    /** @brief 取焦点并发送按下 */
    void mousePressEvent(QMouseEvent* event) override;
    /** @brief 发送抬起 */
    void mouseReleaseEvent(QMouseEvent* event) override;
    /** @brief 发送移动，带当前按住的按钮位 */
    void mouseMoveEvent(QMouseEvent* event) override;
    /** @brief 按 120 一格换成滚轮格数发送，不足一格丢弃 */
    void wheelEvent(QWheelEvent* event) override;
    /** @brief 输入开启时当作按下转发，否则触发双击回调 */
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    /** @brief 转发按下，忽略自动重复 */
    void keyPressEvent(QKeyEvent* event) override;
    /** @brief 转发抬起，忽略自动重复 */
    void keyReleaseEvent(QKeyEvent* event) override;
    /** @brief 失焦时补发所有未松开的键和鼠标按钮 */
    void focusOutEvent(QFocusEvent* event) override;
    /** @brief 隐藏（含所在窗口关闭、最小化）时补发所有未松开的键和鼠标按钮 */
    void hideEvent(QHideEvent* event) override;
    /** @brief 输入开启时拦下 Tab 切焦点，让 Tab 转发到现场 */
    bool focusNextPrevChild(bool next) override;

private:
    /************************************************** 内部 **************************************************/
    /** @brief 画面在控件里的绘制区域，四周留 16 像素；无画面尺寸时返回无效矩形 */
    QRect imageRect() const;
    /**
     * @brief 控件坐标换成画面像素坐标
     * @param clamp true 时画面外的点钳到画面边界
     * @return 没有画面尺寸，或 clamp 为 false 且落在画面外时返回 false
     */
    bool mapPoint(const QPoint& point, bool clamp, uint16_t* x, uint16_t* y) const;
    /**
     * @brief 发一条鼠标消息并维护 m_buttons；down 字段按 action 填，按下为 1
     * @param action 取 InputMsg::Action 的值（不发 ActionLegacy）
     * @param buttons 按钮位：1 左，2 右，4 中；移动时为当前按住的全部按钮，按下/抬起时为该按钮
     * @param wheel 滚轮格数，向上为正
     * @note 输入关闭时不发。按下、滚轮和无按钮的移动在画面外不发；有按钮按着时的移动和抬起钳到画面边界照发，
     *       抬起只发给已发过按下的按钮，没有画面尺寸时用最后位置
     */
    void sendMouse(const QPoint& point, uint8_t action, uint8_t buttons, int16_t wheel);
    /** @brief 对 m_down 里的键逐个发抬起 */
    void releaseKeys();
    /** @brief 对 m_buttons 里的按钮逐个在最后位置发抬起，然后清空 */
    void releaseMouse();
    /** @brief Qt 按钮换成协议按钮位，未知按钮按左键 */
    static uint8_t buttonBit(Qt::MouseButton button);

    QImage m_image;                                 ///< 当前解码后的画面
    uint32_t m_imageW;                              ///< 画面原始宽（像素），0 表示无画面
    uint32_t m_imageH;                              ///< 画面原始高（像素）
    int m_cursorX;                                  ///< 远程光标图像坐标
    int m_cursorY;                                  ///< 远程光标图像坐标
    bool m_cursorOn;                                ///< 有光标位置可画
    QImage m_cursorImage;                           ///< 远程光标形状，空则画箭头
    int m_cursorHotX;                               ///< 光标热点，相对图标左上角
    int m_cursorHotY;                               ///< 光标热点
    H264Codec m_h264;                               ///< 主码流解码，跨帧保留参考帧
    bool m_input;                                   ///< 是否转发键鼠
    bool m_fit;                                     ///< 等比缩放（true）或 1:1（false）
    bool m_operating;                               ///< 持有操作权，画蓝框
    QString m_notice;                               ///< 顶部提示文字，空不显示
    QColor m_noticeColor;                           ///< 提示底色：蓝或橙
    std::set<uint16_t> m_down;                      ///< 已发按下、尚未发抬起的虚拟键
    uint8_t m_buttons;                              ///< 已发按下、尚未发抬起的鼠标按钮位（1 左，2 右，4 中）
    uint16_t m_lastX;                               ///< 最后一次发出的鼠标横坐标（画面像素），补发抬起时用
    uint16_t m_lastY;                               ///< 最后一次发出的鼠标纵坐标（画面像素）
    std::function<void(const InputMsg&)> m_sender;  ///< 输入发送回调，可空
    std::function<void()> m_toggle;                 ///< 只看时双击回调（切全屏），可空
};

}

#endif
