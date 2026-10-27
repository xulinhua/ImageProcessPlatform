/**
 * @file DeviceControl.cpp
 * @brief DeviceControl实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeviceControl.h"
#include "AppPaths.h"
#include "Translate.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Vision {

namespace {

// 目录是否存在
bool dirExists(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
#ifdef _WIN32
    const DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    return false;
#endif
}

} // namespace

// 构造未打开的相机控制
CamDeviceControl::CamDeviceControl()
    : m_open(false)
{
}

// 枚举常用候选：samples 与各盘根目录
std::vector<DeviceProbeInfo> CamDeviceControl::enumerate()
{
    std::vector<DeviceProbeInfo> list;
    DeviceProbeInfo samples;
    samples.key = AppPaths::exeDir() + "/samples";
    samples.title = std::string(Translate::tr("示例图 samples"));
    samples.params.insert("folder", samples.key);
    list.push_back(samples);
#ifdef _WIN32
    char drives[256] = { 0 };
    if (GetLogicalDriveStringsA(sizeof(drives) - 1, drives) > 0) {
        for (char* p = drives; *p; p += strlen(p) + 1) {
            DeviceProbeInfo info;
            info.key = p;
            // 去掉末尾反斜杠显示
            std::string title = p;
            if (!title.empty() && (title[title.size() - 1] == '\\' || title[title.size() - 1] == '/')) {
                title.resize(title.size() - 1);
            }
            info.title = std::string(Translate::tr("磁盘 ")) + title;
            info.params.insert("folder", info.key);
            list.push_back(info);
        }
    }
#endif
    return list;
}

// 打开：文件源校验目录；SDK 暂拒绝
bool CamDeviceControl::open(const HardwareDevice& device)
{
    m_open = false;
    m_folder.clear();
    m_error.clear();
    if (device.kind != DeviceCamera) {
        m_error = std::string(Translate::tr("不是相机设备"));
        return false;
    }
    m_error = std::string(Translate::tr("真相机 SDK 未接入"));
    return false;
}

// 关闭并清掉当前文件源目录
void CamDeviceControl::close()
{
    m_open = false;
    m_folder.clear();
}

// 是否已打开
bool CamDeviceControl::isOpen() const
{
    return m_open;
}

// 最近一次失败原因
std::string CamDeviceControl::lastError() const
{
    return m_error;
}

// 构造未打开的控制卡控制
ControlCardDeviceControl::ControlCardDeviceControl()
    : m_open(false)
{
}

// 首版无真实枚举，返回占位说明项（不自动登记）
std::vector<DeviceProbeInfo> ControlCardDeviceControl::enumerate()
{
    std::vector<DeviceProbeInfo> list;
    DeviceProbeInfo tip;
    tip.key = "stub.controlcard";
    tip.title = std::string(Translate::tr("（暂无已发现控制卡，可手动登记）"));
    tip.params.insert("vendor", std::string(""));
    tip.params.insert("address", std::string(""));
    list.push_back(tip);
    return list;
}

// 校验种类后标记打开（驱动未接入）
bool ControlCardDeviceControl::open(const HardwareDevice& device)
{
    m_open = false;
    m_error.clear();
    if (device.kind != DeviceControlCard) {
        m_error = std::string(Translate::tr("不是控制卡设备"));
        return false;
    }
    // 首版：仅标记打开，真实驱动后续接入
    m_open = true;
    return true;
}

// 关闭控制卡
void ControlCardDeviceControl::close()
{
    m_open = false;
}

// 是否已打开
bool ControlCardDeviceControl::isOpen() const
{
    return m_open;
}

// 最近一次失败原因
std::string ControlCardDeviceControl::lastError() const
{
    return m_error;
}

// 构造未打开的 PLC 控制
PlcDeviceControl::PlcDeviceControl()
    : m_open(false)
{
}

// 返回占位项，提示先配置通信通道
std::vector<DeviceProbeInfo> PlcDeviceControl::enumerate()
{
    std::vector<DeviceProbeInfo> list;
    DeviceProbeInfo tip;
    tip.key = "stub.plc";
    tip.title = std::string(Translate::tr("（请先在通信管理配置 MODBUS 通道后登记 PLC）"));
    tip.params.insert("commChannelId", std::string(""));
    tip.params.insert("slaveId", 1);
    list.push_back(tip);
    return list;
}

// 校验种类，并要求已绑定通信通道
bool PlcDeviceControl::open(const HardwareDevice& device)
{
    m_open = false;
    m_error.clear();
    if (device.kind != DevicePlc) {
        m_error = std::string(Translate::tr("不是 PLC 设备"));
        return false;
    }
    if (channelIdOf(device).empty()) {
        m_error = std::string(Translate::tr("未绑定通信通道"));
        return false;
    }
    m_open = true;
    return true;
}

// 关闭 PLC
void PlcDeviceControl::close()
{
    m_open = false;
}

// 是否已打开
bool PlcDeviceControl::isOpen() const
{
    return m_open;
}

// 最近一次失败原因
std::string PlcDeviceControl::lastError() const
{
    return m_error;
}

// 读设备绑定的通信通道 id
std::string PlcDeviceControl::channelIdOf(const HardwareDevice& device)
{
    return device.defaultParams.value("commChannelId").toString();
}

}
