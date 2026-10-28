/**
 * @file AuditLog.cpp
 * @brief AuditLog实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AuditLog.h"
#include "LogMgr.h"

namespace Vision {

// 兼容旧调用：转到 VisionLog 操作日志
void auditLog(const std::string& event, const std::string& loginName, const std::string& detail)
{
    const char* result = (event == "login_fail") ? "fail" : "ok";
    LogMgr::appendOp(loginName, std::string(), event, std::string(), detail, std::string(result));
}

}
