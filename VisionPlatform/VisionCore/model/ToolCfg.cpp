/**
 * @file ToolCfg.cpp
 * @brief ToolCfg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolCfg.h"

namespace Vision {

// 赋值（不改引用绑定目标）
ToolCfg& ToolCfg::operator=(const ToolCfg& o)
{
        if (this == &o) {
            return *this;
        }
        info = o.info;
        links = o.links;
        param = o.param;
        runtime = o.runtime;
        canvasX = o.canvasX;
        canvasY = o.canvasY;
        canvasPlaced = o.canvasPlaced;
        return *this;
    }

}
