/**
 * @file InvalidateScope.h
 * @brief InvalidateScope头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_INVALIDATESCOPE_H
#define HSIMAGE_INVALIDATESCOPE_H

#include <cstdint>

namespace HsCtrl {

/**
 * @brief 刷新范围。按位组合，告诉窗口这次要重画哪些层。
 */
enum InvalidateScope {
    InvalidateNone     = 0,       ///< 无
    InvalidateImage    = 1u << 0, ///< 底图
    InvalidateOverlay  = 1u << 1, ///< Stack
    InvalidateDrawing  = 1u << 2, ///< Drawing
    InvalidateViewport = 1u << 3  ///< 视口变换
};

/** @brief 按位或 */
inline InvalidateScope operator|(InvalidateScope a, InvalidateScope b)
{
    return static_cast<InvalidateScope>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

/** @brief 按位与 */
inline InvalidateScope operator&(InvalidateScope a, InvalidateScope b)
{
    return static_cast<InvalidateScope>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/** @brief 按位或并写回 */
inline InvalidateScope& operator|=(InvalidateScope& a, InvalidateScope b)
{
    a = a | b;
    return a;
}

}

#endif
