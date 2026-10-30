/**
 * @file NamedEvent.h
 * @brief 命名自动复位事件。进程之间用它叫醒对方
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef IPC_NAMED_EVENT_H
#define IPC_NAMED_EVENT_H
#include "IpcExport.h"

namespace Ipc {

/**
 * @class NamedEvent
 * @brief 创建或打开一个命名事件
 * @note global 要和同一块共享内存一致，否则服务和界面不在同一个对象上
 */
class IPC_API NamedEvent {
public:
    /************************************************** 生命周期 **************************************************/
    /** @brief 尚未打开 */
    NamedEvent();
    /** @brief 关闭句柄 */
    ~NamedEvent();

    /************************************************** 句柄 **************************************************/
    /**
     * @brief 按短名创建或打开自动复位事件
     * @param name 不含 Global\\ 或 Local\\
     */
    bool open(bool global, const wchar_t* name);
    /** @brief 关闭句柄 */
    void close();
    /** @brief 已经打开 */
    bool opened() const;

    /************************************************** 通知 **************************************************/
    /** @brief 唤醒一个等待者 */
    void signal();
    /** @brief 等到被唤醒或超时。timeoutMs 小于 0 时一直等 */
    bool wait(int timeoutMs);

private:
    /** @brief 禁止拷贝，句柄只归一个对象 */
    NamedEvent(const NamedEvent&);
    /** @brief 禁止赋值 */
    NamedEvent& operator=(const NamedEvent&);

    void* m_handle;   ///< 事件 HANDLE，未打开为空
};

}

#endif
