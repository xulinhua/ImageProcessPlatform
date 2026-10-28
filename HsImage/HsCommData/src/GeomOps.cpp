/**
 * @file GeomOps.cpp
 * @brief GeomOps实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/GeomOps.h>
#include <HsCommData/HsInterface2D.h>

#include <algorithm>
#include <cmath>

namespace HsCommData {

// 向量相加
Point2d operator+(const Point2d& a, const Point2d& b)
{
    return Point2d(a.x + b.x, a.y + b.y);
}

// 向量相减
Point2d operator-(const Point2d& a, const Point2d& b)
{
    return Point2d(a.x - b.x, a.y - b.y);
}

// 数乘
Point2d operator*(const Point2d& a, double s)
{
    return Point2d(a.x * s, a.y * s);
}

// 原地相加
Point2d& operator+=(Point2d& a, const Point2d& b)
{
    a.x += b.x;
    a.y += b.y;
    return a;
}

// 点积
double dot2(const Point2d& a, const Point2d& b)
{
    return a.x * b.x + a.y * b.y;
}

// 两点距离
double dist2(const Point2d& a, const Point2d& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// 两点构造包围盒并规范宽高
Box2 fromPoints(const Point2d& a, const Point2d& b)
{
    return normalized(Box2(a.x, a.y, b.x - a.x, b.y - a.y));
}

// 左边界
double left(const Box2& box)
{
    return box.x;
}

// 上边界
double top(const Box2& box)
{
    return box.y;
}

// 右边界，开区间 x+w
double right(const Box2& box)
{
    return box.x + box.w;
}

// 下边界，开区间 y+h
double bottom(const Box2& box)
{
    return box.y + box.h;
}

// 宽
double width(const Box2& box)
{
    return box.w;
}

// 高
double height(const Box2& box)
{
    return box.h;
}

// 左上角
Point2d topLeft(const Box2& box)
{
    return Point2d(box.x, box.y);
}

// 中心
Point2d center(const Box2& box)
{
    return Point2d(box.x + box.w * 0.5, box.y + box.h * 0.5);
}

// 宽或高非正则空
bool isEmpty(const Box2& box)
{
    return box.w <= 0.0 || box.h <= 0.0;
}

// 宽高取非负
Box2 normalized(const Box2& box)
{
    Box2 r = box;
    if (r.w < 0.0) {
        r.x += r.w;
        r.w = -r.w;
    }
    if (r.h < 0.0) {
        r.y += r.h;
        r.h = -r.h;
    }
    return r;
}

// 平移，单位像素
void translate(Box2& box, double dx, double dy)
{
    box.x += dx;
    box.y += dy;
}

// 并集包围盒
Box2 united(const Box2& a, const Box2& b)
{
    if (isEmpty(a)) {
        return b;
    }
    if (isEmpty(b)) {
        return a;
    }
    const double l = (std::min)(left(a), left(b));
    const double t = (std::min)(top(a), top(b));
    const double rgt = (std::max)(right(a), right(b));
    const double btm = (std::max)(bottom(a), bottom(b));
    return Box2(l, t, rgt - l, btm - t);
}

// 用含端点的角点构造
IRect fromInclusive(int x0, int y0, int x1, int y1)
{
    if (x1 < x0) {
        const int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y1 < y0) {
        const int t = y0;
        y0 = y1;
        y1 = t;
    }
    return IRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}

// 左列
int left(const IRect& rect)
{
    return rect.x;
}

// 上行
int top(const IRect& rect)
{
    return rect.y;
}

// 右列，含端点
int right(const IRect& rect)
{
    return rect.x + rect.width - 1;
}

// 下行，含端点
int bottom(const IRect& rect)
{
    return rect.y + rect.height - 1;
}

// 宽或高非正则空
bool isEmpty(const IRect& rect)
{
    return rect.width <= 0 || rect.height <= 0;
}

// 平移，单位像素
void translate(IRect& rect, int dx, int dy)
{
    rect.x += dx;
    rect.y += dy;
}

// 单位位姿
Pose2::Pose2()
    : tx(0.0)
    , ty(0.0)
    , angle(0.0)
{
}

// 指定平移和转角
Pose2::Pose2(double tx_, double ty_, double angle_)
    : tx(tx_)
    , ty(ty_)
    , angle(angle_)
{
}

// 只旋转向量，不加平移
Point2d Pose2::rotate(const Point2d& v) const
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return Point2d(c * v.x - s * v.y, s * v.x + c * v.y);
}

// 局部点变到图像：先旋转再加平移
Point2d Pose2::apply(const Point2d& p) const
{
    const Point2d r = rotate(p);
    return Point2d(r.x + tx, r.y + ty);
}

// 逆位姿
Pose2 Pose2::inverse() const
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return Pose2(-(c * tx + s * ty), s * tx - c * ty, -angle);
}


// 内部转角，弧度
double Pose2::angRad() const
{
    return angle;
}

// 写入内部弧度
void Pose2::setAngRad(double rad)
{
    angle = rad;
}

// 转角的度数，正方向与内部相同
double Pose2::angDeg() const
{
    return toDegree(angle);
}

// 按度数写入，正方向与内部相同
void Pose2::setAngDeg(double deg)
{
    angle = toRadian(deg);
}

// 按系统角度单位和正方向读取
double Pose2::ang() const
{
    return fromAngle(angle);
}

// 按系统角度单位和正方向写入
void Pose2::setAng(double external)
{
    angle = toAngle(external);
}
}
