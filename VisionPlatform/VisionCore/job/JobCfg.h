/**
 * @file JobCfg.h
 * @brief JobCfg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_JOBCONFIG_H
#define VISION_JOBCONFIG_H
#include "VisionCoreExport.h"

#include "ToolCfg.h"
#include "JobEnums.h"
#include <map>
#include <string>
#include <vector>

namespace Vision {

/** @class RstRuleTerm @brief 输出结果矩阵一格：来源 + 三态 */
struct RstRuleTerm {
    int kind;           ///< RstTermKind
    ObjectId sourceId;  ///< 工具 id 或规则 id
    int state;          ///< RstTermState
    /** @brief 默认来源为工具、状态忽略 */
    RstRuleTerm() : kind(RstTermTool), state(RstTermIgnore) {}
};

/** @class RstRule @brief 一行逻辑判断：多来源 AND 合成 */
struct VISIONCORE_API RstRule {
    ObjectId id;                     ///< 行 id
    std::string name;                ///< 显示名，如「逻辑判断1」
    ObjectId toolId;                 ///< 可选绑定工具（取图像/数值）
    std::vector<RstRuleTerm> terms; ///< 非 Ignore 的项做 AND
    /** @brief 空规则 */
    RstRule() {}

    /** @brief 读规则项三态；没有则为 Ignore */
    int termState(int kind, const ObjectId& sourceId) const;
    /** @brief 写规则项三态； Ignore 则去掉该项 */
    void setTermState(int kind, const ObjectId& sourceId, int state);
};

/** @class ToolGroup @brief 流程内工具组（顺序 / 并行 / 条件 / 循环） */
struct VISIONCORE_API ToolGroup {
    ObjectId id;                      ///< 组 id
    GroupKind kind;                   ///< 组语义
    std::string name;                 ///< 显示名
    std::vector<ToolGroup> children;  ///< 子组
    ObjectId toolId;                  ///< 叶节点绑定的工具 id
    ObjectId condSourceId;            ///< 条件来源工具 id
    int condWant;                     ///< CondWant
    int loopCount;                    ///< 循环次数；<=0 按 1
    std::string subJobRef;            ///< 子流程名。仅 GroupSubJob
    unsigned parallelLimit;           ///< 并行上限。0 表示默认
    double canvasX;                   ///< 组框左上角场景 X（用户拖过后）
    double canvasY;                   ///< 组框左上角场景 Y
    bool canvasPlaced;                ///< 是否已由用户拖到自定义位置
    /** @brief 未定义语义，循环 1 次，并行上限默认，未摆放 */
    ToolGroup()
        : kind(GroupKindUndefined)
        , condWant(CondThenOnOk)
        , loopCount(1)
        , parallelLimit(0)
        , canvasX(0)
        , canvasY(0)
        , canvasPlaced(false)
    {}

    /** @brief 深拷贝 */
    ToolGroup clone() const;
    /** @brief 收集组内全部工具 id */
    std::vector<ObjectId> toolIds() const;
};

/** @class JobNodeLayout @brief 流程画布上一个工具节点的摆放 */
struct JobNodeLayout {
    ObjectId toolId;   ///< 工具 id
    double canvasX;    ///< 左上角 X
    double canvasY;    ///< 左上角 Y
    bool canvasPlaced; ///< 是否用户拖过
    /** @brief 原点、未摆放 */
    JobNodeLayout()
        : canvasX(0)
        , canvasY(0)
        , canvasPlaced(false)
    {}
};

/** @class JobRouteLayout @brief 一条输入连线的折线几何（键：下游工具+输入口） */
struct JobRouteLayout {
    ObjectId toToolId; ///< 下游工具
    PortSlot toPort;   ///< 下游输入口
    bool routeCustom;  ///< 是否自定义拐点
    std::vector<RoutePoint> routePoints; ///< 中间拐点（场景坐标）
    /** @brief 默认图像输入口、自动走线 */
    JobRouteLayout()
        : toPort(portSlotIn(PortData::Image))
        , routeCustom(false)
    {}
};

/** @class JobLayout @brief 流程布局表：怎么画；与数据连线分离 */
struct JobLayout {
    std::vector<JobNodeLayout> nodes;   ///< 工具节点坐标
    std::vector<JobRouteLayout> routes; ///< 连线折线
};

