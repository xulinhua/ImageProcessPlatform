/**
 * @file HsSystem.h
 * @brief HsSystem头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSSYSTEM_H
#define HSCOMMDATA_HSSYSTEM_H

#include <HsCommData/HsCommDataExport.h>
#include <HsCommData/HsException.h>

namespace HsCommData {

/**
 * @class HsSystem
 * @brief 全局换算约定。单例，全进程共用
 * 不改已存的几何。平面角内部是弧度，正方向从 +x 转向 +y（画面顺时针，与 OpenCV atan2 / RotatedRect 一致）。单位、正方向和零位只在对外换算。三维仍是右手系、毫米、Y 向上、Z 朝观察者。
 * 这里的单位、正方向、手性和轴朝向只用于把外部数值换进内部，或把内部数值换出去。
 */
class HSCOMMDATA_EXPORT HsSystem {
public:
    /** @brief set_system / get_system 的参数项 */
    enum Param {
        ParamAngleUnit = 0, ///< 角度单位
        ParamAngleDir,      ///< 角度正方向
        ParamAngleZero,     ///< 平面角零位
        ParamLengthUnit,    ///< 长度单位
        ParamHand,          ///< 手性
        ParamAxisFrame,     ///< 轴朝向
        ParamEulerOrder,    ///< 欧拉顺序
        ParamTol,            ///< 比较容差
        ParamExceptionMode   ///< 异常处理方式
    };

    /**
     * @brief 出错时怎么处理。对应 Halcon 的抛出、回调、忽略
     * 默认抛出，和现在的算子一致
     */
    enum ExceptionMode {
        ExModeThrow = 0, ///< 抛出 HsException
        ExModeCallback,  ///< 调用回调后继续。回调为空则仍抛出
        ExModeIgnore     ///< 不处理，调用方继续执行
    };

    /** @brief 异常回调。忽略方式下不会被调用 */
    typedef void (*ExceptionCallback)(const HsException& exception);

    /** @brief 对外角度单位。内部存储是弧度 */
    enum AngleUnit {
        UnitRadian = 0, ///< 弧度
        UnitDegree      ///< 度
    };

    /** @brief 对外角度正方向。平面内部正方向是从 +x 转向 +y，画面上为顺时针。系统为逆时针为正时对外取反 */
    enum AngleDir {
        DirCcw = 0, ///< 逆时针为正
        DirCw       ///< 顺时针为正
    };

    /** @brief 对外平面角零位。内部零位是 +X。欧拉角分量不用这项 */
    enum AngleZero {
        ZeroPosX = 0, ///< +X
        ZeroPosY,     ///< +Y
        ZeroNegX,     ///< -X
        ZeroNegY      ///< -Y
    };

    /** @brief 对外长度单位。内部三维是毫米。二维像素不经这里换 */
    enum LengthUnit {
        UnitMm = 0, ///< 毫米
        UnitM,       ///< 米
        UnitMax ///< 个数，不是合法值
    };

    /** @brief 对外手性。内部是右手系。左手先把外部 Z 取反再映射 */
    enum Handedness {
        HandRight = 0, ///< 右手
        HandLeft       ///< 左手
    };

    /**
     * @brief 对外轴朝向。映射到内部的 X 向右、Y 向上、Z 朝观察者
     * FrameZUp 的 Y 是向前（离开观察者）。FrameCam 是相机系：Y 向下，Z 向前
     */
    enum AxisFrame {
        FrameYUp = 0, ///< Y 向上，Z 朝观察者
        FrameZUp,     ///< Z 向上，Y 向前
        FrameCam      ///< 相机系，Y 向下，Z 向前
    };

    /**
     * @brief 欧拉顺序，只用于位姿换算。旋转矩阵作用在列向量上，先乘的在右边
     * EulerZyx 与 Halcon abg 相同；EulerXyz 与 Halcon gba（顺序码 0）相同
     */
    enum EulerOrder {
        EulerZyx = 0, ///< R = Rz * Ry * Rx，先绕 X
        EulerXyz,     ///< R = Rx * Ry * Rz，先绕 Z
        EulerAbg,     ///< Halcon abg，同 EulerZyx
        EulerGba      ///< Halcon gba，同 EulerXyz
    };

    AngleUnit angleUnit;   ///< 角度单位
    AngleDir angleDir;     ///< 角度正方向
    AngleZero angleZero;   ///< 平面角零位
    LengthUnit lengthUnit; ///< 长度单位
    Handedness hand;       ///< 手性
    AxisFrame axisFrame;   ///< 轴朝向
    EulerOrder eulerOrder; ///< 欧拉顺序
    double tol;            ///< 比较容差。不改几何库内部已有的比较
    ExceptionMode exceptionMode;         ///< 异常处理方式
    ExceptionCallback exceptionCallback; ///< 异常回调。空表示没有
    /** @brief 全进程唯一的一份。首次调用时按内部约定构造 */
    static HsSystem& instance();

private:
    /** @brief 度、逆时针、+X、毫米、右手、Y 向上、EulerZyx，容差 1e-9 */
    HsSystem();
    /** @brief 禁止拷贝，避免出现第二份约定 */
    HsSystem(const HsSystem&);
    /** @brief 禁止赋值 */
    HsSystem& operator=(const HsSystem&);
};

/**
 * @brief 改全局约定里的一项。枚举项传 int，容差传 double。项和取值对不上抛 HsException
 * @param param 参数项
 * @param value 枚举值
 */
HSCOMMDATA_EXPORT void set_system(HsSystem::Param param, int value);
/**
 * @brief 设置异常回调。传空则清掉
 * 只在 ExModeCallback 时使用
 */
HSCOMMDATA_EXPORT void set_system(HsSystem::ExceptionCallback callback);
/** @brief 当前异常回调。没有则为空 */
HSCOMMDATA_EXPORT HsSystem::ExceptionCallback get_exception_callback();
/**
 * @brief 改比较容差。其它项不要用这个重载
 * @param param 只能是 ParamTol
 * @param value 容差
 */
HSCOMMDATA_EXPORT void set_system(HsSystem::Param param, double value);
/**
 * @brief 读枚举项。指针空或拿它读容差抛 HsException
 * @param param 参数项
 * @param value 写出枚举值
 */
HSCOMMDATA_EXPORT void get_system(HsSystem::Param param, int* value);
/**
 * @brief 读比较容差。指针空或拿它读枚举项抛 HsException
 * @param param 只能是 ParamTol
 * @param value 写出容差
 */
HSCOMMDATA_EXPORT void get_system(HsSystem::Param param, double* value);

}

#endif
