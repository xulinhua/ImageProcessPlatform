/**
 * @file ToolCfg.h
 * @brief ToolCfg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOL_CONFIG_H
#define VISION_TOOL_CONFIG_H
#include "VisionCoreExport.h"

#include "ToolInstanceData.h"
#include "ToolGeom.h"
#include <string>
#include <vector>

namespace Vision {

/** @class RoutePoint @brief 连线折线中间拐点（场景坐标；端点由针脚实时算） */
struct RoutePoint {
    double x; ///< 场景 X
    double y; ///< 场景 Y
    /** @brief 原点 */
    RoutePoint() : x(0), y(0) {}
    /** @brief 指定场景坐标 */
    RoutePoint(double xx, double yy) : x(xx), y(yy) {}
};

/**
 * @class ToolLink
 * @brief 流程级连线视图（调度/画布缓存）：含两端工具 id；折线几何见 JobLayout
 */
struct ToolLink {
    ObjectId fromId;   ///< 来源工具 id
    ObjectId toId;     ///< 目标工具 id
    PortSlot fromPort; ///< 来源端口槽位
    PortSlot toPort;   ///< 目标端口槽位
    bool routeCustom;  ///< 是否使用手动路由拐点（过渡：仍可读；真源在 JobLayout）
    std::vector<RoutePoint> routePoints; ///< 中间拐点；空且 routeCustom 时按自动
    int index;             ///< 上游该口的第几个；-1 表示整口
    RegionUse use;          ///< 区域用途，只对 Region / Roi 有意义
    ToolLinkOrigin origin; ///< ToolLinkOrigin
    /** @brief 默认图像出口接图像入口、整口、普通区域、手动连线 */
    ToolLink()
        : fromPort(portSlotOut(PortData::Image))
        , toPort(portSlotIn(PortData::Image))
        , routeCustom(false)
        , index(-1)
        , use(RegionUse::Normal)
        , origin(LinkOriginManual)
    {}
};

/**
 * @class ToolCfg
 * @brief 流程内工具存盘聚合：info / links / param 平行；runtime 不落盘
 * @note 与 Job 内 ToolBase* 实例同步；扁平字段为嵌套成员引用别名
 */
struct VISIONCORE_API ToolCfg {
    ToolInfo info;                      ///< 身份
    std::vector<ToolInputLink> links;   ///< 旧文件迁入流程 links 后清空，不再作为来源
    ToolParam param;                      ///< 参数
    ParamMap runtime;                   ///< 运行态（不持久化）

    /** 画布镜像（过渡）：真源迁至 JobLayout；读写仍可用，保存时写入 layout */
    double canvasX;    ///< 画布节点左上角 X
    double canvasY;    ///< 画布节点左上角 Y
    bool canvasPlaced; ///< 是否已由用户拖到自定义位置

    ObjectId& id;                 ///< -> info.id
    std::string& toolName;       ///< -> info.toolName
    AlgLibKind& algLib;        ///< -> param.algLib
    ToolType& type;              ///< -> info.type
    std::string& name;           ///< -> info.name
    ParamMap& params;            ///< -> param.run
    std::vector<HsCommData::HsObject>& rois;  ///< -> param.rois
    bool& enabled;               ///< -> info.enabled
    ObjectId& deviceId;          ///< -> param.deviceId

    /** @brief 默认构造并绑定引用别名 */
    ToolCfg()
        : canvasX(0)
        , canvasY(0)
        , canvasPlaced(false)
        , id(info.id)
        , toolName(info.toolName)
        , algLib(param.algLib)
        , type(info.type)
        , name(info.name)
        , params(param.run)
        , rois(param.rois)
        , enabled(info.enabled)
        , deviceId(param.deviceId)
    {}

