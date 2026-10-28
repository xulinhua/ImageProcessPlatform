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
        std::string action;         ///< 动作
        int address;                ///< 起始地址
        std::string values;         ///< 写值(逗号分隔)
        std::string expect;         ///< 期望值(逗号分隔)
        int timeoutMs;              ///< 超时ms
        int pollMs;                 ///< 轮询ms
        double scale;               ///< 位姿比例
        int slaveId;                ///< 从站号
        std::string peerId;         ///< 对端id
        std::string commChannelId;  ///< 通道id
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
