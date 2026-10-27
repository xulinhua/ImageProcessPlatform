/**
 * @file StationCfg.h
 * @brief StationCfg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STATIONCONFIG_H
#define VISION_STATIONCONFIG_H
#include "VisionCoreExport.h"

#include "HardwareDevice.h"
#include "JobCfg.h"
#include "StationEnums.h"
#include <string>
#include <vector>

namespace Vision {

/** @class RuntimePane @brief 运行态一个图像格：底图来源与多路叠加 */
struct RuntimePane {
    ObjectId imageToolId;                 ///< 底图工具；空则用工站本拍结果图
    std::vector<ObjectId> overlayToolIds; ///< 叠加工具；空 id 表示工站本拍图形
    bool showOverlay;                     ///< 是否画叠加
    bool enabled;                         ///< 该格是否显示
    RuntimePane()
        : showOverlay(true)
        , enabled(true)
    {}
};

/** @class RuntimeView @brief 工站运行主界面布局（随方案存盘，不是本机设置） */
struct VISIONCORE_API RuntimeView {
    int rows;                       ///< 行数，1 至 3
    int cols;                       ///< 列数，1 至 3
    bool showStats;                 ///< 右侧产量统计
    bool showHistory;               ///< 底部结果历史
    bool showSideButtons;           ///< 左侧开始/停止列
    std::vector<RuntimePane> panes; ///< 最多 9 格，按行优先
    RuntimeView()
        : rows(1)
        , cols(1)
        , showStats(true)
        , showHistory(true)
        , showSideButtons(true)
    {}
    /** @brief 格子数 */
    int slotCount() const;
    /** @brief 行列夹到 1 至 3，窗格补到 9 */
    void normalize();
};

/** @brief 当前布局的格子数（行乘列） */

/**
 * @class StationCfg
 * @brief 工站配方配置：多条 JobCfg、绑定设备与运行界面布局
 */
struct StationCfg {
    ObjectId id;                         ///< 工站 id
    std::string name;                    ///< 显示名
    std::string stationCode;             ///< 工站编码
    bool enabled;                        ///< 是否使能（未使能则运行态不 tick）
    JobSchedule jobSchedule;             ///< 流程调度方式
    std::vector<DeviceRef> boundDevices; ///< 绑定设备
    std::vector<JobCfg> jobs;         ///< 检测流程列表
    std::string commChannelId;           ///< 通讯通道
    int timeoutMs;                       ///< 超时毫秒
    FailPolicy failPolicy;               ///< 失败策略
    RuntimeView runtimeView;             ///< 运行主界面布局
    StationCfg()
        : enabled(true)
        , jobSchedule(ScheduleSequence)
        , timeoutMs(500)
        , failPolicy(FailStopStation)
    {}
};

}

#endif
