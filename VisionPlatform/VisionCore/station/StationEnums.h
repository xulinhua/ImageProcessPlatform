/**
 * @file StationEnums.h
 * @brief StationEnums头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STATION_ENUMS_H
#define VISION_STATION_ENUMS_H
#include "VisionCoreExport.h"
#include <string>

namespace Vision {

/** @brief 硬件设备种类（方案级台账顶层三类） */
enum DeviceKind {
    DeviceKindUndefined = -1, ///< 未定义
    DeviceCamera = 0,      ///< 相机（含文件源子类型）
    DeviceControlCard = 1, ///< 控制卡（含原 IO/光源迁移）
    DevicePlc = 2,         ///< PLC
    DeviceFolder = 3,      ///< 文件夹
    DeviceImage = 4,        ///< 图像，记录每张图的路径
    DeviceKindMax ///< 个数，不是合法值
};

/** @brief 相机后端：文件源或真相机 SDK */
enum CameraBackend {
    CameraBackendUndefined = -1, ///< 未定义
    CameraBackendFile = 0, ///< 文件夹仿真源
    CameraBackendSdk = 1   ///< 真相机 SDK（首版可为壳）
};

/** @brief 工站内多条流程实例（JobCfg）的执行编排 */
enum JobSchedule {
    ScheduleSequence = 0, ///< 串行
    ScheduleParallel      ///< 并行
};

/** @brief 运行态多工站画面壳布局（方案级） */
enum RuntimeStationLayout {
    RuntimeStationsTabs = 0,  ///< 分页签：一次看一个工站
    RuntimeStationsTiles = 1  ///< 一屏拼贴：各工站同时可见
};

VISIONCORE_API /** @brief 设备种类界面显示名 */
std::string deviceKindTitle(DeviceKind k);
VISIONCORE_API /** @brief 相机后端显示名 */
std::string cameraBackendTitle(CameraBackend b);
VISIONCORE_API /**
 * @brief 旧方案 kind 整型迁移到新三类（format&lt;2）
 * @param legacyKind 旧枚举整型（含 File/Io/Light）
 * @param outKind 输出种类
 * @param outBackend 输出相机后端（非相机时忽略）
 */
void migrateLegacyDeviceKind(int legacyKind, DeviceKind* outKind, CameraBackend* outBackend);
VISIONCORE_API /** @brief 工站流程执行方式的界面显示名 */
std::string scheduleTitle(JobSchedule s);
VISIONCORE_API /** @brief 整数转工站编排；越界回串行 */
JobSchedule jobScheduleFromInt(int v);
VISIONCORE_API /** @brief 多工站画面布局显示名 */
std::string runtimeStationLayoutTitle(RuntimeStationLayout layout);
VISIONCORE_API /** @brief 整数转多工站布局；越界回分页签 */
RuntimeStationLayout runtimeStationLayoutFromInt(int v);

}

#endif
