/**
 * @file Color.cpp
 * @brief Color实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/Color.h>

namespace HsCommData {

// 全 0 表示未设置
Color4::Color4()
    : r(0)
    , g(0)
    , b(0)
    , a(0)
{
}

// 指定各通道，alpha 缺省不透明
Color4::Color4(unsigned char r_, unsigned char g_, unsigned char b_, unsigned char a_)
    : r(r_)
    , g(g_)
    , b(b_)
    , a(a_)
{
}

// 全通道为 0 即未设置
bool Color4::isUnset() const
{
    return r == 0 && g == 0 && b == 0 && a == 0;
}

}
