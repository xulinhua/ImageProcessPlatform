/**
 * @file CameraFileTool.h
 * @brief CameraFileTool头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CAMERAFILETOOL_H
#define VISION_CAMERAFILETOOL_H

#include "ToolBase.h"
#include "VisionVersion.h"

namespace Vision {

/**
 * @class CameraFileTool
 * @brief 图像文件或文件夹循环采集
 */
class CameraFileTool : public ToolBase {
public:
    /** @brief 注册名 */
    std::string toolName() const { return "camera.file"; }
    /** @brief 工具箱分组 */
    ToolType type() const { return ToolType::Acquire; }
    /** @brief 中文名 */
    std::string displayName() const { return std::string(Translate::tr("图像文件/文件夹")); }
    /** @brief 工具版本 */
    std::string version() const { return VISION_MODULE_GRABFILE_VERSION; }
    /** @brief 说明 */
    std::string description() const { return std::string(Translate::tr("从单个文件或目录循环读取图像，产出 out.image")); }
    /** @brief 路径、循环等缺省参数 */
    ParamMap defaultParams() const;
    /** @brief 调 FileGrabber::grab */
    ToolOut execute(ToolContext& ctx);
};

}

#endif
