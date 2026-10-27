/**
 * @file CameraFileTool.cpp
 * @brief CameraFileTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CameraFileTool.h"
#include "FileGrabber.h"

namespace Vision {

// 路径、采集方式、像素格式占位
ParamMap CameraFileTool::defaultParams() const
{
    ParamMap p;
    p.insert("path", std::string());
    p.insert("sourceMode", std::string("folder"));
    p.insert("acquire", std::string("trigger"));
    p.insert("pixelFormat", std::string("mono8"));
    return p;
}

// 从绑定设备/工具路径抓下一张图（index/lastFile 为运行态，不标方案 dirty）
ToolOut CameraFileTool::execute(ToolContext& ctx)
{
    ToolOut out;
    if (!ctx.sol || !ctx.tool) {
        out.message = std::string(Translate::tr("上下文无效"));
        return out;
    }
    HardwareDevice* dev = ctx.sol->findDevice(ctx.tool->deviceId);
    std::string err;
    const int64_t fid = ctx.frameSeq ? ++(*ctx.frameSeq) : 1;
    const HsCommData::HsImage fb = FileGrabber::grab(ctx.tool, dev, fid, &err);
    if (fb.isNull()) {
        out.message = err;
        return out;
    }
    out.ok = true;
    out.image = fb;
    out.frameId = fb.frameId;
    out.message = ctx.tool->runtime.value("lastFile").toString();
    return out;
}

}
