/**
 * @file Geom3.h
 * @brief Geom3头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_GEOM3_H
#define HSCOMMDATA_GEOM3_H

#include <HsCommData/HsCommDataExport.h>

namespace HsCommData {

/**
 * @class Point3_
 * @brief 三维点或矢量。右手系，单位毫米。不是图像坐标，Y 不向下
 * 平台几何用 Point3d（double）。整型、单精度只在对接外部库时用。
 */
template<typename T>
struct Point3_ {
    T x; ///< X，向右为正
    T y; ///< Y，向上为正（右手系，不是图像行）
    T z; ///< Z，朝向观察者一侧为正

    /** @brief 原点 */
    Point3_() : x(0), y(0), z(0) {}
    /**
     * @brief 指定坐标
     * @param x_ X
     * @param y_ Y
     * @param z_ Z
     */
    Point3_(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {}
    /**
     * @brief 从另一种数值类型转换。显式，避免和 double 运算混用
     */
    template<typename U>
    explicit Point3_(const Point3_<U>& p)
        : x(static_cast<T>(p.x))
        , y(static_cast<T>(p.y))
        , z(static_cast<T>(p.z))
    {
    }
};

typedef Point3_<int> Point3i;      ///< 整型三维点
typedef Point3_<float> Point3f;    ///< 单精度三维点
typedef Point3_<double> Point3d;    ///< 平台使用的三维点
/** @brief 向量加法 */
HSCOMMDATA_EXPORT Point3d operator+(const Point3d& a, const Point3d& b);
/** @brief 向量减法 */
HSCOMMDATA_EXPORT Point3d operator-(const Point3d& a, const Point3d& b);
/** @brief 取反 */
HSCOMMDATA_EXPORT Point3d operator-(const Point3d& a);
/** @brief 数乘 */
HSCOMMDATA_EXPORT Point3d operator*(const Point3d& a, double s);
/** @brief 数除。除数接近 0 时返回零向量 */
HSCOMMDATA_EXPORT Point3d operator/(const Point3d& a, double s);
/** @brief 原地加法 */
HSCOMMDATA_EXPORT Point3d& operator+=(Point3d& a, const Point3d& b);
/** @brief 原地减法 */
HSCOMMDATA_EXPORT Point3d& operator-=(Point3d& a, const Point3d& b);
/** @brief 点积 */
HSCOMMDATA_EXPORT double dot(const Point3d& a, const Point3d& b);
/** @brief 叉积 */
HSCOMMDATA_EXPORT Point3d cross(const Point3d& a, const Point3d& b);
/** @brief 长度 */
HSCOMMDATA_EXPORT double length(const Point3d& a);
/** @brief 长度平方 */
HSCOMMDATA_EXPORT double lengthSquared(const Point3d& a);
/** @brief 单位向量。长度接近 0 时返回零向量 */
HSCOMMDATA_EXPORT Point3d normalized(const Point3d& a);
/** @brief 两点距离 */
HSCOMMDATA_EXPORT double dist(const Point3d& a, const Point3d& b);

/**
 * @class Pose3
 * @brief 刚体位姿。平移加单位四元数（Hamilton）。单位元为 (0,0,0,1)。
 * 先旋转再平移：p_world = R * p_local + t。欧拉角不放在这里，适配 HALCON 时再换算。
 */
struct HSCOMMDATA_EXPORT Pose3 {
    double tx; ///< 平移 X，毫米
    double ty; ///< 平移 Y，毫米
    double tz; ///< 平移 Z，毫米
    double qx; ///< 四元数虚部 i
    double qy; ///< 四元数虚部 j
    double qz; ///< 四元数虚部 k
    double qw; ///< 四元数实部。单位元为 1

    /** @brief 单位位姿：平移 0，四元数 (0,0,0,1) */
    Pose3();
    /**
     * @brief 指定平移和四元数。不在这里归一化
     * @param tx 平移 X
     * @param ty 平移 Y
     * @param tz 平移 Z
     * @param qx 四元数 i
     * @param qy 四元数 j
     * @param qz 四元数 k
     * @param qw 四元数实部
     */
    Pose3(double tx, double ty, double tz, double qx, double qy, double qz, double qw);

    /**
     * @brief 是否接近单位四元数
     * @param eps 模长与 1 的允许偏差
     */
    bool isUnit(double eps = 1e-6) const;
    /** @brief 四元数归一化后的副本。模长接近 0 时退回单位元，平移保留 */
    Pose3 normalized() const;
    /** @brief 只旋转向量，不加平移。四元数会先归一化 */
    Point3d rotate(const Point3d& v) const;
    /** @brief 把局部点变到世界：先旋转再加平移 */
    Point3d apply(const Point3d& p) const;
    /** @brief 逆位姿。先把四元数归一化 */
    Pose3 inverse() const;
};

/**
 * @class Aabb3
 * @brief 轴对齐包围盒，闭区间。默认构造是空盒（某一维 maxCorner 小于 minCorner）
 */
struct HSCOMMDATA_EXPORT Aabb3 {
    Point3d minCorner; ///< 最小角，含。不用 min，避免和 windows.h 宏冲突
    Point3d maxCorner; ///< 最大角，含。小于 minCorner 的对应分量表示该盒为空

    /** @brief 空盒 */
    Aabb3();
    /**
     * @brief 用最小角、最大角构造。不自动交换
     * @param minCorner 最小角
     * @param maxCorner 最大角
     */
    Aabb3(const Point3d& minCorner, const Point3d& maxCorner);

    /** @brief 任一维 maxCorner 小于 minCorner 则为空 */
    bool isEmpty() const;
    /** @brief 单点盒 */
    static Aabb3 fromPoint(const Point3d& p);
    /** @brief 并入一个点。空盒则变成该点 */
    void include(const Point3d& p);
    /** @brief 与另一盒求并。空盒不参与 */
    Aabb3 united(const Aabb3& other) const;
    /** @brief 点是否在闭区间内。空盒恒为 false */
    bool contains(const Point3d& p) const;
    /** @brief 中心。空盒返回原点 */
    Point3d center() const;
    /** @brief 各边长度。空盒返回零向量 */
    Point3d extent() const;
};

}

#endif
