#ifndef HSCOMMDATA_MEASURECALIB_H
#define HSCOMMDATA_MEASURECALIB_H

/**
 * @file MeasureCalib.h
 * @brief 图像平面的像素比例。几何仍用像素，测量时再换成显示单位。
 */

#include <HsCommData/HsCommDataExport.h>
#include <HsCommData/Geom.h>

#include <string>
#include <vector>

namespace HsCommData {

/** @brief 测量显示单位 */
enum LengthUnit {
    UnitPx = 0, ///< 像素
    UnitMm,     ///< 毫米
    UnitUm,     ///< 微米
    UnitPulse,   ///< 脉冲
    UnitMax ///< 个数，不是合法值
};

/**
 * @class UnitFactor
 * @brief 某一单位下，1 个像素对应多少
 */
struct HSCOMMDATA_EXPORT UnitFactor {
    LengthUnit unit;     ///< 单位
    double xPerPixel;    ///< 列方向
    double yPerPixel;    ///< 行方向

    UnitFactor() : unit(UnitPx), xPerPixel(1.0), yPerPixel(1.0) {}
};

/**
 * @class ImageCalib
 * @brief 窗口使用的像素比例。factors 为空表示尚未标定
 */
struct HSCOMMDATA_EXPORT ImageCalib {
    std::vector<UnitFactor> factors; ///< 已写入的比例。没有的单位按 1:1
    std::vector<LengthUnit> show;    ///< 要显示的单位，顺序即显示顺序，可多个

    /** @brief 默认只显示像素，比例 1:1 */
    ImageCalib();
};

/**
 * @brief 取某个单位的比例。像素恒为 1。微米可从毫米乘 1000，毫米可从微米除 1000。都没有则 1:1
 */
HSCOMMDATA_EXPORT UnitFactor resolveFactor(const ImageCalib& calib, LengthUnit unit);

/** @brief 像素位移换成该单位下的长度 */
HSCOMMDATA_EXPORT double lengthIn(double dxPx, double dyPx, const UnitFactor& factor);

/**
 * @brief 按 show 拼长度。多个单位用空格隔开
 * @return UTF-8
 */
HSCOMMDATA_EXPORT std::string formatLength(double dxPx, double dyPx, const ImageCalib& calib);

/**
 * @brief 按 show 拼点的横纵坐标。只有像素时返回空串，避免和原有像素读数重复
 * @return UTF-8
 */
HSCOMMDATA_EXPORT std::string formatPoint(double xPx, double yPx, const ImageCalib& calib);

/**
 * @brief 像素圆的半径和直径。X、Y 比例相同报一个半径，不同则分开报
 * @return UTF-8
 */
HSCOMMDATA_EXPORT std::string formatRadius(double rPx, const ImageCalib& calib);

/**
 * @brief 顶点处的夹角，单位度。先按显示用的比例缩放再求角
 * @param a 一边上的点
 * @param vertex 顶点
 * @param c 另一边上的点
 * @return UTF-8
 */
HSCOMMDATA_EXPORT std::string formatAngle(const Point2d& a, const Point2d& vertex, const Point2d& c,
                                          const ImageCalib& calib);
/**
 * @brief 轴对齐矩形的宽和高。宽走列比例，高走行比例
 * @return UTF-8
 */
HSCOMMDATA_EXPORT std::string formatRect(double wPx, double hPx, const ImageCalib& calib);

/**
 * @brief 椭圆两条半轴。半轴a走列比例，半轴b走行比例
 * @return UTF-8
 */
HSCOMMDATA_EXPORT std::string formatEllipse(double raPx, double rbPx, const ImageCalib& calib);

}

#endif
