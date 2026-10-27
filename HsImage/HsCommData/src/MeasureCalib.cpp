/**
 * @file MeasureCalib.cpp
 * @brief MeasureCalib实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/MeasureCalib.h>
#include <HsCommData/HsInterface2D.h>
#include <HsCommData/HsSystem.h>

#include <cmath>
#include <cstdio>

namespace HsCommData {

// 默认只显示像素，比例 1:1
ImageCalib::ImageCalib()
{
    show.push_back(UnitPx);
}

namespace {

const double kPi = 3.14159265358979323846;

// 两个比例是否可当成相同
bool nearlySame(double a, double b)
{
    const double scale = std::fabs(a) + std::fabs(b);
    const double tol = 1e-9 * (scale > 1.0 ? scale : 1.0);
    return std::fabs(a - b) <= tol;
}

// 在已写入的比例里查找该单位
bool findFactor(const ImageCalib& calib, LengthUnit unit, UnitFactor* out)
{
    for (size_t i = 0; i < calib.factors.size(); ++i) {
        if (calib.factors[i].unit != unit) {
            continue;
        }
        *out = calib.factors[i];
        if (out->xPerPixel == 0.0) {
            out->xPerPixel = 1.0;
        }
        if (out->yPerPixel == 0.0) {
            out->yPerPixel = 1.0;
        }
        return true;
    }
    return false;
}

// 单位名。微米用 UTF-8 字节，调用方按 UTF-8 显示
const char* unitName(LengthUnit unit)
{
    switch (unit) {
    case UnitMm: return "mm";
    case UnitUm: return "\xCE\xBCm";
    case UnitPulse: return "pulse";
    case UnitPx:
    default: return "px";
    }
}

// 该单位的小数位数
int decimals(LengthUnit unit)
{
    switch (unit) {
    case UnitUm: return 1;
    case UnitPulse: return 1;
    case UnitPx: return 2;
    case UnitMm:
    default: return 3;
    }
}

// 按小数位数格式化
std::string num(double v, int dec)
{
    char buf[64];
    sprintf(buf, "%.*f", dec, v);
    return std::string(buf);
}

// 要显示的单位。空列表当成只显示像素
std::vector<LengthUnit> shown(const ImageCalib& calib)
{
    if (!calib.show.empty()) {
        return calib.show;
    }
    std::vector<LengthUnit> one;
    one.push_back(UnitPx);
    return one;
}

// 显示列表里是否有像素以外的单位
bool physicalShown(const ImageCalib& calib)
{
    const std::vector<LengthUnit> units = shown(calib);
    for (size_t i = 0; i < units.size(); ++i) {
        if (units[i] != UnitPx) {
            return true;
        }
    }
    return false;
}

// 求角用的比例。优先第一个非像素单位
UnitFactor scaleForAngle(const ImageCalib& calib)
{
    const std::vector<LengthUnit> units = shown(calib);
    for (size_t i = 0; i < units.size(); ++i) {
        if (units[i] != UnitPx) {
            return resolveFactor(calib, units[i]);
        }
    }
    if (!calib.factors.empty()) {
        return resolveFactor(calib, calib.factors[0].unit);
    }
    return resolveFactor(calib, UnitPx);
}

}

// 解析单位比例。像素恒为 1，没有记录则 1:1
// 宽高换成显示单位。第一个数走列比例，第二个数走行比例
static std::string formatAxes(double wPx, double hPx, const char* wTag, const char* hTag, const ImageCalib& calib)
{
    const std::vector<LengthUnit> units = shown(calib);
    std::string text;
    for (size_t i = 0; i < units.size(); ++i) {
        const UnitFactor factor = resolveFactor(calib, units[i]);
        if (!text.empty()) {
            text += "\n";
        }
        const int dec = decimals(units[i]);
        const char* name = unitName(units[i]);
        text += wTag;
        text += " ";
        text += num(std::fabs(wPx) * factor.xPerPixel, dec);
        text += " ";
        text += name;
        text += "  ";
        text += hTag;
        text += " ";
        text += num(std::fabs(hPx) * factor.yPerPixel, dec);
        text += " ";
        text += name;
    }
    return text;
}
UnitFactor resolveFactor(const ImageCalib& calib, LengthUnit unit)
{
    UnitFactor factor;
    factor.unit = unit;
    factor.xPerPixel = 1.0;
    factor.yPerPixel = 1.0;
    if (unit == UnitPx) {
        return factor;
    }
    if (findFactor(calib, unit, &factor)) {
        factor.unit = unit;
        return factor;
    }
    if (unit == UnitUm) {
        UnitFactor mm;
        if (findFactor(calib, UnitMm, &mm)) {
            factor.xPerPixel = mm.xPerPixel * 1000.0;
            factor.yPerPixel = mm.yPerPixel * 1000.0;
            return factor;
        }
    }
    if (unit == UnitMm) {
        UnitFactor um;
        if (findFactor(calib, UnitUm, &um)) {
            factor.xPerPixel = um.xPerPixel / 1000.0;
            factor.yPerPixel = um.yPerPixel / 1000.0;
            return factor;
        }
    }
    return factor;
}

// 像素位移换成该单位下的长度
double lengthIn(double dxPx, double dyPx, const UnitFactor& factor)
{
    const double dx = dxPx * factor.xPerPixel;
    const double dy = dyPx * factor.yPerPixel;
    return std::sqrt(dx * dx + dy * dy);
}

// 按显示单位拼长度
std::string formatLength(double dxPx, double dyPx, const ImageCalib& calib)
{
    const std::vector<LengthUnit> units = shown(calib);
    std::string text;
    for (size_t i = 0; i < units.size(); ++i) {
        const UnitFactor factor = resolveFactor(calib, units[i]);
        if (!text.empty()) {
            text += "\n";
        }
        text += num(lengthIn(dxPx, dyPx, factor), decimals(units[i]));
        text += " ";
        text += unitName(units[i]);
    }
    return text;
}

// 按显示单位拼坐标。只有像素时返回空串
std::string formatPoint(double xPx, double yPx, const ImageCalib& calib)
{
    if (!physicalShown(calib)) {
        return std::string();
    }
    const std::vector<LengthUnit> units = shown(calib);
    std::string text;
    for (size_t i = 0; i < units.size(); ++i) {
        if (units[i] == UnitPx) {
            continue;
        }
        const UnitFactor factor = resolveFactor(calib, units[i]);
        if (!text.empty()) {
            text += "\n";
        }
        text += "X ";
        text += num(xPx * factor.xPerPixel, decimals(units[i]));
        text += " ";
        text += unitName(units[i]);
        text += "  Y ";
        text += num(yPx * factor.yPerPixel, decimals(units[i]));
        text += " ";
        text += unitName(units[i]);
    }
    return text;
}

// 像素圆的半径和直径
std::string formatRadius(double rPx, const ImageCalib& calib)
{
    const std::vector<LengthUnit> units = shown(calib);
    std::string text;
    for (size_t i = 0; i < units.size(); ++i) {
        const UnitFactor factor = resolveFactor(calib, units[i]);
        if (!text.empty()) {
            text += "\n";
        }
        const int dec = decimals(units[i]);
        const char* name = unitName(units[i]);
        if (nearlySame(factor.xPerPixel, factor.yPerPixel)) {
            const double r = rPx * factor.xPerPixel;
            text += "R ";
            text += num(r, dec);
            text += " ";
            text += name;
            text += "  D ";
            text += num(r * 2.0, dec);
            text += " ";
            text += name;
        } else {
            const double rx = rPx * factor.xPerPixel;
            const double ry = rPx * factor.yPerPixel;
            text += "Rx ";
            text += num(rx, dec);
            text += " ";
            text += name;
            text += "  Ry ";
            text += num(ry, dec);
            text += " ";
            text += name;
            text += "  Dx ";
            text += num(rx * 2.0, dec);
            text += " ";
            text += name;
            text += "  Dy ";
            text += num(ry * 2.0, dec);
            text += " ";
            text += name;
        }
    }
    return text;
}

// 三点夹角，单位度
std::string formatAngle(const Point2d& a, const Point2d& vertex, const Point2d& c, const ImageCalib& calib)
{
    const UnitFactor factor = scaleForAngle(calib);
    const double v1x = (a.x - vertex.x) * factor.xPerPixel;
    const double v1y = (a.y - vertex.y) * factor.yPerPixel;
    const double v2x = (c.x - vertex.x) * factor.xPerPixel;
    const double v2y = (c.y - vertex.y) * factor.yPerPixel;
    const double n1 = std::sqrt(v1x * v1x + v1y * v1y);
    const double n2 = std::sqrt(v2x * v2x + v2y * v2y);
    if (n1 < 1e-12 || n2 < 1e-12) {
        return std::string();
    }
    double cosv = (v1x * v2x + v1y * v2y) / (n1 * n2);
    if (cosv > 1.0) {
        cosv = 1.0;
    }
    if (cosv < -1.0) {
        cosv = -1.0;
    }
    const double rad = std::acos(cosv);
    if (HsSystem::instance().angleUnit == HsSystem::UnitDegree) {
        return num(toDegree(rad), 2) + " deg";
    }
    return num(rad, 4) + " rad";
}

// 轴对齐矩形的宽和高
std::string formatRect(double wPx, double hPx, const ImageCalib& calib)
{
    return formatAxes(wPx, hPx, "W", "H", calib);
}

// 椭圆两条半轴
std::string formatEllipse(double raPx, double rbPx, const ImageCalib& calib)
{
    return formatAxes(raPx, rbPx, "Ra", "Rb", calib);
}
}
