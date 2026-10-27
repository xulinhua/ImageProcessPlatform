/**
 * @file RegisterGrabFile.cpp
 * @brief RegisterGrabFile实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CameraFileTool.h"
#include "CameraDeviceTool.h"

namespace Vision {

// 注册相机与文件/文件夹采集
void registerGrabFile()
{
    ToolRegistry::instance().add(new CameraDeviceTool());
    ToolRegistry::instance().add(new CameraFileTool());
}

}
