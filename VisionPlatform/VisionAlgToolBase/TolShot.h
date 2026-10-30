/**
 * @file TolShot.h
 * @brief 本拍测值和公差结果。写出 AlgToolOut 之前留在这里
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOL_SHOT_H
#define VISION_TOL_SHOT_H

#include "AlgExport.h"
#include "AlgToolBase.h"

namespace Vision {

/**
 * @class TolShot
 * @brief 本拍测值和公差结果。算法先写在这里，再交给 AlgToolOut
 */
struct VISION_ALGOR_API TolShot {
    ParamMap values;             ///< 本拍测值
    bool ok;                     ///< 已写入的数值是否都在上下限内
    std::vector<TolMiss> miss;   ///< 未通过的项
    std::vector<char> poseOk;    ///< 与定位结果对齐。1 通过，0 不通过
    /** @brief 空结果，缺省判定通过 */
    TolShot() : ok(true) {}
    /** @brief 清空本拍 */
    void reset();
    /** @brief 写入一个浮点测值 */
    void put(const std::string& name, double value);
    /** @brief 写入一个整数测值 */
    void put_int(const std::string& name, int value);
    /** @brief 写入一个文本测值。文本不参与上下限 */
    void put_text(const std::string& name, const std::string& value);
    /** @brief 记下一项超限 */
    void add_miss(const std::string& name, double value, double minVal, double maxVal);
    /** @brief 记下一项没有测值 */
    void add_missing(const std::string& name);
    /** @brief 按公差表比较已写入的数值。skip 这项已经由算法单独判过 */
    void judge(const std::vector<JudgeParam>& rows, const char* skip);
    /** @brief 抄进算法输出 */
    void apply(AlgToolOut* out) const;
};

}

#endif
