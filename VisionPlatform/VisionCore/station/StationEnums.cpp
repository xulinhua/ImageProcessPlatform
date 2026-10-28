/**
 * @file StationEnums.cpp
 * @brief StationEnums实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StationEnums.h"
#include "CommTypes.h"
#include "Translate.h"

namespace Vision {

// 设备种类界面显示名
std::string deviceKindTitle(DeviceKind k)
{
    switch (k) {
    case DeviceCamera: return Translate::tr("相机");
    case DeviceControlCard: return Translate::tr("控制卡");
    case DevicePlc: return Translate::tr("PLC");
    case DeviceFolder: return Translate::tr("文件夹");
    case DeviceImage: return Translate::tr("图像");
    default: return Translate::tr("相机");
    }
}

// 相机后端显示名
std::string cameraBackendTitle(CameraBackend b)
{
    switch (b) {
    case CameraBackendFile: return Translate::tr("文件源");
    case CameraBackendSdk: return Translate::tr("真相机");
    default: return Translate::tr("文件源");
    }
}

// 旧 kind：0相机 1IO 2光源 3PLC 4文件 → 新三类
void migrateLegacyDeviceKind(int legacyKind, DeviceKind* outKind, CameraBackend* outBackend)
{
    if (!outKind || !outBackend) {
        return;
    }
    *outBackend = CameraBackendFile;
    switch (legacyKind) {
    case 0: // DeviceCamera
        *outKind = DeviceCamera;
        *outBackend = CameraBackendSdk;
        break;
    case 1: // DeviceIo
    case 2: // DeviceLight
        *outKind = DeviceControlCard;
        break;
    case 3: // DevicePlc
        *outKind = DevicePlc;
        break;
    case 4: // DeviceFile
        *outKind = DeviceFolder;
        *outBackend = CameraBackendFile;
        break;
    default:
        *outKind = DeviceFolder;
        *outBackend = CameraBackendFile;
        break;
    }
}

// 工站 JobCfg 编排显示名
std::string scheduleTitle(JobSchedule s)
{
    switch (s) {
    case ScheduleParallel: return Translate::tr("并行");
    case ScheduleSequence: return Translate::tr("串行");
    default:               return Translate::tr("串行");
    }
}

// 整数转工站编排
JobSchedule jobScheduleFromInt(int v)
{
    if (v == ScheduleParallel) {
        return ScheduleParallel;
    }
    return ScheduleSequence;
}

// 多工站画面布局显示名
std::string runtimeStationLayoutTitle(RuntimeStationLayout layout)
{
    switch (layout) {
    case RuntimeStationsTiles: return Translate::tr("一屏拼贴");
    case RuntimeStationsTabs:  return Translate::tr("分页签");
    default:                   return Translate::tr("分页签");
    }
}

// 整数转多工站布局
RuntimeStationLayout runtimeStationLayoutFromInt(int v)
{
    if (v == RuntimeStationsTiles) {
        return RuntimeStationsTiles;
    }
    return RuntimeStationsTabs;
}

// 通道类型显示名
std::string commChannelTypeTitle(CommChannelType t)
{
    switch (t) {
    case CommSerial: return Translate::tr("串口");
    case CommTcpClient: return Translate::tr("TCP客户端");
    case CommTcpServer: return Translate::tr("TCP服务端");
    case CommUdp: return Translate::tr("UDP");
    default: return Translate::tr("串口");
    }
}
}
