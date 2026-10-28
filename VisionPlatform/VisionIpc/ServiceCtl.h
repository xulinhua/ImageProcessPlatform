/**
 * @file ServiceCtl.h
 * @brief 启动和停止本机的 Station、Link 服务
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SERVICE_CTL_H
#define VISION_SERVICE_CTL_H
#include "VisionIpcExport.h"

#include <string>

namespace Vision {

/**
 * @class ServiceCtl
 * @brief 通过服务控制管理器启停。没注册时改为拉起同目录下的 exe
 */
class VISION_IPC_API ServiceCtl {
public:
    /** @brief 服务是否在跑。没注册时看同目录进程是否已创建过共享内存心跳由调用方判断 */
    static bool isRunning(const std::wstring& serviceName);
    /**
     * @brief 启动服务。没安装时 CreateProcess 同目录 exe
     * @param exeFile 例如 Vision.Station.exe
     */
    static bool start(const std::wstring& serviceName, const std::wstring& exeFile, std::wstring* err);
    /** @brief 停止已安装的服务。没安装则按进程名结束 */
    static bool stop(const std::wstring& serviceName, const std::wstring& exeFile, std::wstring* err);
    /** @brief 注册自动启动的服务。exe 取本进程目录下的文件名 */
    static bool install(const std::wstring& serviceName, const std::wstring& exeFile, std::wstring* err);
    /** @brief 卸掉服务 */
    static bool uninstall(const std::wstring& serviceName, std::wstring* err);
};

}

#endif
