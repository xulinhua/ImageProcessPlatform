/**
 * @file Geom3.cpp
 * @brief Geom3实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/Geom3.h>
#include <HsCommData/HsConst.h>

#include <cmath>

namespace HsCommData {

namespace {

// 四元数模长平方
double quatLen2(double qx, double qy, double qz, double qw)
{
    return qx * qx + qy * qy + qz * qz + qw * qw;
}

// 单位四元数旋转向量
Point3d rotateUnit(double qx, double qy, double qz, double qw, const Point3d& v)
{
    const double uvx = qy * v.z - qz * v.y;
    const double uvy = qz * v.x - qx * v.z;
    const double uvz = qx * v.y - qy * v.x;
    const double uuvx = qy * uvz - qz * uvy;
    const double uuvy = qz * uvx - qx * uvz;
    const double uuvz = qx * uvy - qy * uvx;
    return Point3d(
        v.x + 2.0 * (qw * uvx + uuvx),
        v.y + 2.0 * (qw * uvy + uuvy),
        v.z + 2.0 * (qw * uvz + uuvz));
}

}

// 向量相加
Point3d operator+(const Point3d& a, const Point3d& b)
{
    return Point3d(a.x + b.x, a.y + b.y, a.z + b.z);
}

// 向量相减
Point3d operator-(const Point3d& a, const Point3d& b)
{
    return Point3d(a.x - b.x, a.y - b.y, a.z - b.z);
}

// 取反
Point3d operator-(const Point3d& a)
{
    return Point3d(-a.x, -a.y, -a.z);
}

// 数乘
Point3d operator*(const Point3d& a, double s)
{
    return Point3d(a.x * s, a.y * s, a.z * s);
}

// 数除。除数接近 0 时返回零向量
Point3d operator/(const Point3d& a, double s)
{
    if (s > -1e-15 && s < 1e-15) {
        return Point3d();
    }
    return Point3d(a.x / s, a.y / s, a.z / s);
}

// 原地相加
Point3d& operator+=(Point3d& a, const Point3d& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

// 原地相减
Point3d& operator-=(Point3d& a, const Point3d& b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

// 点积
double dot(const Point3d& a, const Point3d& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// 叉积
Point3d cross(const Point3d& a, const Point3d& b)
{
    return Point3d(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

// 长度
double length(const Point3d& a)
{
    return std::sqrt(lengthSquared(a));
}

// 长度平方
double lengthSquared(const Point3d& a)
{
    return dot(a, a);
}

// 单位向量。长度接近 0 时返回零向量
Point3d normalized(const Point3d& a)
{
    const double len2 = lengthSquared(a);
    if (len2 < kZeroLen2) {
        return Point3d();
    }
    const double inv = 1.0 / std::sqrt(len2);
    return Point3d(a.x * inv, a.y * inv, a.z * inv);
}

// 两点距离
double dist(const Point3d& a, const Point3d& b)
{
    return length(a - b);
}

// 单位位姿
Pose3::Pose3()
    : tx(0.0)
    , ty(0.0)
    , tz(0.0)
    , qx(0.0)
    , qy(0.0)
    , qz(0.0)
    , qw(1.0)
{
}

// 指定平移和四元数，不在这里归一化
Pose3::Pose3(double tx_, double ty_, double tz_, double qx_, double qy_, double qz_, double qw_)
    : tx(tx_)
    , ty(ty_)
    , tz(tz_)
    , qx(qx_)
    , qy(qy_)
    , qz(qz_)
    , qw(qw_)
{
}

// 是否接近单位四元数
bool Pose3::isUnit(double eps) const
{
    const double n = std::sqrt(quatLen2(qx, qy, qz, qw));
    const double d = n - 1.0;
    return d <= eps && d >= -eps;
}

// 四元数归一化。模长接近 0 时退回单位元，平移保留
Pose3 Pose3::normalized() const
{
    const double len2 = quatLen2(qx, qy, qz, qw);
    Pose3 out(tx, ty, tz, 0.0, 0.0, 0.0, 1.0);
    if (len2 < kZeroLen2) {
        return out;
    }
    const double inv = 1.0 / std::sqrt(len2);
    out.qx = qx * inv;
    out.qy = qy * inv;
    out.qz = qz * inv;
    out.qw = qw * inv;
    return out;
}

// 只旋转向量，不加平移
Point3d Pose3::rotate(const Point3d& v) const
{
    const double len2 = quatLen2(qx, qy, qz, qw);
    if (len2 < kZeroLen2) {
        return v;
    }
    const double inv = 1.0 / std::sqrt(len2);
    return rotateUnit(qx * inv, qy * inv, qz * inv, qw * inv, v);
}

// 局部点变到世界：先旋转再加平移
Point3d Pose3::apply(const Point3d& p) const
{
    const Point3d r = rotate(p);
    return Point3d(r.x + tx, r.y + ty, r.z + tz);
}

// 逆位姿
Pose3 Pose3::inverse() const
{
    const Pose3 u = normalized();
    Pose3 inv(0.0, 0.0, 0.0, -u.qx, -u.qy, -u.qz, u.qw);
    const Point3d rt = inv.rotate(Point3d(u.tx, u.ty, u.tz));
    inv.tx = -rt.x;
    inv.ty = -rt.y;
    inv.tz = -rt.z;
    return inv;
}

// 空盒
Aabb3::Aabb3()
    : minCorner(0.0, 0.0, 0.0)
    , maxCorner(-1.0, -1.0, -1.0)
{
}

// 用最小角和最大角构造，不自动交换
Aabb3::Aabb3(const Point3d& minCorner_, const Point3d& maxCorner_)
    : minCorner(minCorner_)
    , maxCorner(maxCorner_)
{
}

// 任一维 max 小于 min 则为空
bool Aabb3::isEmpty() const
{
    return maxCorner.x < minCorner.x || maxCorner.y < minCorner.y || maxCorner.z < minCorner.z;
}

// 单点盒
Aabb3 Aabb3::fromPoint(const Point3d& p)
{
    return Aabb3(p, p);
}

// 并入一个点。空盒则变成该点
void Aabb3::include(const Point3d& p)
{
    if (isEmpty()) {
        minCorner = p;
        maxCorner = p;
        return;
    }
    if (p.x < minCorner.x) {
        minCorner.x = p.x;
    }
    if (p.y < minCorner.y) {
        minCorner.y = p.y;
    }
    if (p.z < minCorner.z) {
        minCorner.z = p.z;
    }
    if (p.x > maxCorner.x) {
        maxCorner.x = p.x;
    }
    if (p.y > maxCorner.y) {
        maxCorner.y = p.y;
    }
    if (p.z > maxCorner.z) {
        maxCorner.z = p.z;
    }
}

// 与另一盒求并。空盒不参与
Aabb3 Aabb3::united(const Aabb3& other) const
{
    if (isEmpty()) {
        return other;
    }
    if (other.isEmpty()) {
        return *this;
    }
    Aabb3 out = *this;
    out.include(other.minCorner);
    out.include(other.maxCorner);
    return out;
}

// 点是否在闭区间内。空盒恒为 false
bool Aabb3::contains(const Point3d& p) const
{
    if (isEmpty()) {
        return false;
    }
    return p.x >= minCorner.x && p.x <= maxCorner.x
        && p.y >= minCorner.y && p.y <= maxCorner.y
        && p.z >= minCorner.z && p.z <= maxCorner.z;
}

// 中心。空盒返回原点
Point3d Aabb3::center() const
{
    if (isEmpty()) {
        return Point3d();
    }
    return Point3d(
        (minCorner.x + maxCorner.x) * 0.5,
        (minCorner.y + maxCorner.y) * 0.5,
        (minCorner.z + maxCorner.z) * 0.5);
}

// 各边长度。空盒返回零向量
Point3d Aabb3::extent() const
{
    if (isEmpty()) {
        return Point3d();
    }
    return Point3d(maxCorner.x - minCorner.x, maxCorner.y - minCorner.y, maxCorner.z - minCorner.z);
}

}
