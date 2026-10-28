/**
 * @file VisionLogRunSink.h
 * @brief VisionLogRunSink头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_VISIONLOGRUNSINK_H
#define VISION_VISIONLOGRUNSINK_H
#include "VisionUiExport.h"

#include "RunListener.h"
#include <string>

namespace Vision {

/**
 * @class LogMgrRunSink
 * @brief 把运行日志自动写入 VisionLog（debug 类）；不拥有会话，身份由外部刷新
 */
class VISION_UI_API VisionLogRunSink : public IRunListener {
public:
    /** @brief 空身份，落盘门槛为 Info */
    VisionLogRunSink();

    /**
     * @brief 刷新落盘身份（登录/注销时调用）
     * @param user 登录名
     * @param role 角色键
     */
    void setIdentity(const std::string& user, const std::string& role);
    /**
     * @brief 最低落盘级别（默认 Info，Debug 不刷盘）
     * @param minLevel 含该级及以上
     */
    void setMinLevel(RunLogLevel minLevel);

    /** @brief 追加一条 debug 日志 */
    void onRunLog(const RunLogEvent& ev);

private:
    std::string m_user;       ///< 登录名
    std::string m_role;       ///< 角色
    RunLogLevel m_minLevel;   ///< 落盘门槛
};

}

#endif