    /** @brief 拷贝构造（引用绑到本对象嵌套成员） */
    ToolCfg(const ToolCfg& o)
        : info(o.info)
        , links(o.links)
        , param(o.param)
        , runtime(o.runtime)
        , canvasX(o.canvasX)
        , canvasY(o.canvasY)
        , canvasPlaced(o.canvasPlaced)
        , id(info.id)
        , toolName(info.toolName)
        , algLib(param.algLib)
        , type(info.type)
        , name(info.name)
        , params(param.run)
        , rois(param.rois)
        , enabled(info.enabled)
        , deviceId(param.deviceId)
    {}

    /** @brief 赋值（不改引用绑定目标） */
    ToolCfg& operator=(const ToolCfg& o);


    /** @brief 把点轮廓收成直线，并同步分区 */
    void syncRois();
    /** @brief 从 roiItems 回写绘制几何到 rois（引用项保留） */
    void rebuildRoisFromItems();
    /** @brief 实际使用的 ROI 列表（仅绘制几何；运行时引用由 Job 解析） */
    std::vector<HsCommData::HsObject> effectiveRois() const;
    /** @brief 调试预览：绘制用 shape，引用从 Job 缓存解析 */
    std::vector<HsCommData::HsObject> displayRois(const class Job* job) const;
    /** @brief 追加一条 ROI 记录（绘制或引用） */
    void appendRoiItem(const ToolRoiItem& item);
    /** @brief 按 roiItems 下标删除；同步 rois */
    void removeRoiItemAt(int index);
    /** @brief 更新指定 roiItems；同步 rois */
    void setRoiItemAt(int index, const ToolRoiItem& item);
    /** @brief roiItems 下标对应的绘制几何下标；非绘制返回 -1 */
    int drawIndexOfItem(int itemIndex) const;
    /** @brief 查注册表取工具能力 */
    ToolCaps caps() const;
    /** @brief 按名查工具能力 */
    static ToolCaps capsFor(const std::string& toolName);
    /** @brief 是否在图上画 ROI */
    bool usesRoi() const;
    /** @brief 是否允许多 ROI */
    bool usesMultiRoi() const;
    /**
     * @brief 新增 ROI 时是否保留已有项（多分区或分区上限>1）
     */
    bool keepsExistingRoisOnAdd() const;
    /** @brief 新增 ROI 优先写入的分区键（未满槽优先） */
    std::string nextRoiSlotKey() const;
    /** @brief 推荐几何种类；-1 表示不限 */
    int preferredRoiKind() const;
    /** @brief 几何种类是否与推荐一致 */
    bool roiKindMatches(int kind) const;
    /** @brief 用注册表缺省参数补齐缺失项 */
    void ensureDefaults();
    /** @brief 复制并换新 id */
    ToolCfg duplicate() const;
    /** @brief 把误写入 params 的运行态键迁出 */
    void migrateRuntime();
    /** @brief 全部 ROI 的绘制层 */
    std::vector<HsCommData::Graphic> drawings() const;
    /** @brief 绘制层，并按选中行调透明度 */
    std::vector<HsCommData::Graphic> drawingsForDisplay(int selectedIndex) const;
    /** @brief 把一笔绘制写进配置 */
    void applyDrawnRoi(const HsCommData::Graphic& g);
    /** @brief 按行号更新已有 ROI */
    void applyDrawnRoiAt(const HsCommData::Graphic& g, int index);
    /** @brief 清空全部 ROI */
    void clearRois();
    /** @brief 删除最后一个 ROI */
    void removeLastRoi();
    /** @brief 按行号删除一条 ROI */
    void removeRoiAt(int index);
    /** @brief 按注册名创建；未登记则手填 */
    static ToolCfg make(const std::string& toolName, const ObjectId& deviceId);
    /** @brief 按注册名创建；未登记返回空记录 */
    static ToolCfg createRegistered(const std::string& toolName, const ObjectId& deviceId = ObjectId());
    /** @brief 旧 toolName 映射到当前名 */
    static std::string canonicalize(const std::string& n);
    /** @brief 可作为结果矩阵列 */
    bool producesOkNg() const
    {
        return enabled && toolTypeProducesOkNg(type);
    }
};

}

#endif
