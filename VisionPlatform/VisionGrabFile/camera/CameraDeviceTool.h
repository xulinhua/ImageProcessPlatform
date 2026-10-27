/**
 * @file CameraDeviceTool.h
 * @brief CameraDeviceTool头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CAMERADEVICETOOL_H
#define VISION_CAMERADEVICETOOL_H

#include "ToolBase.h"
#include "VisionVersion.h"

namespace Vision {

/**
 * @class CameraDeviceTool
 * @brief 相机采集。真相机 SDK 以后走 IGrabber；现绑定文件源设备仍可出图
 */
class CameraDeviceTool : public ToolBase {
public:
    /** @brief 注册名 */
    std::string toolName() const { return "camera.device"; }
    /** @brief 采集类 */
    ToolType type() const { return ToolType::Acquire; }
    /** @brief 中文名 */
    std::string displayName() const { return std::string(Translate::tr("相机")); }
    /** @brief 工具版本 */
    std::string version() const { return VISION_MODULE_GRABFILE_VERSION; }
    /** @brief 说明 */
    std::string description() const { return std::string(Translate::tr("引用工站绑定的相机或文件源设备，产出 out.image")); }
    /** @brief 缺省参数 */
    ParamMap defaultParams() const;
    /** @brief 按绑定设备取流 */
    ToolOut execute(ToolContext& ctx);
};

}

#endif
