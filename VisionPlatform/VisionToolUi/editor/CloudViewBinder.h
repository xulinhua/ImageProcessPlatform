/**
 * @file CloudViewBinder.h
 * @brief 把 ToolIo 三维数据灌进 HsViewShell
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CLOUDVIEWBINDER_H
#define VISION_CLOUDVIEWBINDER_H
#include "VisionToolUiExport.h"

namespace HsCtrl {
class HsViewShell;
}

namespace Vision {

struct ToolIo;

/**
 * @class CloudViewBinder
 * @brief 调试 / 设计 / 运行共用：点云、形状、位姿写入壳并可选切到 3D
 */
class VISION_TOOLUI_API CloudViewBinder {
public:
    /**
     * @brief 若 io 含点云或三维形状/位姿则灌入壳
     * @param switchTo3d 为真且有三维数据时切换到点云页
     * @return 写入了三维数据返回 true
     */
    static bool apply(HsCtrl::HsViewShell* shell, const ToolIo& io, bool switchTo3d);
};

}

#endif
