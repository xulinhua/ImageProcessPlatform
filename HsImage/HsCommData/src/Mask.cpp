/**
 * @file Mask.cpp
 * @brief Mask实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/Mask.h>

namespace HsCommData {

// 空掩码
MaskP::MaskP()
    : width(0)
    , height(0)
{
}

// 同类型拷贝，像素整份复制
MaskP::MaskP(const MaskP& other)
    : width(other.width)
    , height(other.height)
    , pixels(other.pixels)
{
}

// 同类型赋值，像素整份复制
MaskP& MaskP::operator=(const MaskP& other)
{
    if (this != &other) {
        width = other.width;
        height = other.height;
        pixels = other.pixels;
    }
    return *this;
}

// 尺寸或缓冲非法则为空
bool MaskP::isEmpty() const
{
    if (width <= 0 || height <= 0) {
        return true;
    }
    const size_t n = static_cast<size_t>(width) * static_cast<size_t>(height);
    return pixels.size() < n;
}

}
