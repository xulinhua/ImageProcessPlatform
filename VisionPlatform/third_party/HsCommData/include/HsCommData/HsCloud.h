/**
 * @file HsCloud.h
 * @brief HsCloud头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSCLOUD_H
#define HSCOMMDATA_HSCLOUD_H

#include <vector>

#include <HsCommData/Geom3.h>

namespace HsCommData {

/**
 * @class Rgb8
 * @brief 点云单点颜色，无透明度。叠加绘制颜色用 Color4，不要混用
 */
struct HSCOMMDATA_EXPORT Rgb8 {
    unsigned char r; ///< 红，0-255
    unsigned char g; ///< 绿，0-255
    unsigned char b; ///< 蓝，0-255

    /** @brief 黑色 */
    Rgb8();
    /**
     * @brief 指定三分量
     * @param r 红
     * @param g 绿
     * @param b 蓝
     */
    Rgb8(unsigned char r, unsigned char g, unsigned char b);
};

/**
 * @class Cloud3
 * @brief 点云。xyz 必有，按点存放。法向、灰度、RGB 可空，非空时必须与点数相同。
 * width、height 都为 0 表示无序；都大于 0 且乘积等于点数表示有序点云。没有单独的深度图类型。
 * 整份拷贝，不共享缓冲。
 */
struct HSCOMMDATA_EXPORT Cloud3 {
    std::vector<Point3d> xyz;     ///< 点坐标，毫米。空则点云为空
    std::vector<Point3d> normals; ///< 点法向。空表示没有；非空必须与 xyz 等长
    std::vector<float> gray;     ///< 灰度。空表示没有；非空必须与 xyz 等长
    std::vector<Rgb8> rgb;       ///< 颜色。空表示没有；非空必须与 xyz 等长
    int width;                   ///< 有序点云列数。0 表示无序
    int height;                  ///< 有序点云行数。0 表示无序

    /** @brief 空点云，无序 */
    Cloud3();
    /** @brief 同类型拷贝，坐标和属性整份复制 */
    Cloud3(const Cloud3& other);
    /** @brief 同类型赋值，坐标和属性整份复制 */
    Cloud3& operator=(const Cloud3& other);

    /** @brief 没有点则为空 */
    bool isEmpty() const;
    /** @brief 有序且宽高乘积等于点数 */
    bool isOrganized() const;
    /**
     * @brief 可选数组要么为空，要么与点数相同；宽高要么都是 0，要么构成合法有序尺寸
     */
    bool attribsOk() const;
    /** @brief 点坐标的轴对齐包围盒。空点云返回空 Aabb3 */
    Aabb3 bounds() const;
};

}

#endif