/** @class JobCfg @brief 工站下的一条检测流程：工具实例列表、连线与结果规则 */
struct VISIONCORE_API JobCfg {
    ObjectId id;                         ///< 流程 id
    std::string name;                    ///< 显示名
    ExecWay execMode;                 ///< 触发方式
    int timeoutMs;                       ///< 超时毫秒
    FailPolicy failPolicy;               ///< 失败策略
    int order;                           ///< 工站内排序
    bool enabled;                        ///< 是否启用
    std::vector<ToolCfg> tools;       ///< 流程内工具实例（含 info/links/param）
    std::vector<ToolLink> links; ///< 流程连线。工具上不另存一份
    JobLayout layout;                    ///< 流程布局表
    ToolGroup root;                      ///< 执行树
    std::vector<RstRule> resultRules; ///< 输出结果配置（逻辑判断行）
    ObjectId resultFromRuleId;           ///< 流程最终结果取自哪一行；空则末级工具
    ObjectId resultToolId;               ///< 画面来源工具。空则末工具
    bool runOnUnloadOnce;                ///< 卸载方案时再跑一次
    bool refreshUiWhileRunning;          ///< 运行时刷新界面
    bool encrypted;                      ///< 流程加密
    int retakeCount;                     ///< NG 重拍次数
    int textX;                           ///< 结果文字横坐标
    int textY;                           ///< 结果文字纵坐标
    int textPx;                          ///< 结果文字字号
    std::string okColor;                 ///< OK 颜色
    std::string ngColor;                 ///< NG 颜色
    bool saveOk;                         ///< 存 OK 图
    bool saveNg;                         ///< 存 NG 图
    bool saveAll;                        ///< 存全部图
    bool saveBatchAll;                   ///< 存本批次全部
    int saveLimit;                       ///< 存图张数上限
    std::string saveDir;                 ///< 存图目录
    int yieldMode;                       ///< 0 关 / 1 批次良率 / 2 实时良率 / 3 NG 数量
    double batchYield;                   ///< 批次良率线
    int recentCount;                     ///< 最近样本数
    double recentYield;                  ///< 实时良率线
    int ngLimit;                         ///< NG 数量线
    int yieldAction;                     ///< 0 报警并停 / 1 只报警
    int singleToolScope;                 ///< 0 上游加自己 / 1 只跑自己
    /** @brief 主动执行、超时 500ms、失败停工站，其余为界面缺省 */
    JobCfg()
        : execMode(ExecActive)
        , timeoutMs(500)
        , failPolicy(FailStopStation)
        , order(1)
        , enabled(true)
        , runOnUnloadOnce(false)
        , refreshUiWhileRunning(true)
        , encrypted(false)
        , retakeCount(0)
        , textX(20)
        , textY(20)
        , textPx(16)
        , okColor("#22A050")
        , ngColor("#D03830")
        , saveOk(false)
        , saveNg(false)
        , saveAll(false)
        , saveBatchAll(true)
        , saveLimit(20)
        , yieldMode(0)
        , batchYield(90)
        , recentCount(100)
        , recentYield(90)
        , ngLimit(20)
        , yieldAction(0)
        , singleToolScope(0)
    {}

    /** @brief 按 id 查找工具配置 */
    ToolCfg* findTool(const ObjectId& id);
    /** @brief 按 id 查找工具配置（只读） */
    const ToolCfg* findTool(const ObjectId& id) const;
    /** @brief 按显示名查找工具 */
    ToolCfg* findToolByName(const std::string& name);
    /** @brief 按显示名查找工具（只读） */
    const ToolCfg* findToolByName(const std::string& name) const;
    /** @brief 按下标取工具 */
    ToolCfg* findToolAt(int index);
    /** @brief 按下标取工具（只读） */
    const ToolCfg* findToolAt(int index) const;
    /** @brief 深拷贝 */
    JobCfg clone() const;
    /** @brief 配方是否相同 */
    bool equals(const JobCfg& other) const;
    /** @brief 工具在列表中的下标；找不到或 id 为空为 -1 */
    int indexOf(const ObjectId& id) const;
    /** @brief 流程内工具的执行顺序 */
    std::vector<ObjectId> execOrder() const;

    /** @brief JobCfg 内可作为结果矩阵列的工具（执行序） */
    std::vector<ObjectId> okNgToolIds() const;

    /** @brief 清理结果规则里已删工具/行的引用 */
    void normalizeRstRules();

    /**
    * @brief 按行顺序求值全部规则，并取指定行作为 JobCfg 结果
    * @return false 表示未配置或指定行不存在，调用方回退末级工具
    */
    bool evalRstRules(const std::map<ObjectId, bool>& toolOk,
        const std::map<ObjectId, bool>& toolRan,const ObjectId& fromRuleId,
        bool* outOk,std::string* outMessage);

