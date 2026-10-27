/**
 * @file CompositeAccess.cpp
 * @brief CompositeAccess实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CompositeDef.h"

namespace Vision {

namespace {
CompositeFindFn g_find = nullptr;
}

// 注册定义查找
void setCompositeFinder(CompositeFindFn fn)
{
    g_find = fn;
}

// 按 id 取组合定义
const CompositeDef* findComposite(const ObjectId& id)
{
    if (!g_find || id.isNull()) {
        return 0;
    }
    return g_find(id);
}

// 把已公开字段抄进一条普通工具记录
ToolCfg CompositeDef::makeNode() const
{
    ToolCfg t;
    t.id = ObjectId::create();
    t.toolName = "composite";
    t.name = name;
    t.type = ToolType::Detect;
    t.info.compositeId = id;
    t.info.compositeScope = static_cast<int>(scope);
    t.info.toolVersion = version;
    t.param.paraVersion = "1.0.0.0";
    for (size_t i = 0; i < paras.size(); ++i) {
        const CompositeParaPub& pub = paras[i];
        const ToolCfg* found = this->inner.findTool(pub.innerToolId);
        if (!found) {
            continue;
        }
        const ParamMap& src = pub.learn ? found->param.learn : found->param.run;
        if (!src.contains(pub.key)) {
            continue;
        }
        ParamMap& dst = pub.learn ? t.param.learn : t.param.run;
        dst.insert(pub.key, src.value(pub.key));
    }
    for (size_t i = 0; i < rois.size(); ++i) {
        const CompositeRoiPub& pub = rois[i];
        const ToolCfg* found = this->inner.findTool(pub.innerToolId);
        if (!found) {
            continue;
        }
        for (size_t k = 0; k < found->param.roiItems.size(); ++k) {
            ToolRoiItem item = found->param.roiItems[k];
            if (item.slot != pub.innerSlot) {
                continue;
            }
            item.slot = pub.name;
            t.param.roiItems.push_back(item);
            if (item.shape.isInitialized() && !item.shape.isEmpty()) {
                t.param.rois.push_back(item.shape);
            }
        }
    }
    return t;
}

}