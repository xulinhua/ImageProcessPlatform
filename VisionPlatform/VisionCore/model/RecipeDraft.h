/**
 * @file RecipeDraft.h
 * @brief RecipeDraft头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RECIPEDRAFT_H
#define VISION_RECIPEDRAFT_H
#include "VisionCoreExport.h"

#include "AlgLibKind.h"
#include "Solution.h"

namespace Vision {

/** @class ToolRecipe @brief 工具配方视图（draft 编辑字段；对应 StationRecipe / DeviceRecipe） */
struct VISIONCORE_API ToolRecipe {
    ParamMap params;              ///< 配方参数
    std::vector<HsCommData::HsObject> rois;    ///< ROI 列表（界面绘制，属参数）
    ObjectId deviceId;            ///< 绑定设备
    AlgLibKind algLib;            ///< 算法库
    ToolRecipe() : algLib(AlgLibHal) {}
    /** @brief 从配置加载配方字段 */
    void loadFrom(const ToolCfg& t);
    /** @brief 写回配方字段 */
    void applyTo(ToolCfg* t) const;
    /** @brief 是否与另一份配方相同 */
    bool equals(const ToolRecipe& other) const;
    /** @brief 写回正式工具配置，保留 runtime */
    void commitTo(ToolCfg* formal) const;
};

/**
 * @class StationRecipe
 * @brief 工站配方 draft 字段
 */
struct VISIONCORE_API StationRecipe {
    std::string name;                    ///< 工站名称
    std::string stationCode;             ///< 工位编号
    bool enabled;                        ///< 是否使能
    JobSchedule jobSchedule;             ///< JobCfg 编排
    int timeoutMs;                       ///< 默认超时
    FailPolicy failPolicy;               ///< 失败策略
    std::vector<DeviceRef> boundDevices; ///< 绑定设备
    std::vector<JobCfg> jobs;               ///< 本工站流程
    std::string commChannelId;           ///< 通信通道

    /** @brief 缺省工站配方：使能、串行、超时 500 毫秒、失败停工站 */
    StationRecipe();
    /** @brief 从 StationCfg 加载 */
    void loadFrom(const StationCfg& st);
    /** @brief 写回 StationCfg */
    void applyTo(StationCfg* st) const;
    /** @brief 是否相同 */
    bool equals(const StationRecipe& other) const;
};

/**
 * @class DeviceRecipe
 * @brief 设备配方 draft 字段
 */
struct VISIONCORE_API DeviceRecipe {
    std::string name;           ///< 名称
    DeviceKind kind;            ///< 类型
    CameraBackend cameraBackend; ///< 相机后端
    ParamMap defaultParams;     ///< 默认参数

    DeviceRecipe()
        : kind(DeviceKindUndefined)
        , cameraBackend(CameraBackendUndefined)
    {}
    /** @brief 从 HardwareDevice 加载 */
    void loadFrom(const HardwareDevice& d);
    /** @brief 写回 HardwareDevice */
    void applyTo(HardwareDevice* d) const;
    /** @brief 是否相同 */
    bool equals(const DeviceRecipe& other) const;
};

VISIONCORE_API /** @brief 比较两份 ParamMap 是否相同 */
bool paramMapEquals(const ParamMap& a, const ParamMap& b);

VISIONCORE_API /** @brief 是否为运行态键（画布、调试等，不参与 draft 对比） */
bool isRuntimeParamKey(const std::string& key);


}

#endif
