/**
 * @file FrameEncoder.h
 * @brief 把一拍结果编成 JPEG，直接写入调用方给的缓冲
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_FRAME_ENCODER_H
#define VISION_FRAME_ENCODER_H
#include "VisionRunExport.h"

#include <HsCommData/HsImage.h>
#include <stdint.h>
#include <vector>

namespace Vision {

/**
 * @class FrameEncoder
 * @brief 显示图编码。槽里放的是压缩后的 JPEG，不是 HsImage 指针
 */
class VISION_RUN_API FrameEncoder {
public:
    /**
     * @brief 编码到 jpeg。质量 1 到 100
     * @return 空图或编不出时 jpeg 为空，仍返回 true，让状态可以发布
     */
    bool encode(const HsCommData::HsImage& image, int quality, std::vector<unsigned char>* jpeg) const;
};

}

#endif
