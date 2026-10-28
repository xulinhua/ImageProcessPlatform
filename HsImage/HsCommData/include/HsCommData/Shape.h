/**
 * @file Shape.h
 * @brief Shape头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_SHAPE_H
#define HSCOMMDATA_SHAPE_H

#include <HsCommData/Geom.h>

namespace HsCommData {

/**
 * @class CircleP
 * @brief 圆：中心 + 半径（像素）
 */
struct HSCOMMDATA_EXPORT CircleP {
    Point2d c;     ///< 中心
    double r = 0; ///< 半径
};

/**
 * @class EllipseP
 * @brief 椭圆：中心、长短轴、转角
 */
struct HSCOMMDATA_EXPORT EllipseP {
    Point2d c;        ///< 中心
    double ra = 0;   ///< 半轴 a（像素）
    double rb = 0;   ///< 半轴 b（像素）
    double phi = 0;  ///< 内部转角，弧度。正方向从 +x 转向 +y。对外用 ang、angDeg
    /** @brief 内部转角，弧度 */
    double angRad() const;
    /** @brief 写入内部弧度 */
    void setAngRad(double rad);
    /** @brief 转角的度数，正方向与内部相同 */
    double angDeg() const;
    /** @brief 按度数写入，正方向与内部相同 */
    void setAngDeg(double deg);
    /** @brief 按系统角度单位和正方向读取 */
    double ang() const;
    /** @brief 按系统角度单位和正方向写入 */
    void setAng(double external);
};

/**
 * @class Rect1P
 * @brief 轴对齐矩形
 */
struct HSCOMMDATA_EXPORT Rect1P {
    Box2 r; ///< 图像坐标框
};

/**
 * @class Rect2P
 * @brief 旋转矩形：中心、半长、半宽、转角
 */
struct HSCOMMDATA_EXPORT Rect2P {
    Point2d c;       ///< 中心
    double l1 = 0;  ///< 半长（像素）
    double l2 = 0;  ///< 半宽（像素）
    double phi = 0; ///< 内部转角，弧度。正方向从 +x 转向 +y。对外用 ang、angDeg
    /** @brief 内部转角，弧度 */
    double angRad() const;
    /** @brief 写入内部弧度 */
    void setAngRad(double rad);
    /** @brief 转角的度数，正方向与内部相同 */
    double angDeg() const;
    /** @brief 按度数写入，正方向与内部相同 */
    void setAngDeg(double deg);
    /** @brief 按系统角度单位和正方向读取 */
    double ang() const;
    /** @brief 按系统角度单位和正方向写入 */
    void setAng(double external);
};

/**
 * @class LineP
 * @brief 线段两端点
 */
struct HSCOMMDATA_EXPORT LineP {
    Point2d a; ///< 起点
    Point2d b; ///< 终点
};

}

#endif
