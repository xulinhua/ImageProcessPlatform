/**
 * @file HsInterface3D.h
 * @brief 三维自由函数。点、位姿以及以后的三维自由函数放这里
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSINTERFACE3D_H
#define HSCOMMDATA_HSINTERFACE3D_H

#include <HsCommData/HsCommDataExport.h>

#include <HsCommData/Geom3.h>

namespace HsCommData {

/**
 * @brief 外部点换成内部点（轴、手性、长度一起换）
 * @param external 外部坐标
 */
HSCOMMDATA_EXPORT Point3d toPoint3(const Point3d& external);
/**
 * @brief 内部点换成外部点
 * @param internal 内部坐标，毫米
 */
HSCOMMDATA_EXPORT Point3d fromPoint3(const Point3d& internal);

/**
 * @brief 外部平移和欧拉角换成内部位姿。欧拉分量只换单位和正方向，不套平面角零位
 * @param tx,ty,tz 外部平移
 * @param a,b,c 绕 X、Y、Z 的外部欧拉角，顺序看系统里的欧拉顺序
 */
HSCOMMDATA_EXPORT Pose3 toPose(double tx, double ty, double tz, double a, double b, double c);
/**
 * @brief 内部位姿换成外部平移和欧拉角。万向节锁时把绕 Z 的分量置 0。指针可空
 * @param pose 内部位姿
 */
HSCOMMDATA_EXPORT void fromPose(const Pose3& pose, double* tx, double* ty, double* tz,
                                double* a, double* b, double* c);

}

#endif
