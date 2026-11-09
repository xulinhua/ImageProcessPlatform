/**
 * @file ControlBar.h
 * @brief 被远程操作时贴在屏幕顶部正中的提示条：聊天、暂停操作、断开。按住左侧把手可左右拖
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_CONTROL_BAR_H
#define VISION_CONTROLLED_CONTROL_BAR_H

#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;

namespace Vision {

/**
 * @class ControlBar
 * @brief 被控提示条：告诉现场谁在远程操作，并提供收回操作权和断开
 * @note 无边框置顶、不抢焦点。只能水平拖动，拖过后再次显示保持原位置
 */
class ControlBar : public QWidget {
public:
    /** @brief 搭好界面，不显示 */
    explicit ControlBar(QWidget* parent = 0);

    /** @brief 更新文字与状态点；paused 为现场正在动鼠标，远程输入被暂时挡住 */
    void setState(const QString& controller, bool paused);
    /** @brief 点“暂停操作”时回调 */
    void setOnPause(const std::function<void()>& pause);
    /** @brief 点“断开”时回调 */
    void setOnDisconnect(const std::function<void()>& disconnect);
    /** @brief 点“聊天”时回调；空则按钮禁用 */
    void setOnChat(const std::function<void()>& chat);
    /** @brief 未显示时显示；首次显示放到主屏顶部正中 */
    void showAtTop();

protected:
    /** @brief 左键按下开始水平拖动 */
    void mousePressEvent(QMouseEvent* event) override;
    /** @brief 拖动中只改横坐标 */
    void mouseMoveEvent(QMouseEvent* event) override;
    /** @brief 结束拖动 */
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QLabel* m_text;                         ///< 状态文字：“xx 正在远程操作此电脑”或“已暂停”
    QLabel* m_live;                         ///< 录制圆点：操作中红色，现场暂停时黄色
    QPushButton* m_pause;                   ///< “暂停操作”按钮
    QPushButton* m_chat;                    ///< “聊天”按钮
    bool m_drag;                            ///< 是否正在拖动
    int m_dragX;                            ///< 按下时光标全局横坐标与窗口左边的差
    bool m_placed;                          ///< 是否已放过初始位置
    std::function<void()> m_pauseFn;        ///< 暂停操作回调，可空
    std::function<void()> m_disconnectFn;   ///< 断开回调，可空
    std::function<void()> m_chatFn;         ///< 聊天回调，可空
};

}

#endif
