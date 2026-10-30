/**
 * @file ToolParaComm.hpp
 * @brief ToolParaComm头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARACOMM_H
#define VISION_TOOLPARACOMM_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class ToolParaComm
 * @brief CommTool 运行参数
 */
class ToolParaComm : public ToolParaBase {
public:
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string action;         ///< 动作：writeRegs 写寄存器 / waitRegs 等到位 / writePose 写上游位姿
        int address;                ///< 保持寄存器起始地址，0~65535
        std::string values;         ///< writeRegs 要写的值，逗号分隔，每个钳到 0~65535
        std::string expect;         ///< waitRegs 期望值，逗号分隔，从 address 起逐个比对
        int timeoutMs;              ///< waitRegs 超时（ms），负数不限时
        int pollMs;                 ///< waitRegs 轮询间隔（ms），小于 1 按 1
        double scale;               ///< writePose 比例：x、y、角度（度）乘以它后取整成 int16
        int slaveId;                ///< MODBUS 从站号，1~247
        std::string peerId;         ///< 通道上的对端 id，可空
        std::string commChannelId;  ///< 通信通道 id；空时 Job 从绑定的 PLC 设备补
        /** @brief 缺省值与 CommTool::defaultParams 对齐，action 除外（这里为空） */
        Run()
            : action("")
            , address(0)
            , values("")
            , expect("")
            , timeoutMs(5000)
            , pollMs(50)
            , scale(1000.0)
            , slaveId(1)
            , peerId("")
            , commChannelId("")
        {}
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.action = m.value("action").toString();
        run.address = m.value("address").toInt(0);
        run.values = m.value("values").toString();
        run.expect = m.value("expect").toString();
        run.timeoutMs = m.value("timeoutMs").toInt(5000);
        run.pollMs = m.value("pollMs").toInt(50);
        run.scale = m.value("scale").toDouble(1000.0);
        run.slaveId = m.value("slaveId").toInt(1);
        run.peerId = m.value("peerId").toString();
        run.commChannelId = m.value("commChannelId").toString();
        judge = src.judge;
    }
};

}

#endif
