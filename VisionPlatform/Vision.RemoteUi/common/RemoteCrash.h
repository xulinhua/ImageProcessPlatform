/**
 * @file RemoteCrash.h
 * @brief 进程级防护：检查点日志、非致命报警、C++/SEH 守护。异常可弹窗，尽量不崩进程
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_CRASH_H
#define VISION_REMOTE_CRASH_H

#include <functional>

namespace Vision {

/**
 * @class RemoteCrash
 * @brief 安装异常过滤器与 CRT 钩子；run / protect 包住关键路径，失败只报警
 */
class RemoteCrash {
public:
    /**
     * @brief 尽早调用（QApplication 之前）。role 写入日志文件名，如 control / controlled
     */
    static void install(const char* role);

    /** @brief 记录检查点（UTF-8），崩溃弹窗会带上最近若干条 */
    static void checkpoint(const char* where);

    /** @brief 追加一行诊断日志（UTF-8），不弹窗 */
    static void log(const char* message);

    /**
     * @brief 非致命故障：写日志；popup 为真时 MessageBox 报警，不退出进程
     * @param where 位置标记（UTF-8）
     * @param message 说明（UTF-8）
     * @param popup 是否弹窗，默认 true
     */
    static void alert(const char* where, const char* message, bool popup = true);

    /**
     * @brief 用 C++ try/catch 执行 fn；捕获后 alert，返回 false。不拦截 SEH/访问冲突
     */
    static bool run(const char* where, const std::function<void()>& fn);

    /**
     * @brief 用 SEH __try/__except 执行 fn(arg)；捕获访问冲突等后 alert，返回 false。
     *        fn 内不可有需要展开的 C++ 对象析构跨越 __try（用静态/全局或纯 C 风格）
     */
    static bool protect(const char* where, void (*fn)(void*), void* arg);

    /** @brief 日志文件完整路径（UTF-8），安装前可能为空 */
    static const char* logPath();
};

}

#endif
