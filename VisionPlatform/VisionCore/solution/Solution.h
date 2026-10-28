#ifndef VISION_SOLUTION_H
#define VISION_SOLUTION_H
#include "VisionCoreExport.h"

/**
 * @file Solution.h
 * @brief 整包视觉方案 Solution（*.vpsol 根对象）
 *
 * 相关类型见：
 * - job/JobCfg.h、station/StationCfg.h
 * - ToolCfg.h、HardwareDevice.h
 */

#include "HardwareDevice.h"
#include "StationCfg.h"
#include "CompositeDef.h"
#include "CommTypes.h"
#include "AppMode.h"
#include <string>
#include <vector>

namespace Vision {

/** @class Solution @brief 整包视觉方案（*.vpsol 根对象） */
struct VISIONCORE_API Solution {
    std::string name;                      ///< 方案名
    std::string filePath;                  ///< 文件路径
    std::string lastSavedById;             ///< 最后保存用户 id
    std::string lastSavedByName;           ///< 最后保存用户名
    std::string lastSavedAt;               ///< 最后保存时间
    std::string platformVersion;           ///< 保存时软件版本（追溯用，打开不校验）
    std::vector<HardwareDevice> inventory; ///< 设备台账
    std::vector<CommChannel> commChannels; ///< 通信通道台账
    std::vector<StationCfg> stations;   ///< 工站列表
    std::vector<CompositeDef> composites; ///< 项目组合工具
    RuntimeStationLayout runtimeStationLayout; ///< 多工站运行画面：分页签或拼贴
    bool dirty;                            ///< 是否有未保存修改
    bool haltAll;                          ///< 停全部。不进方案文件

    Solution()
        : runtimeStationLayout(RuntimeStationsTabs)
        , dirty(false)
        , haltAll(false)
    {}

    /** @brief 按 id 查找工站 */
    StationCfg* findStationConfig(const ObjectId& id);
    /** @brief 在全部工站的流程中按 id 查找 */
    JobCfg* findJobConfig(const ObjectId& id);
    /** @brief 在全部流程中按 id 查找工具 */
    ToolCfg* findTool(const ObjectId& id);
    /** @brief 在全部流程中按 id 查找工具（只读） */
    const ToolCfg* findTool(const ObjectId& id) const;
    /** @brief 按 id 查找硬件设备 */
    HardwareDevice* findDevice(const ObjectId& id);
    /** @brief 按 id 查找硬件设备（只读） */
    const HardwareDevice* findDevice(const ObjectId& id) const;
    /** @brief 按 id 查找通信通道 */
    CommChannel* findCommChannel(const ObjectId& id);
    /** @brief 按 id 查找通信通道（只读） */
    const CommChannel* findCommChannel(const ObjectId& id) const;
    /** @brief 按 id 查找工站（只读） */
    const StationCfg* findStationConfig(const ObjectId& id) const;
    /**
     * @brief 找出工具所属流程
     * @param stationOut 非空时写出所属工站
     */
    JobCfg* findJobCfgOfTool(const ObjectId& toolId, StationCfg** stationOut = 0);
    /**
     * @brief 查找流程所属工站
     * @param jobId 流程 id
     * @return 所属 StationCfg；找不到为 0
     */
    StationCfg* findStationCfgOfJob(const ObjectId& jobId);
    /** @brief 第一个工站；没有则为 0 */
    StationCfg* primaryStation();
    /** @brief 提交流程草稿 */
    bool commitJob(const JobCfg& draft);
    /** @brief 提交工站草稿 */
    bool commitStation(const StationCfg& draft);
    /** @brief 提交设备草稿 */
    bool commitDevice(const HardwareDevice& draft);
    /** @brief 新建空方案 */
    static Solution makeEmpty();
};


}

#endif
