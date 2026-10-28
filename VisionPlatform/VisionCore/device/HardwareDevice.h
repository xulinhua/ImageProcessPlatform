/**
 * @file HardwareDevice.h
 * @brief HardwareDevice头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_HARDWARE_DEVICE_H
#define VISION_HARDWARE_DEVICE_H

#include "ObjectId.h"
#include "ParamValue.h"
#include "StationEnums.h"
#include <string>

namespace Vision {

/** @class DeviceRef @brief 工站绑定的设备引用 */
struct DeviceRef {
    ObjectId deviceId; ///< 设备 id
    bool enabled;      ///< 是否启用
    DeviceRef() : enabled(true) {}
};

/** @class HardwareDevice @brief 方案级硬件设备台账项 */
struct HardwareDevice {
    ObjectId id;                    ///< 设备 id
    std::string name;               ///< 显示名
    DeviceKind kind;                ///< 顶层种类：相机/控制卡/PLC
    CameraBackend cameraBackend;    ///< 仅相机有效：文件源或 SDK
    ParamMap defaultParams;         ///< 缺省参数
    HardwareDevice()
        : kind(DeviceKindUndefined)
        , cameraBackend(CameraBackendUndefined)
    {}
};

}

#endif
