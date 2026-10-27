/**
 * @file Geom.h
 * @brief Geom头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_GEOM_H
#define HSCOMMDATA_GEOM_H

#include <HsCommData/HsCommDataExport.h>

#include <cstdint>

namespace HsCommData {

/**
 * @class Point_
 * @brief 图像平面上的点或矢量。x 为列，y 为行，向下为正
 * 平台几何用 Point2d（double）。整型、单精度只在像素下标或对接外部库时用。
 */
template<typename T>
struct Point_ {
    T x; ///< 列，向右为正
    T y; ///< 行，向下为正

    /** @brief 原点 */
    Point_() : x(0), y(0) {}
    /**
     * @brief 指定坐标
     * @param x_ 列
     * @param y_ 行
     */
    Point_(T x_, T y_) : x(x_), y(y_) {}
    /**
     * @brief 从另一种数值类型转换。显式，避免和 double 运算混用
     */
    template<typename U>
    explicit Point_(const Point_<U>& p)
        : x(static_cast<T>(p.x))
        , y(static_cast<T>(p.y))
    {
    }
};

typedef Point_<int> Point2i;       ///< 整型像素点
typedef Point_<float> Point2f;     ///< 单精度点
typedef Point_<double> Point2d;     ///< 平台使用的亚像素点

/**
 * @class Box2
 * @brief Axis-aligned float box. (x,y) top-left; w/h size; right = x+w
 */
struct HSCOMMDATA_EXPORT Box2 {
    double x; ///< left
    double y; ///< top
    double w; ///< width in pixels
    double h; ///< height in pixels

    Box2() : x(0.0), y(0.0), w(0.0), h(0.0) {} ///< empty
    Box2(double x_, double y_, double w_, double h_) : x(x_), y(y_), w(w_), h(h_) {}
};

/**
 * @class IRect
 * @brief Integer pixel rect. right/bottom are inclusive.
 */
struct HSCOMMDATA_EXPORT IRect {
    int x;      ///< left
    int y;      ///< top
    int width;  ///< width in pixels
    int height; ///< height in pixels

    IRect() : x(0), y(0), width(0), height(0) {} ///< empty
    IRect(int x_, int y_, int w_, int h_) : x(x_), y(y_), width(w_), height(h_) {}
};

/** @brief vector add */
HSCOMMDATA_EXPORT Point2d operator+(const Point2d& a, const Point2d& b);
/** @brief vector sub */
HSCOMMDATA_EXPORT Point2d operator-(const Point2d& a, const Point2d& b);
/** @brief scale */
HSCOMMDATA_EXPORT Point2d operator*(const Point2d& a, double s);
/** @brief in-place add */
HSCOMMDATA_EXPORT Point2d& operator+=(Point2d& a, const Point2d& b);
/** @brief dot product */
HSCOMMDATA_EXPORT double dot2(const Point2d& a, const Point2d& b);
/** @brief distance */
HSCOMMDATA_EXPORT double dist2(const Point2d& a, const Point2d& b);

/** @brief box from two points, normalized */
HSCOMMDATA_EXPORT Box2 fromPoints(const Point2d& a, const Point2d& b);
/** @brief left */
HSCOMMDATA_EXPORT double left(const Box2& box);
/** @brief top */
HSCOMMDATA_EXPORT double top(const Box2& box);
/** @brief right (open x+w) */
HSCOMMDATA_EXPORT double right(const Box2& box);
/** @brief bottom (open y+h) */
HSCOMMDATA_EXPORT double bottom(const Box2& box);
/** @brief width */
HSCOMMDATA_EXPORT double width(const Box2& box);
/** @brief height */
HSCOMMDATA_EXPORT double height(const Box2& box);
/** @brief top-left */
HSCOMMDATA_EXPORT Point2d topLeft(const Box2& box);
/** @brief center */
HSCOMMDATA_EXPORT Point2d center(const Box2& box);
/** @brief empty if w or h is non-positive */
HSCOMMDATA_EXPORT bool isEmpty(const Box2& box);
/** @brief ensure non-negative w/h */
HSCOMMDATA_EXPORT Box2 normalized(const Box2& box);
/** @brief translate in pixels */
HSCOMMDATA_EXPORT void translate(Box2& box, double dx, double dy);
/** @brief union bounding box */
HSCOMMDATA_EXPORT Box2 united(const Box2& a, const Box2& b);

/**
 * @brief build from inclusive corners
 * @param x0 left column
 * @param y0 top row
 * @param x1 right column (inclusive)
 * @param y1 bottom row (inclusive)
 */
HSCOMMDATA_EXPORT IRect fromInclusive(int x0, int y0, int x1, int y1);
/** @brief left column */
HSCOMMDATA_EXPORT int left(const IRect& rect);
/** @brief top row */
HSCOMMDATA_EXPORT int top(const IRect& rect);
/** @brief right column (inclusive) */
HSCOMMDATA_EXPORT int right(const IRect& rect);
/** @brief bottom row (inclusive) */
HSCOMMDATA_EXPORT int bottom(const IRect& rect);
/** @brief empty if width or height non-positive */
HSCOMMDATA_EXPORT bool isEmpty(const IRect& rect);
/** @brief translate in pixels */
HSCOMMDATA_EXPORT void translate(IRect& rect, int dx, int dy);

/**
 * @class Pose2
 * @brief 平面刚体位姿。平移加转角。图像坐标：x 为列，y 为行，向下为正。
 * 先旋转再平移：p = R * p_local + t。转角内部为弧度，从 +x 转向 +y。对外用 ang、angDeg。
 */
struct HSCOMMDATA_EXPORT Pose2 {
    double tx;    ///< 平移，列方向，像素
    double ty;    ///< 平移，行方向，像素
    double angle; ///< 内部转角，弧度。正方向从 +x 转向 +y。对外用 ang、angDeg

    /** @brief 单位位姿：平移 0，转角 0 */
    Pose2();
    /**
     * @brief 指定平移和转角
     * @param tx 列方向平移
     * @param ty 行方向平移
     * @param angle 内部转角，弧度。正方向从 +x 转向 +y。对外用 ang、angDeg
     */
    Pose2(double tx, double ty, double angle);

    /** @brief 只旋转向量，不加平移 */
    Point2d rotate(const Point2d& v) const;
    /** @brief 把局部点变到图像：先旋转再加平移 */
    Point2d apply(const Point2d& p) const;
    /** @brief 逆位姿 */
    Pose2 inverse() const;
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
    void setAng(double external);};

}

#endif
