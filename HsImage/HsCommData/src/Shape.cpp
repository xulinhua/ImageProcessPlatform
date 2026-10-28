/**
 * @file Shape.cpp
 * @brief Shape实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/Shape.h>
#include <HsCommData/HsInterface2D.h>

namespace HsCommData {

// 内部转角，弧度
double EllipseP::angRad() const
{
    return phi;
}

// 写入内部弧度
void EllipseP::setAngRad(double rad)
{
    phi = rad;
}

// 转角的度数，正方向与内部相同
double EllipseP::angDeg() const
{
    return toDegree(phi);
}

// 按度数写入，正方向与内部相同
void EllipseP::setAngDeg(double deg)
{
    phi = toRadian(deg);
}

// 按系统角度单位和正方向读取
double EllipseP::ang() const
{
    return fromAngle(phi);
}

// 按系统角度单位和正方向写入
void EllipseP::setAng(double external)
{
    phi = toAngle(external);
}

// 内部转角，弧度
double Rect2P::angRad() const
{
    return phi;
}

// 写入内部弧度
void Rect2P::setAngRad(double rad)
{
    phi = rad;
}

// 转角的度数，正方向与内部相同
double Rect2P::angDeg() const
{
    return toDegree(phi);
}

// 按度数写入，正方向与内部相同
void Rect2P::setAngDeg(double deg)
{
    phi = toRadian(deg);
}

// 按系统角度单位和正方向读取
double Rect2P::ang() const
{
    return fromAngle(phi);
}

// 按系统角度单位和正方向写入
void Rect2P::setAng(double external)
{
    phi = toAngle(external);
}

}