/**
 * @file FrameOverlay.h
 * @brief FrameOverlay头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_FRAMEOVERLAY_H
#define HSCOMMDATA_FRAMEOVERLAY_H

#include <HsCommData/HsCommDataExport.h>

#include <HsCommData/Graphic.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace HsCommData {

/**
 * @class FrameOverlay
 * @brief 一帧的结果叠加。frameId 须与当前底图一致，否则这一帧不画
 */
class HSCOMMDATA_EXPORT FrameOverlay {
public:
    int64_t frameId;            ///< 对应底图帧号
    std::vector<Graphic> items; ///< 本帧 Stack 图元

    FrameOverlay(); ///< frameId 为 0
    explicit FrameOverlay(int64_t frameId); ///< 指定帧号

    /** @brief 创建共享快照 */
    static std::shared_ptr<FrameOverlay> create(int64_t frameId);
};

}

#endif
