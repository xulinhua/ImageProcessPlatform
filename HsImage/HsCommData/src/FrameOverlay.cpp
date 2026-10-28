/**
 * @file FrameOverlay.cpp
 * @brief FrameOverlay实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/FrameOverlay.h>

namespace HsCommData {

// 空叠加，帧号为 0
FrameOverlay::FrameOverlay()
    : frameId(0)
{
}

// 指定帧号
FrameOverlay::FrameOverlay(int64_t id)
    : frameId(id)
{
}

// 创建共享快照
std::shared_ptr<FrameOverlay> FrameOverlay::create(int64_t frameId)
{
    return std::shared_ptr<FrameOverlay>(new FrameOverlay(frameId));
}

}
