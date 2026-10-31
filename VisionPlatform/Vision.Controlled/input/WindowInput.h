/**
 * @file WindowInput.h
 * @brief 把图像坐标送进目标窗口。本地鼠标活动时暂停
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_WINDOW_INPUT_H
#define VISION_CONTROLLED_WINDOW_INPUT_H

#include "Messages.h"
#include <vector>
#include <windows.h>

namespace Vision {

/**
 * @class WindowInput
 * @brief 远程键鼠注入：进程画面用 PostMessage 投到目标窗口，整个桌面用 SendInput；现场在用鼠标时暂停约 1 秒
 * @note allowRemote / allowRemoteRect / post / postScreen / releaseAll 改内部状态且不加锁，
 *       只能在同一线程（被控端收包线程，或该线程退出之后）调用
 */
class WindowInput {
public:
    /** @brief 尚未记录本地光标位置，不暂停 */
    WindowInput();

    /** @brief 0=PostMessage 到客户区，1=SetForegroundWindow 后用 SendInput（仍映射图像坐标） */
    void setMode(int mode);
    /** @brief 当前注入方式，见 setMode */
    int mode() const;

    /** @brief 目标完整性更高，或打不开进程时，返回 false */
    bool operable(HWND hwnd) const;
    /** @brief 前台是目标窗口且本地鼠标在动时，约 1 秒内返回 false */
    bool allowRemote(HWND hwnd);
    /** @brief 本地鼠标在这块屏幕里移动或按着键时，约 1 秒内返回 false */
    bool allowRemoteRect(const RECT& rect);
    /**
     * @brief 进程画面：把图像坐标换成客户区坐标，PostMessage 投递键盘、滚轮或鼠标消息
     * @param clientW 客户区实际宽（像素），与 imageW 一起做比例换算
     * @param imageW 控制端看到的画面宽（像素）
     * @note 按下/抬起取 buttons 的最低置位按钮；移动发 WM_MOUSEMOVE，wParam 带 buttons 对应的 MK_ 位。按下与抬起记入 m_held
     */
    void post(HWND hwnd, const InputMsg& msg, uint32_t clientW, uint32_t clientH, uint32_t imageW, uint32_t imageH);
    /**
     * @brief 整个桌面：换成虚拟屏幕绝对坐标后用 SendInput 注入；键盘事件发给当前前台窗口
     * @param originX 该显示器左上角在虚拟屏幕上的横坐标
     * @param originY 该显示器左上角在虚拟屏幕上的纵坐标
     * @note 移动只发 MOUSEEVENTF_MOVE，不发按钮抬起。按下与抬起记入 m_held
     */
    void postScreen(int originX, int originY, const InputMsg& msg, uint32_t clientW, uint32_t clientH, uint32_t imageW, uint32_t imageH);
    /** @brief 松开全部已注入按下、未注入抬起的键和鼠标按钮：窗口消息注入的发 UP 消息到当时的目标窗口，SendInput 注入的发 UP 事件 */
    void releaseAll();

private:
    /** @brief 一个已注入按下、尚未注入抬起的键或鼠标按钮 */
    struct Held {
        HWND hwnd;          ///< 窗口消息注入时的目标窗口；SendInput 注入为 0
        uint16_t vk;        ///< 虚拟键码；0 表示鼠标按钮
        uint8_t button;     ///< 鼠标按钮位 1 左、2 右、4 中；键盘为 0
        LPARAM pos;         ///< 窗口消息注入的鼠标按钮在该窗口客户区的最新坐标（MAKELPARAM），补发抬起时用；其他为 0
    };

    /** @brief 令牌的完整性级别 RID（如 0x2000 中、0x3000 高），取不到返回 0 */
    int integrityRid(HANDLE token) const;
    /** @brief 鼠标事件是按下（1）、抬起（-1）还是移动（0）；没有按钮位恒为移动，旧版报文按 down 推断 */
    static int edgeOf(const InputMsg& msg);
    /** @brief 按左、右、中的顺序取第一个置位的按钮位，没有为 0 */
    static uint8_t lowestButton(uint8_t buttons);
    /**
     * @brief 登记一次注入：按下时加入（已有则更新坐标），抬起时按 hwnd + vk + button 移除
     * @param hwnd 窗口消息注入的目标窗口，SendInput 注入传 0
     * @param pos 窗口消息注入鼠标按钮时的客户区坐标，其他传 0
     */
    void markHeld(HWND hwnd, uint16_t vk, uint8_t button, LPARAM pos, bool down);

    POINT m_last;                       ///< 上一次查到的本地光标屏幕坐标，用来判断是否移动
    bool m_haveLast;                    ///< m_last 是否已有值
    unsigned long long m_pauseUntil;    ///< 远程输入暂停到此时刻（GetTickCount64 ms）
    int m_mode;                         ///< 0 PostMessage，1 前台 SendInput
    std::vector<Held> m_held;           ///< 已注入按下、尚未注入抬起的键和鼠标按钮，releaseAll 后清空
};

}

#endif
