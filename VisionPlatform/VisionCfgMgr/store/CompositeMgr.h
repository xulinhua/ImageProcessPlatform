/**
 * @file CompositeMgr.h
 * @brief CompositeMgr头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMPOSITE_MGR_H
#define VISION_COMPOSITE_MGR_H
#include "VisionConfigExport.h"

#include "CompositeDef.h"
#include "Solution.h"
#include <string>
#include <vector>

namespace Vision {

/** @class CompositeMgr @brief 组合工具目录。进程内唯一 */
class VISION_CONFIG_API CompositeMgr {
public:
    static CompositeMgr& instance();
    /** @brief 扫描 data\composites\ */
    bool load(std::string* error);
    /** @brief 把这一份独立组合写成自己的文件 */
    bool saveOne(const ObjectId& id, std::string* error) const;
    /** @brief 打开方案时交出方案内的那一份，关闭时传空 */
    void setSolutionDefs(std::vector<CompositeDef>* defs);
    /** @brief 当前打开的方案，删除时用来查引用 */
    void setOpenSolution(Solution* sol);
    /** @brief 新建。范围创建后不变 */
    ObjectId create(CompositeScope scope, const std::string& name, std::string* error);
    /** @brief 按 id 取定义 */
    CompositeDef* find(const ObjectId& id);
    const CompositeDef* find(const ObjectId& id) const;
    /** @brief 删除。流程里还引用着的不允许删 */
    bool remove(const ObjectId& id, std::string* error);
    /** @brief 两种范围合在一起 */
    std::vector<CompositeDef*> all();
    /** @brief 供流程执行注册的查找函数 */
    static const CompositeDef* findRaw(const ObjectId& id);

private:
    CompositeMgr();
    std::vector<CompositeDef> m_library;       ///< 独立组合
    std::vector<CompositeDef>* m_solution;     ///< 项目组合，不拥有
    Solution* m_open;                          ///< 当前方案，不拥有
};

}
#endif