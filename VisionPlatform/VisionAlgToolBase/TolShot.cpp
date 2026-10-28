/**
 * @file TolShot.cpp
 * @brief 本拍测值的写入和上下限比较
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "TolShot.h"

namespace Vision {

void TolShot::reset()
{
    values.clear();
    ok = true;
    miss.clear();
    poseOk.clear();
}

void TolShot::put(const std::string& name, double value)
{
    values.insert(name, value);
}

void TolShot::put_int(const std::string& name, int value)
{
    values.insert(name, value);
}

void TolShot::put_text(const std::string& name, const std::string& value)
{
    values.insert(name, value);
}

void TolShot::add_miss(const std::string& name, double value, double minVal, double maxVal)
{
    ok = false;
    TolMiss one;
    one.name = name;
    one.hasValue = true;
    one.value = value;
    one.minVal = minVal;
    one.maxVal = maxVal;
    miss.push_back(one);
}

void TolShot::add_missing(const std::string& name)
{
    ok = false;
    TolMiss one;
    one.name = name;
    one.hasValue = false;
    miss.push_back(one);
}

void TolShot::judge(const std::vector<JudgeParam>& rows, const char* skip)
{
    const std::string skipped = skip != nullptr ? skip : "";
    for (size_t i = 0; i < rows.size(); ++i) {
        const JudgeParam& row = rows[i];
        if (!row.enable || (!skipped.empty() && row.name == skipped)) {
            continue;
        }
        if (!values.contains(row.name)) {
            add_missing(row.name);
            continue;
        }
        const ParamValue got = values.value(row.name);
        if (got.kind() != ParamValue::KindInt && got.kind() != ParamValue::KindDouble) {
            continue;
        }
        const double value = got.toDouble(0);
        if (value < row.minVal || value > row.maxVal) {
            add_miss(row.name, value, row.minVal, row.maxVal);
        }
    }
}

void TolShot::apply(AlgToolOut* out) const
{
    if (out == nullptr) {
        return;
    }
    out->values = values;
    out->judgeOk = ok;
    out->tolMiss = miss;
    if (!poseOk.empty()) {
        out->poseOk = poseOk;
    }
}

}
