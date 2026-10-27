/**
 * @file HsCloud.cpp
 * @brief HsCloud实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsCloud.h>

namespace HsCommData {

// 黑色
Rgb8::Rgb8()
    : r(0)
    , g(0)
    , b(0)
{
}

// 指定三分量
Rgb8::Rgb8(unsigned char r_, unsigned char g_, unsigned char b_)
    : r(r_)
    , g(g_)
    , b(b_)
{
}

// 空点云
Cloud3::Cloud3()
    : width(0)
    , height(0)
{
}

// 同类型拷贝，坐标和属性整份复制
Cloud3::Cloud3(const Cloud3& other)
    : xyz(other.xyz)
    , normals(other.normals)
    , gray(other.gray)
    , rgb(other.rgb)
    , width(other.width)
    , height(other.height)
{
}

// 同类型赋值，坐标和属性整份复制
Cloud3& Cloud3::operator=(const Cloud3& other)
{
    if (this != &other) {
        xyz = other.xyz;
        normals = other.normals;
        gray = other.gray;
        rgb = other.rgb;
        width = other.width;
        height = other.height;
    }
    return *this;
}

// 没有点则为空
bool Cloud3::isEmpty() const
{
    return xyz.empty();
}

// 有序且宽高乘积等于点数
bool Cloud3::isOrganized() const
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    const long long cells = static_cast<long long>(width) * static_cast<long long>(height);
    return cells == static_cast<long long>(xyz.size());
}

// 可选数组与有序尺寸是否自洽
bool Cloud3::attribsOk() const
{
    if (!normals.empty() && normals.size() != xyz.size()) {
        return false;
    }
    if (!gray.empty() && gray.size() != xyz.size()) {
        return false;
    }
    if (!rgb.empty() && rgb.size() != xyz.size()) {
        return false;
    }
    if (width == 0 && height == 0) {
        return true;
    }
    return isOrganized();
}

// 点坐标的轴对齐包围盒
Aabb3 Cloud3::bounds() const
{
    if (xyz.empty()) {
        return Aabb3();
    }
    Aabb3 box = Aabb3::fromPoint(xyz[0]);
    for (size_t i = 1; i < xyz.size(); ++i) {
        box.include(xyz[i]);
    }
    return box;
}

}
