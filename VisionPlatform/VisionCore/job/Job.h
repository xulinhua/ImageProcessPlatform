/**
 * @file Job.h
 * @brief Job头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RUNTIME_JOB_H
#define VISION_RUNTIME_JOB_H
#include "VisionCoreExport.h"

#include "RunListener.h"
#include "Solution.h"
#include "ToolBase.h"
#include <atomic>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace Vision {

/**
 * @class JobRuntime
 * @brief 单条流程的运行态：工具级图/OK/数值缓存（不进配方 JSON）
 */
struct VISIONCORE_API JobRuntime {
    std::map<ObjectId, std::shared_ptr<HsCommData::HsImage> > images;              ///< 工具输出图
    std::map<ObjectId, std::shared_ptr<std::vector<HsCommData::HsObject> > > overlays; ///< 工具结果几何
    std::map<ObjectId, bool> ok;                                  ///< 工具 OK
    std::map<ObjectId, std::shared_ptr<ParamMap> > values;                          ///< 工具数值
    std::map<ObjectId, std::shared_ptr<ToolIo> > io;                                ///< 工具分类输出
    std::map<ObjectId, int> elapsedMs;                          ///< 工具耗时 ms
    std::map<ObjectId, std::vector<ToolTrace> > traces;         ///< 本拍日志和调试记录
    std::map<ObjectId, std::vector<ToolInfoItem> > infos;       ///< 本拍信息记录，调试页读取
    std::map<ObjectId, std::vector<ToolInfoItem> > prodInfos;   ///< 自动运行的信息，待写入 prod.db
    std::shared_ptr<HsCommData::HsImage> source;                                  ///< 本流程源图
    ToolOut lastOut;                                              ///< 最近一次 run 结果
    int64_t frameSeq;                                             ///< 帧序号
    int loopIndex;                                                ///< 当前循环下标；-1 表示不在循环内
    int loopCount;                                                ///< 当前循环总次数
    JobRuntime() : frameSeq(1), loopIndex(-1), loopCount(0) {}
    /** @brief 清空缓存 */
    void clear();
};

/**
 * @class Job
 * @brief 流程综合体：配置 + 运行态 + 执行（可独立于工站试跑）
 * @note 不读写 JSON、不碰窗口；config 不拥有；下属 ToolBase* 由本类 clone/destroy
 * @note 不持有/不访问上级 Station；流程级 source/日志由本层收尾
 */
class VISIONCORE_API Job {
public:
    /************************************************** 配置 **************************************************/
    /**
     * @brief 绑定流程配置
     * @param config 方案内或草稿 JobCfg，不拥有，须长于本对象
     */
    explicit Job(JobCfg* config);
    /** @brief 释放下属 ToolBase 实例 */
    ~Job();

    /** @brief 当前配置（可空） */
    JobCfg* config() const;
    /** @brief 换绑配置并重建工具实例（工站重建子流程时用） */
    void setConfig(JobCfg* config);
    /** @brief 运行态 */
    JobRuntime& runtime();
    /** @brief 运行态（只读） */
    std::shared_ptr<const JobRuntime> publishedRuntime() const;

    /************************************************** 日志 **************************************************/
    /** @brief 设置日志监听，不拥有 */
    void setListener(IRunListener* listener);
    /**
     * @brief 设置日志用的工站 id（非 Station*；由 Station 注入，仅打标签）
     * @param stationId 工站配置 id，可空
     */
    void setStationId(const ObjectId& stationId);
    /** @brief 写一行运行日志（Info；自动填 job/station id） */
    void emitLog(const std::string& line);
    /**
     * @brief 写结构化运行日志
     * @param level 级别
     * @param message 正文
     * @param toolId 相关工具，可空
     * @param elapsedMs 耗时，未知传 -1
     */
    void emitLog(RunLogLevel level, const std::string& message,
                 const ObjectId& toolId = ObjectId(), int elapsedMs = -1);

    /************************************************** 工具实例 **************************************************/
    /** @brief 按配置对齐工具实例。同 id 同类型留下，缺的创建，多的销毁 */
    void rebuildTools();
    /** @brief 把当前实例 info/links/param/runtime 回写到 JobCfg.tools */
    void syncToolsToConfig();
    /** @brief 按 id 查找工具实例 */
    ToolBase* findTool(const ObjectId& id) const;
    /** @brief 工具实例个数 */
    int toolCount() const;
    /** @brief 按下标取工具 */
    ToolBase* toolAt(int index) const;

    /************************************************** 缓存 **************************************************/
    /** @brief 清空工具缓存与源图 */
    void clearRuntime();
    /** @brief 某工具上次输出图 */
    HsCommData::HsImage cachedImage(const ObjectId& toolId) const;
    /** @brief 本流程源图 */
    HsCommData::HsImage sourceImage() const;
    /** @brief 某工具上次结果几何 */
    std::vector<HsCommData::HsObject> cachedOverlay(const ObjectId& toolId) const;
    /** @brief 是否已有该工具缓存 */
    bool hasCache(const ObjectId& toolId) const;
    /** @brief 该工具是否在本周期内跑过 */
    bool hasToolRun(const ObjectId& toolId) const;
    /** @brief 该工具最近一次 OK/NG；未跑过返回 false */
    bool cachedToolOk(const ObjectId& toolId, bool* ok) const;
    /** @brief 该工具最近一次耗时 ms；未跑过返回 -1 */
    int cachedElapsedMs(const ObjectId& toolId) const;
    /** @brief 按连线解析的输入图 */
    HsCommData::HsImage cachedInputImage(const JobCfg& job, const ObjectId& toolId) const;
    /** @brief 按 in.pose 解析的上游位姿 */
    bool cachedInputPose(const JobCfg& job, const ObjectId& toolId, HsCommData::Pose2* pose) const;
    /** @brief 某工具上次输出数值 */
    ParamMap cachedValues(const ObjectId& toolId) const;
    /** @brief 某工具上次分类输出 */
    ToolIo cachedIo(const ObjectId& toolId) const;

