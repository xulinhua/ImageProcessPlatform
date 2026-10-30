/**
 * @file HsInterface2D.cpp
 * @brief HsInterface2D实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsInterface2D.h>
#include <HsCommData/HsConst.h>
#include <HsCommData/HsSystem.h>

namespace HsCommData {

// 角度换成弧度
double toRadian(double deg)
{
    return deg * kPi / 180.0;
}

// 弧度换成角度
double toDegree(double rad)
{
    return rad * 180.0 / kPi;
}

// 外部平面角换成内部弧度。图像 y 向下，内部正角从 +x 转向 +y，画面上是顺时针。系统逆时针为正时取反
double toAngle(double external)
{
    const HsSystem& system = HsSystem::instance();
    double a = external;
    if (system.angleUnit == HsSystem::UnitDegree) {
        a = toRadian(a);
    }
    if (system.angleDir == HsSystem::DirCcw) {
        a = -a;
    }
    return a;
}

// 内部弧度换成外部平面角。与 toAngle 互逆
double fromAngle(double internalRad)
{
    const HsSystem& system = HsSystem::instance();
    double a = internalRad;
    if (system.angleDir == HsSystem::DirCcw) {
        a = -a;
    }
    if (system.angleUnit == HsSystem::UnitDegree) {
        a = toDegree(a);
    }
    return a;
}


// 外部长度到毫米
double toLength(double external)
{
    if (HsSystem::instance().lengthUnit == HsSystem::UnitM) {
        return external * 1000.0;
    }
    return external;
}

// 毫米到外部长度
double fromLength(double mm)
{
    if (HsSystem::instance().lengthUnit == HsSystem::UnitM) {
        return mm * 0.001;
    }
    return mm;
}

// 差不超过系统容差
bool withinTol(double a, double b)
{
    const double d = a - b;
    const double tol = HsSystem::instance().tol;
    return d <= tol && d >= -tol;
}

}
