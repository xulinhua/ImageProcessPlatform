/**
 * @file Shape3.cpp
 * @brief Shape3实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/Shape3.h>
#include <HsCommData/HsConst.h>

#include <cmath>

namespace HsCommData {

namespace {

// 单位方向在各轴上的圆截面半径系数
double ringExtent(double axisComponent, double radius)
{
    const double rest = 1.0 - axisComponent * axisComponent;
    if (rest <= 0.0) {
        return 0.0;
    }
    return radius * std::sqrt(rest);
}

}

// 过原点、法向为 +Z
Plane3::Plane3()
    : origin()
    , normal(0.0, 0.0, 1.0)
{
}

// 指定平面上一点和法向
Plane3::Plane3(const Point3d& origin_, const Point3d& normal_)
    : origin(origin_)
    , normal(normal_)
{
}

// 法向长度是否接近 1
bool Plane3::isNormalUnit(double eps) const
{
    const double n = length(normal);
    const double d = n - 1.0;
    return d <= eps && d >= -eps;
}

// 有符号距离
double Plane3::signedDistance(const Point3d& p) const
{
    return dot(p - origin, normal);
}

// 空盒
Box3::Box3()
    : pose()
    , hx(-1.0)
    , hy(-1.0)
    , hz(-1.0)
{
}

// 指定中心位姿和三个半长
Box3::Box3(const Pose3& pose_, double hx_, double hy_, double hz_)
    : pose(pose_)
    , hx(hx_)
    , hy(hy_)
    , hz(hz_)
{
}

// 任一半长小于 0 则为空
bool Box3::isEmpty() const
{
    return hx < 0.0 || hy < 0.0 || hz < 0.0;
}

// 世界坐标轴对齐包围盒
Aabb3 Box3::bounds() const
{
    if (isEmpty()) {
        return Aabb3();
    }
    const double sx[2] = {-hx, hx};
    const double sy[2] = {-hy, hy};
    const double sz[2] = {-hz, hz};
    Aabb3 box;
    for (int ix = 0; ix < 2; ++ix) {
        for (int iy = 0; iy < 2; ++iy) {
            for (int iz = 0; iz < 2; ++iz) {
                box.include(pose.apply(Point3d(sx[ix], sy[iy], sz[iz])));
            }
        }
    }
    return box;
}

// 空球
Sphere3::Sphere3()
    : center()
    , radius(-1.0)
{
}

// 指定球心和半径
Sphere3::Sphere3(const Point3d& center_, double radius_)
    : center(center_)
    , radius(radius_)
{
}

// 半径小于 0 则为空
bool Sphere3::isEmpty() const
{
    return radius < 0.0;
}

// 轴对齐包围盒
Aabb3 Sphere3::bounds() const
{
    if (isEmpty()) {
        return Aabb3();
    }
    const Point3d r(radius, radius, radius);
    return Aabb3(center - r, center + r);
}

// 空圆柱
Cylinder3::Cylinder3()
    : center()
    , axis(0.0, 0.0, 1.0)
    , radius(-1.0)
    , halfHeight(0.0)
{
}

// 指定轴、半径和半高
Cylinder3::Cylinder3(const Point3d& center_, const Point3d& axis_, double radius_, double halfHeight_)
    : center(center_)
    , axis(axis_)
    , radius(radius_)
    , halfHeight(halfHeight_)
{
}

// 半径小于 0 则为空
bool Cylinder3::isEmpty() const
{
    return radius < 0.0;
}

// 半高小于 0 为无限圆柱
bool Cylinder3::isInfinite() const
{
    return !isEmpty() && halfHeight < 0.0;
}

// 有限圆柱的轴对齐包围盒
Aabb3 Cylinder3::bounds() const
{
    if (isEmpty() || isInfinite()) {
        return Aabb3();
    }
    const double len2 = lengthSquared(axis);
    if (len2 < kZeroLen2) {
        return Aabb3();
    }
    const double inv = 1.0 / std::sqrt(len2);
    const double ax = axis.x * inv;
    const double ay = axis.y * inv;
    const double az = axis.z * inv;
    const double ex = halfHeight * std::fabs(ax) + ringExtent(ax, radius);
    const double ey = halfHeight * std::fabs(ay) + ringExtent(ay, radius);
    const double ez = halfHeight * std::fabs(az) + ringExtent(az, radius);
    return Aabb3(
        Point3d(center.x - ex, center.y - ey, center.z - ez),
        Point3d(center.x + ex, center.y + ey, center.z + ez));
}

// 两端都在原点
Line3::Line3()
    : a()
    , b()
{
}

// 指定两端
Line3::Line3(const Point3d& a_, const Point3d& b_)
    : a(a_)
    , b(b_)
{
}

// 两端距离
double Line3::length() const
{
    return dist(a, b);
}

// 两端的轴对齐包围盒
Aabb3 Line3::bounds() const
{
    Aabb3 box = Aabb3::fromPoint(a);
    box.include(b);
    return box;
}

// 线段上的点
Point3d Line3::pointAt(double t) const
{
    return Point3d(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t);
}

// 空圆
Circle3::Circle3()
    : center()
    , normal(0.0, 0.0, 1.0)
    , radius(-1.0)
{
}

// 指定圆心、法向和半径
Circle3::Circle3(const Point3d& center_, const Point3d& normal_, double radius_)
    : center(center_)
    , normal(normal_)
    , radius(radius_)
{
}

// 半径小于 0 则为空
bool Circle3::isEmpty() const
{
    return radius < 0.0;
}

// 轴对齐包围盒
Aabb3 Circle3::bounds() const
{
    if (isEmpty()) {
        return Aabb3();
    }
    const double len2 = lengthSquared(normal);
    if (len2 < kZeroLen2) {
        return Aabb3();
    }
    const double inv = 1.0 / std::sqrt(len2);
    const double nx = normal.x * inv;
    const double ny = normal.y * inv;
    const double nz = normal.z * inv;
    const Point3d e(
        ringExtent(nx, radius),
        ringExtent(ny, radius),
        ringExtent(nz, radius));
    return Aabb3(center - e, center + e);
}

}
