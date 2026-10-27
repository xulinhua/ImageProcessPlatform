/**
 * @file HsSystem.cpp
 * @brief HsSystem实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsSystem.h>
#include <HsCommData/HsException.h>

namespace HsCommData {

namespace {

// 枚举项取值是否落在该枚举里
bool enumOk(HsSystem::Param param, int value)
{
    switch (param) {
    case HsSystem::ParamAngleUnit:
        return value == HsSystem::UnitRadian || value == HsSystem::UnitDegree;
    case HsSystem::ParamAngleDir:
        return value == HsSystem::DirCcw || value == HsSystem::DirCw;
    case HsSystem::ParamAngleZero:
        return value >= HsSystem::ZeroPosX && value <= HsSystem::ZeroNegY;
    case HsSystem::ParamLengthUnit:
        return value == HsSystem::UnitMm || value == HsSystem::UnitM;
    case HsSystem::ParamHand:
        return value == HsSystem::HandRight || value == HsSystem::HandLeft;
    case HsSystem::ParamAxisFrame:
        return value >= HsSystem::FrameYUp && value <= HsSystem::FrameCam;
    case HsSystem::ParamEulerOrder:
        return value >= HsSystem::EulerZyx && value <= HsSystem::EulerGba;
    case HsSystem::ParamExceptionMode:
        return value >= HsSystem::ExModeThrow && value <= HsSystem::ExModeIgnore;
    case HsSystem::ParamTol:
        return false;
    }
    return false;
}

// 参数和取值对不上
void failSystem()
{
    throw HsException("set_system", "参数或取值不符", HS_ERR_TYPE);
}

}

// 对外默认：度、逆时针
HsSystem::HsSystem()
{
    angleUnit = AngleUnit::UnitDegree;
    angleDir = AngleDir::DirCw;
    angleZero = AngleZero::ZeroPosX;
    lengthUnit = LengthUnit::UnitMm;
    hand = Handedness::HandRight;
    axisFrame = AxisFrame::FrameYUp;
    eulerOrder = EulerOrder::EulerZyx;
    tol = 1e-9;
    exceptionMode = ExceptionMode::ExModeThrow;
    exceptionCallback = nullptr;
}

// 全进程唯一一份
HsSystem& HsSystem::instance()
{
    static HsSystem system;
    return system;
}

// 用整数改枚举项
void set_system(HsSystem::Param param, int value)
{
    if (param == HsSystem::ParamTol || !enumOk(param, value)) {
        failSystem();
    }
    HsSystem& system = HsSystem::instance();
    switch (param) {
    case HsSystem::ParamAngleUnit:
        system.angleUnit = static_cast<HsSystem::AngleUnit>(value);
        break;
    case HsSystem::ParamAngleDir:
        system.angleDir = static_cast<HsSystem::AngleDir>(value);
        break;
    case HsSystem::ParamAngleZero:
        system.angleZero = static_cast<HsSystem::AngleZero>(value);
        break;
    case HsSystem::ParamLengthUnit:
        system.lengthUnit = static_cast<HsSystem::LengthUnit>(value);
        break;
    case HsSystem::ParamHand:
        system.hand = static_cast<HsSystem::Handedness>(value);
        break;
    case HsSystem::ParamAxisFrame:
        system.axisFrame = static_cast<HsSystem::AxisFrame>(value);
        break;
    case HsSystem::ParamEulerOrder:
        system.eulerOrder = static_cast<HsSystem::EulerOrder>(value);
        break;
    case HsSystem::ParamExceptionMode:
        system.exceptionMode = static_cast<HsSystem::ExceptionMode>(value);
        break;
    case HsSystem::ParamTol:
        break;
    }
}

// 改比较容差
void set_system(HsSystem::Param param, double value)
{
    if (param != HsSystem::ParamTol || value < 0.0) {
        failSystem();
    }
    HsSystem::instance().tol = value;
}

// 读出枚举项
void get_system(HsSystem::Param param, int* value)
{
    if (!value || param == HsSystem::ParamTol) {
        throw HsException("get_system", "参数或取值不符", HS_ERR_TYPE);
    }
    const HsSystem& system = HsSystem::instance();
    switch (param) {
    case HsSystem::ParamAngleUnit:
        *value = system.angleUnit;
        break;
    case HsSystem::ParamAngleDir:
        *value = system.angleDir;
        break;
    case HsSystem::ParamAngleZero:
        *value = system.angleZero;
        break;
    case HsSystem::ParamLengthUnit:
        *value = system.lengthUnit;
        break;
    case HsSystem::ParamHand:
        *value = system.hand;
        break;
    case HsSystem::ParamAxisFrame:
        *value = system.axisFrame;
        break;
    case HsSystem::ParamEulerOrder:
        *value = system.eulerOrder;
        break;
    case HsSystem::ParamExceptionMode:
        *value = system.exceptionMode;
        break;
    case HsSystem::ParamTol:
        break;
    }
}

// 读出比较容差
void get_system(HsSystem::Param param, double* value)
{
    if (!value || param != HsSystem::ParamTol) {
        throw HsException("get_system", "参数或取值不符", HS_ERR_TYPE);
    }
    *value = HsSystem::instance().tol;
}

// 设置异常回调
void set_system(HsSystem::ExceptionCallback callback)
{
    HsSystem::instance().exceptionCallback = callback;
}

// 读出异常回调
HsSystem::ExceptionCallback get_exception_callback()
{
    return HsSystem::instance().exceptionCallback;
}

}
