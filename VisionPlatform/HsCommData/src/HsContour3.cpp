/**
 * @file HsContour3.cpp
 * @brief HsContour3实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsContour3.h>

namespace HsCommData {

// 默认开折线
Contour3::Contour3()
    : closed(false)
{
}

// 同类型拷贝，点列整份复制
Contour3::Contour3(const Contour3& other)
    : pts(other.pts)
    , closed(other.closed)
{
}

// 同类型赋值，点列整份复制
Contour3& Contour3::operator=(const Contour3& other)
{
    if (this != &other) {
        pts = other.pts;
        closed = other.closed;
    }
    return *this;
}

// 无顶点则为空
bool Contour3::isEmpty() const
{
    return pts.empty();
}

// 顶点包围盒
Aabb3 Contour3::bounds() const
{
    if (pts.empty()) {
        return Aabb3();
    }
    Aabb3 box = Aabb3::fromPoint(pts[0]);
    for (size_t i = 1; i < pts.size(); ++i) {
        box.include(pts[i]);
    }
    return box;
}

}
