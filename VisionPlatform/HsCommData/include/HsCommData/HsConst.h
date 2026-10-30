/**
 * @file HsConst.h
 * @brief 库内共用的数值常量。只放多处会用到的数，某个类型自己的比例留在该类型里
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSCONST_H
#define HSCOMMDATA_HSCONST_H

namespace HsCommData {

const double kPi = 3.14159265358979323846; ///< 圆周率
const double kZeroLen2 = 1e-24;            ///< 长度平方低于此值视为零长度

}

#endif
