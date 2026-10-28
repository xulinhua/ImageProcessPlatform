/**
 * @file AuditLog.h
 * @brief AuditLog头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_AUDITLOG_H
#define VISION_AUDITLOG_H
#include "VisionAuthExport.h"

#include <string>

namespace Vision {

/**
 * @brief 追加一条审计（登录、改密、改权限等）。写 logs 目录文本，不弹窗
 * @param event 事件名
 * @param loginName 当事用户
 * @param detail 补充说明
 */
VISION_AUTH_API void auditLog(const std::string& event, const std::string& loginName, const std::string& detail = std::string());

}

#endif
