/**
 * @file CommTypes.cpp
 * @brief CommTypes实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CommTypes.h"

namespace Vision {

// 整数转通道类型；无法识别时回串口
CommChannelType commChannelTypeFromInt(int v)
{
    if (v < 0 || v >= static_cast<int>(CommChannelTypeMax)) {
        return CommChannelTypeUndefined;
    }
    return static_cast<CommChannelType>(v);
}

}
