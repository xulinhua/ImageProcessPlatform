/**
 * @file IpcSession.h
 * @brief 跨会话内核对象的安全描述和名字
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 只给 IpcBase 内部用。空 DACL 让服务（会话 0）和已登录用户打开同一对象。
 */

#ifndef IPC_SESSION_H
#define IPC_SESSION_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

namespace Ipc {

/**
 * @class IpcSession
 * @brief 准备空 DACL，并把短名拼成 Global 或 Local 对象名
 */
class IpcSession {
public:
    /** @brief 初始化空 DACL */
    IpcSession();
    /** @brief 传给 CreateFileMapping / CreateEvent */
    SECURITY_ATTRIBUTES* attributes();
    /** @brief name 不含前缀。global 为真时得到 Global\\name */
    std::wstring objectName(bool global, const wchar_t* name) const;

private:
    SECURITY_DESCRIPTOR m_sd;   ///< 带空 DACL 的安全描述，m_sa 指向它
    SECURITY_ATTRIBUTES m_sa;   ///< 创建内核对象时传入，不可继承
};

}

#endif
