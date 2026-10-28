/**
 * @file HsInterface3D.cpp
 * @brief HsInterface3D实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsInterface3D.h>
#include <HsCommData/HsInterface2D.h>
#include <HsCommData/HsSystem.h>

#include <cmath>

namespace HsCommData {

namespace {

// 绕 X 的旋转矩阵
void rotX(double a, double* r)
{
    const double c = std::cos(a);
    const double s = std::sin(a);
    r[0] = 1.0; r[1] = 0.0; r[2] = 0.0;
    r[3] = 0.0; r[4] = c;   r[5] = -s;
    r[6] = 0.0; r[7] = s;   r[8] = c;
}

// 绕 Y 的旋转矩阵
void rotY(double a, double* r)
{
    const double c = std::cos(a);
    const double s = std::sin(a);
    r[0] = c;   r[1] = 0.0; r[2] = s;
    r[3] = 0.0; r[4] = 1.0; r[5] = 0.0;
    r[6] = -s;  r[7] = 0.0; r[8] = c;
}

// 绕 Z 的旋转矩阵
void rotZ(double a, double* r)
{
    const double c = std::cos(a);
    const double s = std::sin(a);
    r[0] = c;   r[1] = -s;  r[2] = 0.0;
    r[3] = s;   r[4] = c;   r[5] = 0.0;
    r[6] = 0.0; r[7] = 0.0; r[8] = 1.0;
}

// 3x3 矩阵相乘，out = A * B
void mul3(const double* a, const double* b, double* out)
{
    double t[9];
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            t[row * 3 + col] = a[row * 3 + 0] * b[0 * 3 + col]
                + a[row * 3 + 1] * b[1 * 3 + col]
                + a[row * 3 + 2] * b[2 * 3 + col];
        }
    }
    for (int i = 0; i < 9; ++i) {
        out[i] = t[i];
    }
}

// 按顺序合成欧拉矩阵。abg 与 Zyx 相同，gba 与 Xyz 相同
void eulerMatrix(HsSystem::EulerOrder order, double rx, double ry, double rz, double* r)
{
    double x[9];
    double y[9];
    double z[9];
    rotX(rx, x);
    rotY(ry, y);
    rotZ(rz, z);
    const bool zyx = (order == HsSystem::EulerZyx || order == HsSystem::EulerAbg);
    if (zyx) {
        mul3(y, x, r);
        mul3(z, r, r);
    } else {
        mul3(y, z, r);
        mul3(x, r, r);
    }
}

// 旋转矩阵转 Hamilton 四元数，与 Pose3::rotate 一致
void quatFromMatrix(const double* r, double* qx, double* qy, double* qz, double* qw)
{
    const double tr = r[0] + r[4] + r[8];
    if (tr > 0.0) {
        const double s = std::sqrt(tr + 1.0) * 2.0;
        *qw = 0.25 * s;
        *qx = (r[7] - r[5]) / s;
        *qy = (r[2] - r[6]) / s;
        *qz = (r[3] - r[1]) / s;
    } else if (r[0] > r[4] && r[0] > r[8]) {
        const double s = std::sqrt(1.0 + r[0] - r[4] - r[8]) * 2.0;
        *qw = (r[7] - r[5]) / s;
        *qx = 0.25 * s;
        *qy = (r[1] + r[3]) / s;
        *qz = (r[2] + r[6]) / s;
    } else if (r[4] > r[8]) {
        const double s = std::sqrt(1.0 + r[4] - r[0] - r[8]) * 2.0;
        *qw = (r[2] - r[6]) / s;
        *qx = (r[1] + r[3]) / s;
        *qy = 0.25 * s;
        *qz = (r[5] + r[7]) / s;
    } else {
        const double s = std::sqrt(1.0 + r[8] - r[0] - r[4]) * 2.0;
        *qw = (r[3] - r[1]) / s;
        *qx = (r[2] + r[6]) / s;
        *qy = (r[5] + r[7]) / s;
        *qz = 0.25 * s;
    }
}

// 由 R = Rz*Ry*Rx 反解绕 X、Y、Z。万向节锁时 rz 置 0
void anglesZyx(const double* r, double* rx, double* ry, double* rz)
{
    const double sy = -r[6];
    *ry = std::asin(sy > 1.0 ? 1.0 : (sy < -1.0 ? -1.0 : sy));
    const double cy = std::cos(*ry);
    if (cy > 1e-12 || cy < -1e-12) {
        *rx = std::atan2(r[7], r[8]);
        *rz = std::atan2(r[3], r[0]);
    } else {
        *rx = std::atan2(-r[5], r[4]);
        *rz = 0.0;
    }
}

// 由 R = Rx*Ry*Rz 反解。万向节锁时 rz 置 0
void anglesXyz(const double* r, double* rx, double* ry, double* rz)
{
    const double sy = r[2];
    *ry = std::asin(sy > 1.0 ? 1.0 : (sy < -1.0 ? -1.0 : sy));
    const double cy = std::cos(*ry);
    if (cy > 1e-12 || cy < -1e-12) {
        *rx = std::atan2(-r[5], r[8]);
        *rz = std::atan2(-r[1], r[0]);
    } else {
        *rx = std::atan2(r[3], r[4]);
        *rz = 0.0;
    }
}

// 外部轴换到内部。Y 向上原样返回
Point3d axesToInternal(HsSystem::AxisFrame frame, const Point3d& e)
{
    if (frame == HsSystem::FrameZUp) {
        return Point3d(e.x, e.z, -e.y);
    }
    if (frame == HsSystem::FrameCam) {
        return Point3d(e.x, -e.y, -e.z);
    }
    return e;
}

// 内部轴换回外部。Y 向上原样返回
Point3d axesFromInternal(HsSystem::AxisFrame frame, const Point3d& p)
{
    if (frame == HsSystem::FrameZUp) {
        return Point3d(p.x, -p.z, p.y);
    }
    if (frame == HsSystem::FrameCam) {
        return Point3d(p.x, -p.y, -p.z);
    }
    return p;
}

// 外部欧拉角换到内部弧度、逆时针
double eulerToInternal(const HsSystem& system, double external)
{
    double a = external;
    if (system.angleUnit == HsSystem::UnitDegree) {
        a = toRadian(a);
    }
    if (system.angleDir == HsSystem::DirCw) {
        a = -a;
    }
    return a;
}

// 内部弧度换回外部单位和正方向
double eulerFromInternal(const HsSystem& system, double internalRad)
{
    double a = internalRad;
    if (system.angleDir == HsSystem::DirCw) {
        a = -a;
    }
    if (system.angleUnit == HsSystem::UnitDegree) {
        a = toDegree(a);
    }
    return a;
}

}

// 外部点换成内部点
Point3d toPoint3(const Point3d& external)
{
    const HsSystem& system = HsSystem::instance();
    Point3d e = external;
    if (system.hand == HsSystem::HandLeft) {
        e.z = -e.z;
    }
    const Point3d p = axesToInternal(system.axisFrame, e);
    const double s = toLength(1.0);
    return Point3d(p.x * s, p.y * s, p.z * s);
}

// 内部点换成外部点
Point3d fromPoint3(const Point3d& internal)
{
    const HsSystem& system = HsSystem::instance();
    const double s = fromLength(1.0);
    Point3d e = axesFromInternal(system.axisFrame, Point3d(internal.x * s, internal.y * s, internal.z * s));
    if (system.hand == HsSystem::HandLeft) {
        e.z = -e.z;
    }
    return e;
}

// 外部平移和欧拉角换成内部位姿
Pose3 toPose(double tx, double ty, double tz, double a, double b, double c)
{
    const HsSystem& system = HsSystem::instance();
    const Point3d t = toPoint3(Point3d(tx, ty, tz));
    double r[9];
    eulerMatrix(system.eulerOrder, eulerToInternal(system, a), eulerToInternal(system, b), eulerToInternal(system, c), r);
    double qx = 0.0;
    double qy = 0.0;
    double qz = 0.0;
    double qw = 1.0;
    quatFromMatrix(r, &qx, &qy, &qz, &qw);
    return Pose3(t.x, t.y, t.z, qx, qy, qz, qw);
}

// 内部位姿换成外部平移和欧拉角
void fromPose(const Pose3& pose, double* tx, double* ty, double* tz,
              double* a, double* b, double* c)
{
    const HsSystem& system = HsSystem::instance();
    const Point3d t = fromPoint3(Point3d(pose.tx, pose.ty, pose.tz));
    if (tx) {
        *tx = t.x;
    }
    if (ty) {
        *ty = t.y;
    }
    if (tz) {
        *tz = t.z;
    }
    const Point3d cx = pose.rotate(Point3d(1.0, 0.0, 0.0));
    const Point3d cy = pose.rotate(Point3d(0.0, 1.0, 0.0));
    const Point3d cz = pose.rotate(Point3d(0.0, 0.0, 1.0));
    const double r[9] = {
        cx.x, cy.x, cz.x,
        cx.y, cy.y, cz.y,
        cx.z, cy.z, cz.z
    };
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    const bool zyx = (system.eulerOrder == HsSystem::EulerZyx || system.eulerOrder == HsSystem::EulerAbg);
    if (zyx) {
        anglesZyx(r, &rx, &ry, &rz);
    } else {
        anglesXyz(r, &rx, &ry, &rz);
    }
    if (a) {
        *a = eulerFromInternal(system, rx);
    }
    if (b) {
        *b = eulerFromInternal(system, ry);
    }
    if (c) {
        *c = eulerFromInternal(system, rz);
    }
}

}
