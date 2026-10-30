/**
 * @file CompositeDef.h
 * @brief CompositeDef头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMPOSITE_DEF_H
#define VISION_COMPOSITE_DEF_H
#include "VisionCoreExport.h"

#include "JobCfg.h"
#include <string>
#include <vector>

namespace Vision {

/** @brief 创建时选定。项目的跟方案走，独立的本机共用 */
enum CompositeScope {
    CompositeSolution = 0, ///< 项目组合工具
    CompositeLibrary = 1   ///< 独立组合工具
};

/** @class CompositePort @brief 外壳上的一针，接到内部某个工具的一口 */
struct CompositePort {
    std::string name;      ///< 外壳上的显示名
    PortDir dir;           ///< 输入或输出
    PortData data;         ///< 数据类型
    ObjectId innerToolId;  ///< 内部工具
    PortSlot innerPort;    ///< 内部那一口
    /** @brief 默认图像输入针 */
    CompositePort() : dir(PortDir::In), data(PortData::Image) {}
};

/** @class CompositeRoiPub @brief 外壳上的一个 ROI 分区 */
struct CompositeRoiPub {
    std::string name;      ///< 外壳上的显示名
    ObjectId innerToolId;  ///< 内部工具
    std::string innerSlot; ///< 内部分区键，对应 RoiSlotRule.slot
};

/** @class CompositeParaPub @brief 外壳上的一个参数 */
struct CompositeParaPub {
    std::string name;      ///< 外壳上的显示名
    ObjectId innerToolId;  ///< 内部工具
    std::string key;       ///< 内部参数键
    bool learn;            ///< true 取学习参数，否则取运行参数
    /** @brief 默认取运行参数 */
    CompositeParaPub() : learn(false) {}
};

/** @class CompositeDef @brief 一份组合定义 */
struct VISIONCORE_API CompositeDef {
    ObjectId id;                              ///< 定义 id
    CompositeScope scope;                     ///< 项目或独立
    std::string name;                         ///< 工具箱里的名字
    std::string description;                  ///< 说明
    std::string version;                      ///< 四段版本
    JobCfg inner;                             ///< 内部流程
    std::vector<CompositePort> ports;         ///< 公开针脚
    std::vector<CompositeRoiPub> rois;        ///< 公开的 ROI 分区
    std::vector<CompositeParaPub> paras;      ///< 公开的参数键
    /** @brief 默认独立组合、版本 1.0.0.0 */
    CompositeDef() : scope(CompositeLibrary), version("1.0.0.0") {}
    /** @brief 抄入已公开字段，生成流程节点 */
    ToolCfg makeNode() const;
};

/** @brief 按 id 查找。由 CompositeMgr 注册 */
typedef const CompositeDef* (*CompositeFindFn)(const ObjectId& id);
/** @brief 注册全局组合定义查找函数（主窗口启动时注入 CompositeMgr::findRaw） */
VISIONCORE_API void setCompositeFinder(CompositeFindFn fn);
/** @brief 按 id 取组合定义；未注册查找函数或 id 为空时返回空 */
VISIONCORE_API const CompositeDef* findComposite(const ObjectId& id);


}
#endif
