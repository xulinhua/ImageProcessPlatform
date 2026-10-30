/**
 * @file CommTool.cpp
 * @brief CommTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CommTool.h"
#include "CommMgr.h"
#include "TextEncoding.h"
#include "VisionVersion.h"
#include <cstdio>
#include <sstream>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
// 当前版本。必须等于变更表最后一行
const char kToolVersion[] = "1.0.0.0";
// 变更表。只追加，不删旧行
const Vision::VerNoteRaw kVerNotes[] = {
    { "1.0.0.0", "首版" },
};

// 注册名
const char kToolName[] = "comm.plc";
// 界面显示名
const char kDisplayName[] = "PLC通信";
// 工具说明
const char kDescription[] = "写保持寄存器、等到位、或把上游位姿写入寄存器";

// 逗号分隔整数解析为寄存器值
bool parseRegs(const std::string& s, std::vector<uint16_t>* out, std::string* err)
{
    out->clear();
    if (s.empty()) {
        return true;
    }
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ',')) {
        while (!part.empty() && (part[0] == ' ' || part[0] == '\t')) {
            part.erase(0, 1);
        }
        if (part.empty()) {
            continue;
        }
        int v = 0;
        if (sscanf(part.c_str(), "%d", &v) != 1) {
            if (err) {
                *err = "寄存器值格式错误";
            }
            return false;
        }
        if (v < 0) {
            v = 0;
        }
        if (v > 65535) {
            v = 65535;
        }
        out->push_back(static_cast<uint16_t>(v));
    }
    return true;
}

// 双精度按比例压成有符号 int16 再转 uint16
uint16_t scaleToReg(double v, double scale)
{
    double s = v * scale;
    if (s > 32767.0) {
        s = 32767.0;
    }
    if (s < -32768.0) {
        s = -32768.0;
    }
    const short iv = static_cast<short>(s >= 0 ? (s + 0.5) : (s - 0.5));
    return static_cast<uint16_t>(iv);
}

} // namespace

// 注册名
std::string Vision::CommTool::toolName() const
{
    return kToolName;
}

// 工具大类
Vision::ToolType Vision::CommTool::type() const
{
    return Vision::ToolType::Comm;
}

// 界面显示名
std::string Vision::CommTool::displayName() const
{
    return kDisplayName;
}

// 工具版本
std::string Vision::CommTool::version() const
{
    return std::string(kToolVersion);
}

// 版本变更，只追加
std::vector<Vision::VerNote> Vision::CommTool::versionNotes() const
{
    return Vision::verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}


// 工具说明
std::string Vision::CommTool::description() const
{
    return kDescription;
}

// 缺省参数
Vision::ParamMap Vision::CommTool::defaultParams() const
{
    Vision::ParamMap p;
    p.insert("action", std::string("writeRegs"));
    p.insert("address", 0);
    p.insert("values", std::string());
    p.insert("expect", std::string());
    p.insert("timeoutMs", 5000);
    p.insert("pollMs", 50);
    p.insert("scale", 1000.0);
    p.insert("slaveId", 1);
    p.insert("peerId", std::string());
    p.insert("commChannelId", std::string());
    return p;
}

// 参数说明
std::vector<Vision::ParamField> Vision::CommTool::paramFields() const
{
    std::vector<Vision::ParamField> f;
    f.push_back(Vision::paramDevice("deviceId", "PLC设备"));
    f.push_back(Vision::paramString("action", "动作", std::string("writeRegs"),
                                    Vision::paramChoices("writeRegs", "waitRegs", "writePose")));
    f.push_back(Vision::paramInt("address", "起始地址", 0, true, 0, true, 65535));
    f.push_back(Vision::paramString("values", "写值(逗号分隔)", std::string(), std::vector<std::string>()));
    f.push_back(Vision::paramString("expect", "期望值(逗号分隔)", std::string(), std::vector<std::string>()));
    f.push_back(Vision::paramInt("timeoutMs", "超时ms", 5000, true, 0, true, 600000));
    f.push_back(Vision::paramInt("pollMs", "轮询ms", 50, true, 1, true, 10000));
    f.push_back(Vision::paramDouble("scale", "位姿比例", 1000.0, true, 0.001, true, 1e6));
    f.push_back(Vision::paramInt("slaveId", "从站号", 1, true, 1, true, 247));
    f.push_back(Vision::paramString("peerId", "对端id", std::string(), std::vector<std::string>()));
    f.push_back(Vision::paramString("commChannelId", "通道id", std::string(), std::vector<std::string>()));
    return f;
}

// 写寄存器、等到位或写位姿
Vision::ToolOut Vision::CommTool::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    out.ok = false;
    out.image = in.input;
    if (!in.inputIo.logics.empty()) {
        out.io.logics = in.inputIo.logics;
    }

    // 先确认通道已绑定且已打开
    const std::string action = in.param.run.value("action").toString();
    const std::string chStr = in.param.run.value("commChannelId").toString();
    if (chStr.empty()) {
        out.error = true;
        out.message = "未绑定通信通道（请选 PLC 设备）";
        return out;
    }
    const Vision::ObjectId chId(chStr);
    const std::string peerId = in.param.run.value("peerId").toString();
    const int slave = in.param.run.value("slaveId").toInt(1);
    const int address = in.param.run.value("address").toInt(0);
    Vision::CommMgr& hub = Vision::CommMgr::instance();
    if (!hub.isOpen(chId)) {
        out.error = true;
        out.message = "通信通道未打开";
        return out;
    }

    std::string err;
    // 写位姿：取第一个上游位姿，x、y、角度按比例压成三个寄存器
    if (action == "writePose") {
        if (in.inputIo.poses.empty()) {
            out.error = true;
            out.message = "无有效上游位姿";
            return out;
        }
        const double scale = in.param.run.value("scale").toDouble(1000.0);
        const HsCommData::Pose2& pose = in.inputIo.poses[0].pose;
        const double angDeg = pose.angDeg();
        std::vector<uint16_t> regs;
        regs.push_back(scaleToReg(pose.tx, scale));
        regs.push_back(scaleToReg(pose.ty, scale));
        regs.push_back(scaleToReg(angDeg, scale));
        if (!hub.modbusWriteRegs(chId, peerId, slave, Vision::ModbusHolding, address, regs, &err)) {
            out.error = true;
            out.message = err.empty() ? "写位姿失败" : err;
            return out;
        }
        out.ok = true;
        out.values.insert("x", pose.tx);
        out.values.insert("y", pose.ty);
        out.values.insert("angle", pose.ang());
        out.message = "已写位姿到寄存器";
        return out;
    }

    // 写寄存器：把逗号分隔的值从起始地址连续写入
    if (action == "writeRegs") {
        std::vector<uint16_t> regs;
        if (!parseRegs(in.param.run.value("values").toString(), &regs, &err)) {
            out.error = true;
            out.message = err;
            return out;
        }
        if (regs.empty()) {
            out.error = true;
            out.message = "写值为空";
            return out;
        }
        if (!hub.modbusWriteRegs(chId, peerId, slave, Vision::ModbusHolding, address, regs, &err)) {
            out.error = true;
            out.message = err.empty() ? "写寄存器失败" : err;
            return out;
        }
        out.ok = true;
        out.message = "已写寄存器";
        return out;
    }

    // 等到位：轮询读寄存器，全部等于期望值即 OK，超时 NG
    if (action == "waitRegs") {
        std::vector<uint16_t> expect;
        if (!parseRegs(in.param.run.value("expect").toString(), &expect, &err)) {
            out.error = true;
            out.message = err;
            return out;
        }
        if (expect.empty()) {
            out.error = true;
            out.message = "期望值为空";
            return out;
        }
        const int timeoutMs = in.param.run.value("timeoutMs").toInt(5000);
        int pollMs = in.param.run.value("pollMs").toInt(50);
        if (pollMs < 1) {
            pollMs = 1;
        }
        const DWORD t0 = GetTickCount();
        for (;;) {
            std::vector<uint16_t> regs;
            if (!hub.modbusReadRegs(chId, peerId, slave, Vision::ModbusHolding, address,
                                    static_cast<int>(expect.size()), &regs, &err)) {
                out.error = true;
                out.message = err.empty() ? "读寄存器失败" : err;
                return out;
            }
            bool match = (regs.size() >= expect.size());
            for (size_t i = 0; match && i < expect.size(); ++i) {
                if (regs[i] != expect[i]) {
                    match = false;
                }
            }
            if (match) {
                out.ok = true;
                out.message = "寄存器已到位";
                return out;
            }
            const DWORD elapsed = GetTickCount() - t0;
            if (timeoutMs >= 0 && static_cast<int>(elapsed) >= timeoutMs) {
                out.ok = false;
                out.message = "等待寄存器超时";
                return out;
            }
            Sleep(static_cast<DWORD>(pollMs));
        }
    }

    out.error = true;
    out.message = "未知动作";
    return out;
}

// 克隆本实例
Vision::ToolBase* Vision::CommTool::clone() const
{
    CommTool* p = new CommTool();
    p->copyInstanceDataFrom(*this);
    return p;
}
// 返回本工具参数对象

const Vision::ToolParaBase* Vision::CommTool::toolParam() const
{
    return &m_param;
}
