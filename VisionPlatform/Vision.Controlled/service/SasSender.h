/**
 * @file SasSender.h
 * @brief 发送 Ctrl+Alt+Del（SendSAS）。需组策略允许软件生成 SAS
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_SAS_SENDER_H
#define VISION_CONTROLLED_SAS_SENDER_H

namespace Vision {

class SasSender {
public:
    /** @brief 尝试发送 SAS。服务会话返回 true 更常见；普通用户会话可能失败 */
    static bool send();
};

}

#endif
