/**
 * @file ToolRegistry.h
 * @brief ToolRegistry头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLREGISTRY_H
#define VISION_TOOLREGISTRY_H
#include "ToolExport.h"
#include "ToolEnums.h"
#include <map>
#include <string>
#include <vector>

namespace Vision {

class ToolBase;

/**
 * @class ToolRegistry
 * @brief 进程内工具原型表。单例在本 DLL 内；create 经 clone 出实例
 */
class VISION_TOOL_API ToolRegistry {
public:
    /************************************************** 单例 **************************************************/
    /** @brief 进程内单例 */
    static ToolRegistry& instance();
    /** @brief 释放全部已登记原型 */
    ~ToolRegistry();

    /************************************************** 登记 **************************************************/
    /** @brief 按注册名去重后接管原型；重复则释放传入对象 */
    void add(ToolBase* tool);
    /**
     * @brief 登记即将加载的工具 DLL 修改时间；空串表示清除
     * @param stamp 本地时间字符串，如 2026-09-21 17:04:20
     */
    void setIncomingPluginStamp(const std::string& stamp);

    /************************************************** 查询 **************************************************/
    /** @brief 已登记工具的 DLL 修改时间；没有则为空 */
    std::string pluginStamp(const std::string& toolName) const;
    /** @brief 按注册名精确查找原型；没有则为 0 */
    ToolBase* find(const std::string& toolName) const;
    /** @brief 按注册名查找（find 的别名） */
    ToolBase* findByName(const std::string& toolName) const;
    /** @brief 返回该大类的第一个原型；没有则为 0 */
    ToolBase* findByType(ToolType type) const;
    /** @brief 全部已登记原型 */
    std::vector<ToolBase*> all() const;

    /************************************************** 实例 **************************************************/
    /**
     * @brief 按注册名 clone 出新实例；未登记返回 0
     * @note 调用方负责 destroy
     */
    ToolBase* create(const std::string& toolName) const;

private:
    ToolRegistry() {}
    std::vector<ToolBase*> m_tools; ///< 已登记的原型，本类拥有
    std::string m_incomingStamp;    ///< 当前正在加载的工具 DLL 时间戳
    std::map<std::string, std::string> m_stamps; ///< toolName 到修改时间
};

}

#endif

