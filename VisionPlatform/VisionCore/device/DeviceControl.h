/**
 * @file DeviceControl.h
 * @brief DeviceControl头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DEVICE_CONTROL_H
#define VISION_DEVICE_CONTROL_H
#include "VisionCoreExport.h"

#include "HardwareDevice.h"
#include <string>
#include <vector>

namespace Vision {

/** @class DeviceProbeInfo @brief 遍历到的一条设备候选（尚未登记台账） */
struct DeviceProbeInfo {
    std::string key;   ///< 稳定键（如 COM3、文件夹路径、卡号）
    std::string title; ///< 显示名
    ParamMap params;   ///< 建议写入 defaultParams 的初值
};

/**
 * @class IDeviceControl
 * @brief 设备控制抽象：遍历与开闭（无 Qt）
 */
class VISIONCORE_API IDeviceControl {
public:
    virtual ~IDeviceControl() {}
    /** @brief 遍历本机/可用设备候选 */
    virtual std::vector<DeviceProbeInfo> enumerate() = 0;
    /** @brief 打开（校验参数）；失败写 lastError */
    virtual bool open(const HardwareDevice& device) = 0;
    /** @brief 关闭 */
    virtual void close() = 0;
    /** @brief 是否已打开 */
    virtual bool isOpen() const = 0;
    /** @brief 最近错误 */
    virtual std::string lastError() const = 0;
};

/** @class CamDeviceControl @brief 相机控制（文件源校验；SDK 占位） */
class VISIONCORE_API CamDeviceControl : public IDeviceControl {
public:
    /** @brief 构造未打开的相机控制 */
    CamDeviceControl();
    /** @brief 枚举 samples 与各盘根目录，作为文件源候选 */
    std::vector<DeviceProbeInfo> enumerate();
    /** @brief 打开文件源并校验目录；真相机暂拒绝 */
    bool open(const HardwareDevice& device);
    /** @brief 关闭并清掉当前文件源目录 */
    void close();
    /** @brief 是否已打开 */
    bool isOpen() const;
    /** @brief 最近一次失败原因 */
    std::string lastError() const;

private:
    bool m_open;           ///< 打开状态
    std::string m_error;   ///< 最近错误
    std::string m_folder;  ///< 当前文件源目录
};

/** @class ControlCardDeviceControl @brief 控制卡控制（首版壳） */
class VISIONCORE_API ControlCardDeviceControl : public IDeviceControl {
public:
    /** @brief 构造未打开的控制卡控制 */
    ControlCardDeviceControl();
    /** @brief 返回占位项，不自动登记控制卡 */
    std::vector<DeviceProbeInfo> enumerate();
    /** @brief 校验种类后标记打开（驱动未接入） */
    bool open(const HardwareDevice& device);
    /** @brief 关闭控制卡 */
    void close();
    /** @brief 是否已打开 */
    bool isOpen() const;
    /** @brief 最近一次失败原因 */
    std::string lastError() const;

private:
    bool m_open;         ///< 打开状态
    std::string m_error; ///< 最近错误
};

/** @class PlcDeviceControl @brief PLC 控制（参数校验；通路由 VisionComm 实测） */
class VISIONCORE_API PlcDeviceControl : public IDeviceControl {
public:
    /** @brief 构造未打开的 PLC 控制 */
    PlcDeviceControl();
    /** @brief 返回占位项，提示先配置通信通道 */
    std::vector<DeviceProbeInfo> enumerate();
    /** @brief 校验种类，并要求已绑定通信通道 */
    bool open(const HardwareDevice& device);
    /** @brief 关闭 PLC */
    void close();
    /** @brief 是否已打开 */
    bool isOpen() const;
    /** @brief 最近一次失败原因 */
    std::string lastError() const;
    /** @brief 绑定的通信通道 id（params["commChannelId"]） */
    static std::string channelIdOf(const HardwareDevice& device);

private:
    bool m_open;         ///< 打开状态
    std::string m_error; ///< 最近错误
};

}

#endif
