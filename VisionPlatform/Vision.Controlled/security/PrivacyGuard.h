/**
 * @file PrivacyGuard.h
 * @brief 隐私屏期间丢掉本机键鼠，保留远程注入的输入
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_PRIVACY_GUARD_H
#define VISION_CONTROLLED_PRIVACY_GUARD_H

#include <windows.h>

namespace Vision {

/**
 * @class PrivacyGuard
 * @brief 低级键盘、鼠标钩子。没有 LLKHF_INJECTED / LLMHF_INJECTED 的事件直接吃掉
 * @note 必须在有消息循环的界面线程上开关。析构或关掉时卸钩，进程退出钩子随之释放
 */
class PrivacyGuard {
public:
    /** @brief 未安装钩子 */
    PrivacyGuard();
    /** @brief 卸掉钩子 */
    ~PrivacyGuard();

    /** @brief 打开或关掉拦截。重复设置同一状态不重新装钩 */
    void setEnabled(bool on);

private:
    /** @brief 键盘钩子，转给当前实例 */
    static LRESULT CALLBACK onKey(int code, WPARAM wp, LPARAM lp);
    /** @brief 鼠标钩子，转给当前实例 */
    static LRESULT CALLBACK onMouse(int code, WPARAM wp, LPARAM lp);
    /** @brief 非注入事件返回 1，注入事件交给下一个钩子 */
    LRESULT filter(int code, WPARAM wp, LPARAM lp, bool keyboard);

    static PrivacyGuard* s_guard;   ///< 钩子过程里找到当前实例
    HHOOK m_key;                    ///< 键盘低级钩子，未安装为空
    HHOOK m_mouse;                  ///< 鼠标低级钩子
    bool m_on;                      ///< 钩子已安装
};

}

#endif
