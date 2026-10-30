/**
 * @file Markers.cpp
 * @brief Markers实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsCross.h>
#include <HsCommData/HsInterface2D.h>
#include <HsCommData/HsArrow.h>
#include <HsCommData/HsText.h>

#include <cmath>

namespace HsCommData {

namespace {

const double kHeadOpen = 0.4; ///< 默认张角（弧度），半宽未给定时用

}

// 十字默认半宽 10、角度 0
HsCross::HsCross()
    : size(10.0)
    , angle(0.0)
{
}
// 内部转角，弧度
double HsCross::angRad() const
{
    return angle;
}

// 写入内部弧度
void HsCross::setAngRad(double rad)
{
    angle = rad;
}

// 转角的度数，正方向与内部相同
double HsCross::angDeg() const
{
    return toDegree(angle);
}

// 按度数写入，正方向与内部相同
void HsCross::setAngDeg(double deg)
{
    angle = toRadian(deg);
}

// 按系统角度单位和正方向读取
double HsCross::ang() const
{
    return fromAngle(angle);
}

// 按系统角度单位和正方向写入
void HsCross::setAng(double external)
{
    angle = toAngle(external);
}


// 箭头默认按箭身比例
HsArrow::HsArrow()
    : headLen(-1.0)
    , headWidth(-1.0)
{
}

// 由箭身方向和箭头尺寸算出两翼
bool HsArrow::wings(Point2d* left, Point2d* right) const
{
    if (!left || !right) {
        return false;
    }
    const double dx = head.x - tail.x;
    const double dy = head.y - tail.y;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len <= 1e-3) {
        return false;
    }
    double hl = headLen;
    if (hl < 0.0) {
        hl = len * 0.25;
        if (hl > 16.0) {
            hl = 16.0;
        }
    }
    if (hl <= 1e-3) {
        return false;
    }
    double hw = headWidth;
    if (hw < 0.0) {
        hw = hl * std::tan(kHeadOpen);
    }
    const double ux = dx / len;
    const double uy = dy / len;
    const double bx = head.x - ux * hl;
    const double by = head.y - uy * hl;
    left->x = bx - uy * hw;
    left->y = by + ux * hw;
    right->x = bx + uy * hw;
    right->y = by - ux * hw;
    return true;
}

// 空正文
HsText::HsText()
{
}

}