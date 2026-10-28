/**
 * @file HsCross.h
 * @brief HsCross头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSCROSS_H
#define HSCOMMDATA_HSCROSS_H

#include <HsCommData/HsCommDataExport.h>
#include <HsCommData/Geom.h>

namespace HsCommData {

/**
 * @class HsCross
 * @brief Cross marker. Center, arm half-length and angle stay together.
 */
struct HSCOMMDATA_EXPORT HsCross {
    Point2d center; ///< center
    double size;   ///< half arm in pixels; <0 means follow window
    double angle;  ///< 内部转角，弧度。正方向从 +x 转向 +y。对外用 ang、angDeg
    /** @brief 内部转角，弧度 */
    double angRad() const;
    /** @brief 写入内部弧度 */
    void setAngRad(double rad);
    /** @brief 转角的度数，正方向与内部相同 */
    double angDeg() const;
    /** @brief 按度数写入，正方向与内部相同 */
    void setAngDeg(double deg);
    /** @brief 按系统角度单位和正方向读取 */
    double ang() const;
    /** @brief 按系统角度单位和正方向写入 */
    void setAng(double external);

    HsCross(); ///< size 10, angle 0
};

}

#endif
