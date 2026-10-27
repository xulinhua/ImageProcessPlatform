/**
 * @file FileGrabber.h
 * @brief FileGrabber头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_FILEGRABBER_H
#define VISION_FILEGRABBER_H
#include "VisionGrabExport.h"

#include "ParamValue.h"
#include <HsCommData/HsImage.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class FileGrabber
 * @brief 从文件/目录读图（stb_image）。无 Qt、无相机 SDK、不含方案类型
 */
class VISION_GRAB_API FileGrabber {
public:
    /** @brief 目录下的图像文件列表；path 为文件则只含它自己 */
    static std::vector<std::string> imageFiles(const std::string& path);
    /**
     * @brief 读一帧。sourceMode 为 file 时固定该文件，否则按目录循环
     * @param runtime 进出：Index、GrabPath、LastFile
     * @param err 失败原因
     */
    static HsCommData::HsImage grab(const std::string& path, const std::string& sourceMode,
                                    ParamMap* runtime, int64_t frameId, std::string* err);
};

}

#endif