    /** @brief 指向该输入端口的连线；没有则为 0 */
    const ToolLink* findLinkToPort(const ObjectId& toId, const PortSlot& toPort) const;

    /** @brief 从各工具 links 汇总全流程连线视图 */
    std::vector<ToolLink> collectLinks() const;

    /** @brief 用工具 links 重建 job.links，这是缓存的唯一写入 */
    void rebuildLinksCache();

    /** @brief 把工具 links 同步到 job.links 缓存 */
    void syncLinksCache();

    /** @brief 把 job.links 拆入各工具 links（旧方案迁移；只补空工具） */
    void distributeLinks();

    /** @brief 工具画布坐标写入 JobLayout */
    void syncLayoutFromTools();

    /** @brief JobLayout 写回工具画布镜像字段 */
    void applyLayoutToTools();

    /** @brief 先补全执行树，再按树重建数据连线 */
    void rebuildSerialLinks();

    /** @brief 补根组、剪掉失效叶子，并把未入树的工具挂到根 */
    void ensureTree();

    /** @brief 按执行树重建数据连线 */
    void rebuildLinksFromTree();

    /**
    * @brief 画布拖线：接到指定输入端口；同端口已有来源则替换
    * @param err 失败原因
    */
    bool connectPorts(const ObjectId& fromId, const PortSlot& fromPort,
    const ObjectId& toId, const PortSlot& toPort, std::string* err);

    /** @brief 按 links 下标断开一条数据连线 */
    bool disconnectLink(int index);

    /** @brief 断开指向指定输入端口的连线；无连线时返回 false */
    bool disconnectPort(const ObjectId& toId, const PortSlot& toPort);

    /** @brief 按 id 查找工具组 */
    ToolGroup* findGroup(const ObjectId& id);

    /** @brief 查找绑定该工具的叶节点 */
    ToolGroup* findLeaf(const ObjectId& toolId);

    /** @brief 查找直接包含该子节点的父组 */
    ToolGroup* findParent(const ObjectId& childId);

    /** @brief 追加工具，并在选中处挂到执行树 */
    bool insertTool(const ToolCfg& t, const ObjectId& selectId);

    /**
    * @brief 新工具加入 JobCfg 时分配合适画布坐标，避免与已有节点叠在一起
    * @param selectId 当前选中工具；已放置则在其下方插入
    * @note 并行/条件组及其支路内不写 canvasPlaced，仍由画布按树结构横/纵向排布
    */
    void assignNewToolCanvasPosition(ToolCfg* t, const ObjectId& selectId);

    /**
    * @brief 自动排列画布：工具/组水平中心对齐、自上而下；清除自定义连线路由以便横平竖直绕行
    */
    void autoLayoutCanvas();

    /** @brief 在选中处插入带两条空支路的并行组 */
    ObjectId addParallelGroup(const ObjectId& selectId);

    /** @brief 在选中处插入条件组；选中叶子时以其判定为条件 */
    ObjectId addIfGroup(const ObjectId& selectId);

    /** @brief 在选中处插入循环组 */
    ObjectId addLoopGroup(const ObjectId& selectId);

    /** @brief 设置循环组次数；至少为 1 */
    bool setLoopCount(const ObjectId& loopId, int count);

    /** @brief 给并行组追加一条空的顺序支路 */
    ObjectId addSequenceBranch(const ObjectId& parallelId);

    /** @brief 设置条件组的来源工具与期望判定 */
    bool setIfCondition(const ObjectId& ifId, const ObjectId& sourceId, int want);

    /** @brief 在同级中上移或下移节点 */
    bool moveTreeNode(const ObjectId& nodeId, int delta);

    /** @brief 改工具组显示名（根组除外） */
    bool renameGroup(const ObjectId& id, const std::string& name);

    /** @brief 删除工具组及其工具；条件组的两个分支不可删 */
    bool removeGroup(const ObjectId& id);

    /** @brief 复制工具组并插到原节点之后 */
    ObjectId duplicateGroup(const ObjectId& id);

    /** @brief 在同级中上移或下移工具 */
    bool moveTool(const ObjectId& id, int delta);

    /** @brief 从执行树和工具列表中删除该工具 */
    bool removeTool(const ObjectId& id);

    /** @brief 复制工具并插到原叶子附近 */
    ObjectId duplicateTool(const ObjectId& id);

    /** @brief 同步改工具显示名与对应叶子名 */
    bool renameTool(const ObjectId& id, const std::string& name);
};

}


#endif
