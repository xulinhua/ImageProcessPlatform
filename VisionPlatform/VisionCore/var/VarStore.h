/**
 * @file VarStore.h
 * @brief 方案级全局变量表
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_VARSTORE_H
#define VISION_VARSTORE_H
#include "VisionCoreExport.h"

#include "GlobalVar.h"
#include "ToolGeom.h"
#include <mutex>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class VarStore
 * @brief 全局变量表：增删查改、通讯串解析、工具参数绑定读写
 */
class VISIONCORE_API VarStore {
public:
    /** @brief 空表 */
    VarStore();
    /** @brief 拷贝变量表（新建互斥锁） */
    VarStore(const VarStore& other);
    /** @brief 移动变量表 */
    VarStore(VarStore&& other) noexcept;
    /** @brief 拷贝赋值 */
    VarStore& operator=(const VarStore& other);
    /** @brief 移动赋值 */
    VarStore& operator=(VarStore&& other) noexcept;
    /** @brief 析构 */
    ~VarStore();

    /** @brief 变量条数 */
    int size() const;
    /** @brief 按下标取（越界返回空名条目的拷贝） */
    GlobalVar at(int index) const;
    /** @brief 全部快照（UI 用） */
    std::vector<GlobalVar> items() const;
    /** @brief 整表替换（对话框 OK 写回） */
    void setItems(const std::vector<GlobalVar>& items);
    /** @brief 清空 */
    void clear();

    /** @brief 按名查找下标；找不到返回 -1 */
    int findIndex(const std::string& name) const;
    /** @brief 是否存在 */
    bool contains(const std::string& name) const;
    /**
     * @brief 添加；名称空或重名返回 false
     */
    bool add(const GlobalVar& v);
    /** @brief 按名删除；不存在返回 false */
    bool remove(const std::string& name);
    /** @brief 按下标删除 */
    bool removeAt(int index);

    /** @brief 读值；不存在返回空 ParamValue */
    ParamValue get(const std::string& name) const;
    /** @brief 读字符串形式 */
    std::string getString(const std::string& name, const std::string& def = std::string()) const;
    /**
     * @brief 写值（保持已有类型做转换）；不存在返回 false
     */
    bool set(const std::string& name, const ParamValue& value);
    /**
     * @brief 按变量类型把文本解析后写入
     */
    bool setFromString(const std::string& name, const std::string& text);

    /**
     * @brief 解析通讯行：SetGlobalValue:name=value（容忍空格与大小写前缀）
     * @return 成功写入返回 true；非本协议或失败返回 false
     */
    bool applyCommLine(const std::string& line);

    /**
     * @brief 订阅绑定：把 global:Name 拷进 run[paramKey]（direction==0）
     */
    void applySubscribeBinds(const std::vector<ParamBind>& binds, ParamMap* run) const;
    /**
     * @brief 回写绑定：工具成功后把 run/output 中的键写回全局（direction==1，path 为 global:）
     */
    void applyWriteBinds(const std::vector<ParamBind>& binds, const ParamMap& run,
                         const ParamMap& outputValues);

    /**
     * @brief 从 path 取出全局变量名；非 global: 前缀返回空串
     */
    static std::string globalNameFromPath(const std::string& path);
    /**
     * @brief 按 GlobalVarType 把文本建成 ParamValue
     */
    static ParamValue valueFromText(GlobalVarType type, const std::string& text);

private:
    /** @brief 无锁按名查找 */
    int findIndexUnlocked(const std::string& name) const;
    /** @brief 无锁按类型写值 */
    bool setUnlocked(const std::string& name, const ParamValue& value);

    mutable std::mutex m_mu;       ///< UI + 引擎互斥
    std::vector<GlobalVar> m_vars; ///< 变量表
};

}

#endif
