/**
 * @file names.h
 * @brief 空 DACL，以及 Global / Local 命名空间
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 命名空间由第一块共享内存决定，互斥量和事件跟它走。
 * 普通用户创建不了 Global 段，此时整组对象退回 Local。
 */
#ifndef HS_IPC_NAMES_H
#define HS_IPC_NAMES_H

#include <stdint.h>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace HsIpc {

class Security {
public:
    Security();
    SECURITY_ATTRIBUTES* attr();

private:
    SECURITY_DESCRIPTOR m_sd;
    SECURITY_ATTRIBUTES m_sa;
};

bool validChannelName(const char* name);
std::wstring widen(const char* utf8);
std::wstring objectName(bool global, const std::wstring& shortName);

/**
 * @brief 一组同命名空间的映射、互斥量和事件
 * @note 先 openMapping。它试过 Global 之后，后面的对象不再换命名空间。
 */
class Kernel {
public:
    Kernel();
    ~Kernel();
    void close();

    /** @brief 命名空间已由共享内存定过。随后的事件不再自己试 Global。 */
    void useNamespace(bool global);

    bool openEvent(const std::wstring& shortName, std::string* err);
    bool openMutex(const std::wstring& shortName, std::string* err);
    bool openMapping(const std::wstring& shortName, uint64_t bytes, bool* created, std::string* err);

    bool lock(int timeoutMs);
    void unlock();
    void signal();
    int waitEvent(int timeoutMs);

    void* view() const { return m_view; }
    bool global() const { return m_global; }
    HANDLE mapping() const { return m_mapping; }
    unsigned long lastError() const { return m_lastError; }

private:
    HANDLE create(const std::wstring& shortName, bool eventWanted, uint64_t bytes, bool* created);

    Security m_security;
    HANDLE m_mapping;
    void* m_view;
    HANDLE m_mutex;
    HANDLE m_event;
    bool m_global;
    bool m_namespaceChosen;
    unsigned long m_lastError;
};

}

#endif
