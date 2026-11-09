/**
 * @file ConfirmDialog.h
 * @brief 屏幕右下角的操作申请确认框，到时自动拒绝
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_CONFIRM_DIALOG_H
#define VISION_CONTROLLED_CONFIRM_DIALOG_H

#include <QWidget>
#include <cstdint>
#include <functional>

class QCheckBox;
class QLabel;
class QProgressBar;
class QTimer;

namespace Vision {

/**
 * @class ConfirmDialog
 * @brief 现场来访确认：显示谁在申请、倒计时进度条，现场点“允许”或“拒绝”
 * @note 无边框置顶工具窗。倒计时到了只隐藏不回调，超时答复由 AgentSession 自己发
 */
class ConfirmDialog : public QWidget {
public:
    /** @brief 搭好界面，不显示 */
    explicit ConfirmDialog(QWidget* parent = 0);

    /**
     * @brief 显示一个申请：填控制端名与地址，放到主屏右下角并开始倒计时
     * @param seq 申请序号，答复时原样带回
     * @param controller 控制端显示名，空时显示“未具名控制端”
     * @param host 控制端地址
     * @param secondsLeft 剩余秒数，决定截止时刻
     * @param totalSeconds 总秒数，用于进度条比例，<=0 按 30
     */
    void showRequest(uint64_t seq, const QString& controller, const QString& host, int secondsLeft, int totalSeconds,
        uint32_t requestPerms = 0);
    /** @brief 当前显示的申请序号 */
    uint64_t seq() const;

    /** @brief allow 为允许，trust 为以后信任；perms 为勾选的权限位（含观看） */
    void setOnAnswer(const std::function<void(uint64_t, bool, bool, uint32_t)>& answer);

private:
    /** @brief 每 100 ms 刷新进度条与剩余秒数，到时隐藏 */
    void tick();
    /** @brief 停止倒计时、隐藏并回调；只有允许时才带上“以后信任” */
    void answer(bool allow);

    QLabel* m_who;              ///< 申请方说明：控制端名、地址、认证状态
    QLabel* m_left;             ///< “N 秒后自动拒绝”
    QProgressBar* m_progress;   ///< 剩余时间进度条，范围 0 ~ 1000
    QCheckBox* m_trust;         ///< “以后信任这个控制端”，每次显示时清掉
    QCheckBox* m_perms[6];      ///< 观看/操作/剪贴板/文件/电源/隐私屏
    QTimer* m_timer;            ///< 倒计时刷新定时器，100 ms
    uint64_t m_seq;             ///< 当前申请序号
    qint64 m_deadline;          ///< 截止时刻，墙钟 ms（QDateTime::currentMSecsSinceEpoch）
    int m_total;                ///< 申请总时长（秒），用于进度条比例
    std::function<void(uint64_t, bool, bool, uint32_t)> m_answer; ///< 答复回调，可空
    /** @brief 勾选框对应的权限位 */
    uint32_t grantedPerms() const;
};

}

#endif
