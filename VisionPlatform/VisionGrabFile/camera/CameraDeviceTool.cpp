/**
 * @file CameraDeviceTool.cpp
 * @brief CameraDeviceTool实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CameraDeviceTool.h"
#include "FileGrabber.h"
#include "Translate.h"

namespace Vision {

// 曝光增益占位，真相机 SDK 接入后使用
ParamMap CameraDeviceTool::defaultParams() const
{
    ParamMap p;
    p.insert("acquire", std::string("trigger"));
    p.insert("exposureUs", 10000);
    p.insert("gainDb", 0.0);
    return p;
}

// 先走文件源设备；未接相机 SDK 时给出明确原因（index/lastFile 为运行态，不标方案 dirty）
ToolOut CameraDeviceTool::execute(ToolContext& ctx)
{
    ToolOut out;
    if (!ctx.sol || !ctx.tool) {
        out.message = std::string(Translate::tr("上下文无效"));
        return out;
    }
    HardwareDevice* dev = ctx.sol->findDevice(ctx.tool->deviceId);
    if (!dev) {
        out.message = std::string(Translate::tr("未绑定采集设备。请在工具参数里选择设备。"));
        return out;
    }
    if (dev->kind != DeviceCamera) {
        out.message = std::string(Translate::tr("绑定设备不是相机。"));
        return out;
    }
    std::string err;
    const int64_t fid = ctx.frameSeq ? ++(*ctx.frameSeq) : 1;
    if (dev->cameraBackend != CameraBackendFile) {
        out.message = std::string(Translate::tr("相机 SDK 未接入。请把该工具改绑文件源设备，或使用「图像文件/文件夹」。"));
        return out;
    }
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
