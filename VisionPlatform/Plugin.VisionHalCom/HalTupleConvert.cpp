/**
 * @file HalTupleConvert.cpp
 * @brief HalTupleConvert实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalTupleConvert.h"

namespace Vision {

// HsTuple → HTuple。混合元组按格类型追加；空元组得空 HTuple
bool hsTupleToHTuple(const HsCommData::HsTuple& src, HalconCpp::HTuple*& out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = "HTuple is null";
        }
        return false;
    }
    try {
        HalconCpp::HTuple t;
        const int n = src.length();
        if (n <= 0) {
            *out = t;
            return true;
        }
        const HsCommData::TupleType ty = src.type();
        if (ty == HsCommData::TupleLong) {
            for (int i = 0; i < n; ++i) {
                t.Append(static_cast<Hlong>(src.L(i)));
            }
        } else if (ty == HsCommData::TupleDouble) {
            for (int i = 0; i < n; ++i) {
                t.Append(src.D(i));
            }
        } else if (ty == HsCommData::TupleString) {
            for (int i = 0; i < n; ++i) {
                t.Append(src.S(i).c_str());
            }
        } else {
            // 混合：逐格按类型追加
            for (int i = 0; i < n; ++i) {
                int64_t lv = 0;
                double dv = 0;
                std::string sv;
                if (src.getLong(i, &lv)) {
                    t.Append(static_cast<Hlong>(lv));
                } else if (src.getDouble(i, &dv)) {
                    t.Append(dv);
                } else if (src.getString(i, &sv)) {
                    t.Append(sv.c_str());
                } else {
                    if (err) {
                        *err = "unsupported HsTuple element";
                    }
                    return false;
                }
            }
        }
        *out = t;
        return true;
    } catch (const HalconCpp::HException& e) {
        if (err) {
            *err = e.ErrorMessage().Text() ? std::string(e.ErrorMessage().Text()) : std::string("Halcon error");
        }
        return false;
    } catch (...) {
        if (err) {
            *err = "HsTuple convert failed";
        }
        return false;
    }
}

// HTuple → HsTuple。按 Halcon 元素类型写入 long/double/string
bool hTupleToHsTuple(const HalconCpp::HTuple& src, HsCommData::HsTuple*& out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = "HsTuple is null";
        }
        return false;
    }
    try {
        HsCommData::HsTuple t;
        const Hlong n = src.Length();
        for (Hlong i = 0; i < n; ++i) {
            const HalconCpp::HTupleElement e = src[i];
            const HalconCpp::HTupleElementType ty = e.Type();
            if (ty == HalconCpp::eElementTypeLong) {
                t.append(static_cast<int64_t>(e.L()));
            } else if (ty == HalconCpp::eElementTypeDouble) {
                t.append(e.D());
            } else if (ty == HalconCpp::eElementTypeString) {
                const char* s = e.S().Text();
                t.append(s ? std::string(s) : std::string());
            } else {
                if (err) {
                    *err = "unsupported HTuple element type";
                }
                return false;
            }
        }
        *out = t;
        return true;
    } catch (const HalconCpp::HException& e) {
        if (err) {
            *err = e.ErrorMessage().Text() ? std::string(e.ErrorMessage().Text()) : std::string("Halcon error");
        }
        return false;
    } catch (...) {
        if (err) {
            *err = "HTuple convert failed";
        }
        return false;
    }
}

}
