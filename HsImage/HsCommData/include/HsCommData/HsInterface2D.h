#ifndef HSCOMMDATA_HSINTERFACE2D_H
#define HSCOMMDATA_HSINTERFACE2D_H

#include <HsCommData/HsCommDataExport.h>

/**
 * @file HsInterface2D.h
 * @brief 与维数无关以及平面上的自由函数。角度、长度、容差在这里，以后的平面自由函数也放这里
 */

namespace HsCommData {

/**
 * @brief 角度换成弧度
 * @param deg 角度
 */
HSCOMMDATA_EXPORT double toRadian(double deg);
/**
 * @brief 弧度换成角度
 * @param rad 弧度
 */
HSCOMMDATA_EXPORT double toDegree(double rad);
/**
 * @brief 外部平面角换成内部弧度。单位跟系统。内部正角在画面上是顺时针，系统逆时针为正时取反
 * @param external 外部转角
 */
HSCOMMDATA_EXPORT double toAngle(double external);
/**
 * @brief 内部弧度换成外部平面角
 * @param internalRad 内部弧度
 */
HSCOMMDATA_EXPORT double fromAngle(double internalRad);

/**
 * @brief 外部长度换成毫米
 * @param external 外部长度
 */
HSCOMMDATA_EXPORT double toLength(double external);
/**
 * @brief 毫米换成外部长度
 * @param mm 毫米
 */
HSCOMMDATA_EXPORT double fromLength(double mm);

/**
 * @brief 两数之差不超过系统容差。不叫 near，避免和 windows.h 的 near 宏冲突
 * @return 足够近时为 true
 */
HSCOMMDATA_EXPORT bool withinTol(double a, double b);

}

#endif
