/**
 * @file ConsoleHost.h
 * @brief --console。和托盘互斥，quit 只关掉这个控制台
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_APP_CONSOLEHOST_H
#define VISION_APP_CONSOLEHOST_H

#include <atomic>

/**
 * @class ConsoleHost
 * @brief 从 CONIN$ 读 status、start、stop、reload、quit
 */
class ConsoleHost {
public:
    /************************************************** 生命周期 **************************************************/
    /** @brief 尚未分配控制台 */
    ConsoleHost();
    /** @brief 结束读线程 */
    ~ConsoleHost();

    /************************************************** 窗口 **************************************************/
    /** @brief 分配控制台并开始读命令。已在读时只把窗口显示出来。失败返回 false */
    bool start();
    /** @brief 控制台窗口当前是否可见 */
    bool visible() const;
    /** @brief 没有控制台就创建，有则显示到前台 */
    void showWindow();
    /** @brief 藏起控制台窗口，读命令的线程保留 */
    void hideWindow();

    /************************************************** 命令 **************************************************/
    /**
     * @brief 单机时 start/stop 交给界面进程。不设置则写共享内存命令
     */
    void setLocalRun(void (*startFn)(void*), void (*stopFn)(void*), void* user);

private:
    /** @brief 读循环 */
    void loop();
    /** @brief 打印一行状态 */
    void printStatus();

    void* m_thread; ///< std::thread*，头文件不暴露线程库
    std::atomic<bool> m_run; ///< 读循环还在；读线程与界面线程都会读写
    void (*m_localStart)(void*); ///< 单机启动工站
    void (*m_localStop)(void*);  ///< 单机停止工站
    void* m_localUser;           ///< 回调用户数据
};

#endif
