/**
 * @file region.cpp
 * @brief 调用方自己排版的共享内存，以及配套的命名事件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 三个内部进程的画面槽仍由 VisionIpc 排版。这里只提供映射和事件，
 * 对象名不加 HsIpc. 前缀，这样仍是 Global\Vision.Ipc。
 */
#include "hs_ipc.h"
#include "names.h"

namespace {

void writeErr(char* err, int errBytes, const char* text)
{
    if (!err || errBytes <= 0) {
        return;
    }
    if (!text) {
        err[0] = 0;
        return;
    }
    int i = 0;
    for (; i < errBytes - 1 && text[i]; ++i) {
        err[i] = text[i];
    }
    err[i] = 0;
}

}

struct HsIpcRegion {
    HsIpc::Kernel kernel;
    uint32_t bytes;
};

struct HsIpcNotify {
    HsIpc::Kernel kernel;
};

HsIpcRegion* hs_ipc_region_open(const char* name, uint32_t bytes,
    int* created, int* isGlobal, char* err, int errBytes)
{
    if (created) {
        *created = 0;
    }
    if (isGlobal) {
        *isGlobal = 0;
    }
    if (!HsIpc::validChannelName(name) || bytes == 0) {
        writeErr(err, errBytes, "共享内存参数无效");
        return 0;
    }
    HsIpcRegion* region = new HsIpcRegion();
    region->bytes = 0;
    bool made = false;
    std::string reason;
    if (!region->kernel.openMapping(HsIpc::widen(name), bytes, &made, &reason)) {
        writeErr(err, errBytes, reason.empty() ? "创建共享内存失败" : reason.c_str());
        delete region;
        return 0;
    }
    region->bytes = bytes;
    if (created) {
        *created = made ? 1 : 0;
    }
    if (isGlobal) {
        *isGlobal = region->kernel.global() ? 1 : 0;
    }
    return region;
}

void hs_ipc_region_close(HsIpcRegion* region)
{
    delete region;
}

void* hs_ipc_region_data(HsIpcRegion* region)
{
    return region ? region->kernel.view() : 0;
}

uint32_t hs_ipc_region_size(HsIpcRegion* region)
{
    return region ? region->bytes : 0;
}

HsIpcNotify* hs_ipc_notify_open(int isGlobal, const char* name, char* err, int errBytes)
{
    if (!HsIpc::validChannelName(name)) {
        writeErr(err, errBytes, "事件名无效");
        return 0;
    }
    HsIpcNotify* notify = new HsIpcNotify();
    notify->kernel.useNamespace(isGlobal != 0);
    std::string reason;
    if (!notify->kernel.openEvent(HsIpc::widen(name), &reason)) {
        writeErr(err, errBytes, reason.empty() ? "创建命名事件失败" : reason.c_str());
        delete notify;
        return 0;
    }
    return notify;
}

void hs_ipc_notify_close(HsIpcNotify* notify)
{
    delete notify;
}

void hs_ipc_notify_signal(HsIpcNotify* notify)
{
    if (notify) {
        notify->kernel.signal();
    }
}

int hs_ipc_notify_wait(HsIpcNotify* notify, int timeoutMs)
{
    if (!notify) {
        return HS_IPC_ERR_IO;
    }
    return notify->kernel.waitEvent(timeoutMs);
}