    /************************************************** 执行 **************************************************/
    /** @brief 跑单个工具；recipeTool 非空时用其配方字段试跑 */
    ToolOut runTool(Solution* sol, const ObjectId& toolId, const ToolCfg* recipeTool = 0);
    /**
     * @brief 按执行序先跑上游再跑本工具
     * @param recipeJob 整流程草稿（优先于 recipeTool 同 id）
     */
    ToolOut runToolChain(Solution* sol, const ObjectId& toolId, const ToolCfg* recipeTool = 0,
                         const JobCfg* recipeJob = 0);
    /** @brief 跑整条流程；recipe 非空时用草稿配置试跑 */
    ToolOut run(Solution* sol, const JobCfg* recipe = 0);
    /** @brief 自动运行前把各工具参数从 map 填到参数类，本次流程只调一次 */
    void initAutoParas();

    /**
     * @brief 尝试占用本流程（防重入；设计/复检/运行互斥）
     * @return false 表示已在执行
     */
    bool tryBeginRun();
    /** @brief 结束占用（与 tryBeginRun 成对） */
    void endRun();
    /** @brief 是否正在执行 */
    bool isRunning() const;

private:
    /************************************************** 日志 **************************************************/
    /** @brief 写日志给监听器 */
    void log(const std::string& line);
    /** @brief 写结构化日志给监听器（补全 station/job id） */
    void log(const RunLogEvent& ev);

    /************************************************** 输入 **************************************************/
    /** @brief 优先图像连线的上游图；否则取执行序中上一张缓存图 */
    HsCommData::HsImage inputImage(const JobCfg& job, const ObjectId& toolId) const;
    /** @brief 合并区域、轮廓、ROI 连线的上游几何 */
    std::vector<HsCommData::HsObject> inputGraphics(const JobCfg& job, const ObjectId& toolId) const;
    /** @brief 合并位姿连线的上游位姿（后写覆盖） */
    bool inputPose(const JobCfg& job, const ObjectId& toolId, HsCommData::Pose2* pose) const;
    /** @brief 合并逻辑连线的上游数值 */
    ParamMap upstreamValues(const JobCfg& job, const ObjectId& toolId) const;
    /** @brief 按连线把上游分类向量汇总到本工具输入 */
    ToolIo inputIo(const JobCfg& job, const ObjectId& toolId) const;
    /** @brief 用旧缓存拼一份分类数据 */
    ToolIo ioFromLegacyCache(const ObjectId& toolId) const;

    /************************************************** 执行 **************************************************/
    /** @brief 按组递归执行：叶子跑工具，条件选支，并行全跑，循环按次数重跑 */
    ToolOut runGroup(Solution* sol, JobCfg* job, const ToolGroup& g, bool fallbackOk);
    /** @brief 取条件源判定；期望 NG 时取反 */
    bool evalIf(const ToolGroup& g, bool fallbackOk) const;
    /** @brief 把草稿配方合并进正式工具副本，供本次执行使用 */
    ToolCfg resolveRecipeTool(const ToolCfg* formal, const ToolCfg* recipeTool,
                               const JobCfg* recipeJob) const;
    /** @brief 单工具执行体（调用方已占用 m_executing） */
    ToolOut runToolBody(Solution* sol, const ObjectId& toolId, const ToolCfg* recipeTool);

    /************************************************** 实例数据 **************************************************/
    /** @brief 把 ToolCfg 四件套写入工具实例 */
    std::string paramStamp(const ToolParam& param) const;
    void applyToolConfig(ToolBase* inst, const ToolCfg& config);
    /** @brief 把实例四件套收回配置，不动画布坐标 */
    void harvestToolConfig(const ToolBase* inst, ToolCfg* config);
    /** @brief 执行工具实例，没有实例时用注册表原型 */
    ToolOut runToolInstance(ToolBase* inst, const ToolIn& in, ToolCfg* formal);

    /************************************************** 成员 **************************************************/
    struct RtView {
        const JobRuntime* rt; ///< 可读的运行态
        std::shared_ptr<const JobRuntime> keep; ///< 握住已发布快照
        RtView() : rt(0) {}
    };
    /** @brief 执行线程读草稿，其它线程读已发布快照 */
    RtView viewRuntime() const;
    /** @brief 把草稿换成界面可见的快照 */
    void publishRuntime();

    JobCfg* m_config;                 ///< 正式配置，不拥有
    JobRuntime m_runtime;             ///< 本流程运行态
    mutable std::mutex m_publishMu; ///< 只保护快照指针
    std::shared_ptr<const JobRuntime> m_published; ///< 界面可见的上一拍
    std::vector<ToolBase*> m_tools;   ///< 下属工具实例（拥有；经 destroy 释放）
    IRunListener* m_listener;         ///< 日志监听（常为 RunLogHub）
    ObjectId m_stationId;             ///< 日志标签用工站 id（非上级指针）
    std::atomic<bool> m_executing;    ///< 执行占用（防重入）
    bool m_autoPrepared;              ///< 自动运行参数是否已填好
    std::map<std::string, std::string> m_paramStamp;
    const JobCfg* m_recipeJob;        ///< 当前试跑用的流程草稿
    const ToolCfg* m_recipeTool;      ///< 当前试跑用的单工具草稿
};

}

#endif


