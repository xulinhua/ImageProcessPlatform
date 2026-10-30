/**
 * @file PluginHost.h
 * @brief PluginHost头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PLUGINHOST_H
#define VISION_PLUGINHOST_H
#include "VisionPluginApiExport.h"
#include "VisionVersion.h"

#include <string>
#include <vector>

namespace Vision {

class ToolBase;      // 工具插件基类（定义在 VisionToolBase）
class AlgToolBase; // 算法插件基类（定义在 VisionAlgToolBase）

/**
 * @class IPluginHost
 * @brief 插件侧看到的宿主接口：登记工具/算法、写加载期系统日志
 *
 * 由宿主实现并在 VisionPlugin_register 时传入。插件只依赖本抽象，
 * 不依赖 VisionCore / 注册表实现细节。
 */
class IPluginHost {
public:
    /** @brief 虚析构 */
    virtual ~IPluginHost() {}

    /**
     * @brief 登记一个工具原型（当前实现写入 ToolRegistry；Job 经 create/clone 取实例）
     * @param tool 插件 new 出的原型；空指针忽略；所有权交注册表
     */
    virtual void addTool(ToolBase* tool) = 0;

    /**
     * @brief 登记一个算法（当前实现写入 AlgToolBaseRegistry）
     * @param algor 插件 new 出的实例；空指针忽略；所有权交注册表
     */
    virtual void addAlg(AlgToolBase* algor) = 0;

    /**
     * @brief 写一条系统日志（加载/登记过程用）
     * @param message UTF-8 文本；可空
     */
    virtual void logSystem(const char* message) = 0;
};

/**
 * @class PluginHost
 * @brief 默认宿主实现：接入进程内工具/算法注册表与 VisionLog
 * @note 仅宿主进程使用；插件 DLL 不链接、不构造本类。
 */
class VISION_PLUGINAPI_API PluginHost : public IPluginHost {
public:
    /**
     * @brief 将工具交给 ToolRegistry
     * @param tool 可空
     */
    void addTool(ToolBase* tool);

    /**
     * @brief 将算法交给 AlgToolBaseRegistry（按 toolName+algLib）
     * @param algor 可空
     */
    void addAlg(AlgToolBase* algor);

    /**
     * @brief 写入系统日志分类 plugin_load
     * @param message UTF-8；空则记空串
     */
    void logSystem(const char* message);
};

/**
 * @class ToolLibItem
 * @brief 已经载入进程的一只插件 DLL
 */
struct VISION_PLUGINAPI_API ToolLibItem {
    std::string fileName; ///< 文件名
    std::string kind;     ///< tool 或 algo
    std::string stamp;    ///< 文件时间
};

/**
 * @class ToolLibReg
 * @brief 某只 DLL 登记的一条工具或算法
 */
struct VISION_PLUGINAPI_API ToolLibReg {
    std::string fileName;    ///< 来源 DLL
    std::string kind;        ///< tool 或 algo
    std::string displayName; ///< 显示名
    std::string toolName;    ///< 注册名
    std::string algLib;      ///< 算法库显示名，工具为空
    std::string version;     ///< 版本，算法可空
    std::vector<VerNote> notes; ///< 变更表。界面按方案号到当前号截取
    std::string description; ///< 说明
    std::string stamp;       ///< DLL 文件时间
};

/**
 * @class ToolLibMgr
 * @brief 工具库管理。进程内单例，负责扫描和热加载插件
 */
class VISION_PLUGINAPI_API ToolLibMgr : public IPluginHost {
public:
    /** @brief 进程内唯一实例 */
    static ToolLibMgr& instance();

    /**
     * @brief 扫描插件目录。已经加载过的文件跳过，新 DLL 立即登记
     * @return 这次新加载的个数
     */
    int loadAll();
    /** @brief 已载入的插件 */
    std::vector<ToolLibItem> loaded() const;
    /** @brief 已登记项，带上来源 DLL */
    std::vector<ToolLibReg> registrations() const;

    /** @brief 记下来源 DLL 的登记项，再交 PluginHost 写入 ToolRegistry */
    void addTool(ToolBase* tool);
    /** @brief 记下来源 DLL 的登记项，再交 PluginHost 写入 AlgToolBaseRegistry */
    void addAlg(AlgToolBase* algor);
    /** @brief 转交 PluginHost 写系统日志 */
    void logSystem(const char* message);

private:
    /** @brief 单例私有构造 */
    ToolLibMgr();
    /** @brief 禁止拷贝（只声明不实现） */
    ToolLibMgr(const ToolLibMgr&);
    /** @brief 禁止赋值（只声明不实现） */
    ToolLibMgr& operator=(const ToolLibMgr&);

    struct Impl; ///< 私有实现
    Impl* m_impl; ///< 已加载文件表
};

/**
 * @brief 扫描并加载工具、算法插件目录
 * @param host 宿主实现；空则直接返回 0
 * @return 成功完成 register 的 DLL 个数（tools + algos 合计）
 *
 * 目录来自 AppPaths：plugins\\tools、plugins\\algos。
 * 每个 DLL 须导出 VisionPlugin_apiVersion（返回 1）与 VisionPlugin_register。
 * 加载后不 FreeLibrary，插件在进程生命周期内保持驻留。
 */
VISION_PLUGINAPI_API int loadVisionPlugins(IPluginHost* host);

}

#endif
