/**
 * @file CalibMath.h
 * @brief CalibMath头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CALIB_MATH_H
#define VISION_CALIB_MATH_H
#include "CalibTypes.h"

namespace Vision {

/** @brief 用点对拟合平面映射，并填写残差与逆矩阵 */
bool fitPlaneMap(CalibPlaneFit fit, const std::vector<PlaneSample>& samples, PlaneMap* out, std::string* err);

/**
 * @brief 用角度样本拟合旋转中心，样本可以是像素也可以是毫米，结果与样本同单位
 * @return 样本少于 3 个、重合、近似共线或拟合失败时返回假并写 err；最小半径由调用方按单位判断
 */
VISION_CALIB_API bool fitRotateCenter(const std::vector<RotateSample>& samples, RotateCenter* out, std::string* err);

/** @brief 像素点经平面映射到毫米。矩阵不可用时返回假 */
bool mapImageToWorld(const double imageToWorld[9], double imageX, double imageY, double* worldX, double* worldY);

/** @brief 绕中心旋转一个点。角度为度，逆时针为正 */
void rotatePoint(double cx, double cy, double angleDeg, double x, double y, double* outX, double* outY);

}

#endif