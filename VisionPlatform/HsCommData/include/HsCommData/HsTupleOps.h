/**
 * @file HsTupleOps.h
 * @brief HsTuple 与标量的比较、算术运算符。由 HsTuple.h 末尾包含，勿单独包含
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSTUPLEOPS_H
#define HSCOMMDATA_HSTUPLEOPS_H

namespace HsCommData {

inline bool operator==(int a, const HsTuple& b)
{
    return HsTuple(a) == b;
}
/** @brief 左操作数 int64 的 == */
inline bool operator==(int64_t a, const HsTuple& b)
{
    return HsTuple(a) == b;
}
/** @brief 左操作数 double 的 == */
inline bool operator==(double a, const HsTuple& b)
{
    return HsTuple(a) == b;
}
/** @brief 左操作数 C 字符串的 ==。空指针当空串 */
inline bool operator==(const char* a, const HsTuple& b)
{
    return HsTuple(a) == b;
}
/** @brief 左操作数 string 的 == */
inline bool operator==(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) == b;
}
/** @brief 右操作数 int 的 == */
inline bool operator==(const HsTuple& a, int b)
{
    return a == HsTuple(b);
}
/** @brief 右操作数 int64 的 == */
inline bool operator==(const HsTuple& a, int64_t b)
{
    return a == HsTuple(b);
}
/** @brief 右操作数 double 的 == */
inline bool operator==(const HsTuple& a, double b)
{
    return a == HsTuple(b);
}
/** @brief 右操作数 C 字符串的 ==。空指针当空串 */
inline bool operator==(const HsTuple& a, const char* b)
{
    return a == HsTuple(b);
}
/** @brief 右操作数 string 的 == */
inline bool operator==(const HsTuple& a, const std::string& b)
{
    return a == HsTuple(b);
}

/** @brief 左操作数 int 的 != */
inline bool operator!=(int a, const HsTuple& b)
{
    return HsTuple(a) != b;
}
/** @brief 左操作数 int64 的 != */
inline bool operator!=(int64_t a, const HsTuple& b)
{
    return HsTuple(a) != b;
}
/** @brief 左操作数 double 的 != */
inline bool operator!=(double a, const HsTuple& b)
{
    return HsTuple(a) != b;
}
/** @brief 左操作数 C 字符串的 !=。空指针当空串 */
inline bool operator!=(const char* a, const HsTuple& b)
{
    return HsTuple(a) != b;
}
/** @brief 左操作数 string 的 != */
inline bool operator!=(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) != b;
}
/** @brief 右操作数 int 的 != */
inline bool operator!=(const HsTuple& a, int b)
{
    return a != HsTuple(b);
}
/** @brief 右操作数 int64 的 != */
inline bool operator!=(const HsTuple& a, int64_t b)
{
    return a != HsTuple(b);
}
/** @brief 右操作数 double 的 != */
inline bool operator!=(const HsTuple& a, double b)
{
    return a != HsTuple(b);
}
/** @brief 右操作数 C 字符串的 !=。空指针当空串 */
inline bool operator!=(const HsTuple& a, const char* b)
{
    return a != HsTuple(b);
}
/** @brief 右操作数 string 的 != */
inline bool operator!=(const HsTuple& a, const std::string& b)
{
    return a != HsTuple(b);
}

/** @brief 左操作数 int 的 < */
inline bool operator<(int a, const HsTuple& b)
{
    return HsTuple(a) < b;
}
/** @brief 左操作数 int64 的 < */
inline bool operator<(int64_t a, const HsTuple& b)
{
    return HsTuple(a) < b;
}
/** @brief 左操作数 double 的 < */
inline bool operator<(double a, const HsTuple& b)
{
    return HsTuple(a) < b;
}
/** @brief 左操作数 C 字符串的 <。空指针当空串 */
inline bool operator<(const char* a, const HsTuple& b)
{
    return HsTuple(a) < b;
}
/** @brief 左操作数 string 的 < */
inline bool operator<(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) < b;
}
/** @brief 右操作数 int 的 < */
inline bool operator<(const HsTuple& a, int b)
{
    return a < HsTuple(b);
}
/** @brief 右操作数 int64 的 < */
inline bool operator<(const HsTuple& a, int64_t b)
{
    return a < HsTuple(b);
}
/** @brief 右操作数 double 的 < */
inline bool operator<(const HsTuple& a, double b)
{
    return a < HsTuple(b);
}
/** @brief 右操作数 C 字符串的 <。空指针当空串 */
inline bool operator<(const HsTuple& a, const char* b)
{
    return a < HsTuple(b);
}
/** @brief 右操作数 string 的 < */
inline bool operator<(const HsTuple& a, const std::string& b)
{
    return a < HsTuple(b);
}

/** @brief 左操作数 int 的 > */
inline bool operator>(int a, const HsTuple& b)
{
    return HsTuple(a) > b;
}
/** @brief 左操作数 int64 的 > */
inline bool operator>(int64_t a, const HsTuple& b)
{
    return HsTuple(a) > b;
}
/** @brief 左操作数 double 的 > */
inline bool operator>(double a, const HsTuple& b)
{
    return HsTuple(a) > b;
}
/** @brief 左操作数 C 字符串的 >。空指针当空串 */
inline bool operator>(const char* a, const HsTuple& b)
{
    return HsTuple(a) > b;
}
/** @brief 左操作数 string 的 > */
inline bool operator>(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) > b;
}
/** @brief 右操作数 int 的 > */
inline bool operator>(const HsTuple& a, int b)
{
    return a > HsTuple(b);
}
/** @brief 右操作数 int64 的 > */
inline bool operator>(const HsTuple& a, int64_t b)
{
    return a > HsTuple(b);
}
/** @brief 右操作数 double 的 > */
inline bool operator>(const HsTuple& a, double b)
{
    return a > HsTuple(b);
}
/** @brief 右操作数 C 字符串的 >。空指针当空串 */
inline bool operator>(const HsTuple& a, const char* b)
{
    return a > HsTuple(b);
}
/** @brief 右操作数 string 的 > */
inline bool operator>(const HsTuple& a, const std::string& b)
{
    return a > HsTuple(b);
}

/** @brief 左操作数 int 的 <= */
inline bool operator<=(int a, const HsTuple& b)
{
    return HsTuple(a) <= b;
}
/** @brief 左操作数 int64 的 <= */
inline bool operator<=(int64_t a, const HsTuple& b)
{
    return HsTuple(a) <= b;
}
/** @brief 左操作数 double 的 <= */
inline bool operator<=(double a, const HsTuple& b)
{
    return HsTuple(a) <= b;
}
/** @brief 左操作数 C 字符串的 <=。空指针当空串 */
inline bool operator<=(const char* a, const HsTuple& b)
{
    return HsTuple(a) <= b;
}
/** @brief 左操作数 string 的 <= */
inline bool operator<=(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) <= b;
}
/** @brief 右操作数 int 的 <= */
inline bool operator<=(const HsTuple& a, int b)
{
    return a <= HsTuple(b);
}
/** @brief 右操作数 int64 的 <= */
inline bool operator<=(const HsTuple& a, int64_t b)
{
    return a <= HsTuple(b);
}
/** @brief 右操作数 double 的 <= */
inline bool operator<=(const HsTuple& a, double b)
{
    return a <= HsTuple(b);
}
/** @brief 右操作数 C 字符串的 <=。空指针当空串 */
inline bool operator<=(const HsTuple& a, const char* b)
{
    return a <= HsTuple(b);
}
/** @brief 右操作数 string 的 <= */
inline bool operator<=(const HsTuple& a, const std::string& b)
{
    return a <= HsTuple(b);
}

/** @brief 左操作数 int 的 >= */
inline bool operator>=(int a, const HsTuple& b)
{
    return HsTuple(a) >= b;
}
/** @brief 左操作数 int64 的 >= */
inline bool operator>=(int64_t a, const HsTuple& b)
{
    return HsTuple(a) >= b;
}
/** @brief 左操作数 double 的 >= */
inline bool operator>=(double a, const HsTuple& b)
{
    return HsTuple(a) >= b;
}
/** @brief 左操作数 C 字符串的 >=。空指针当空串 */
inline bool operator>=(const char* a, const HsTuple& b)
{
    return HsTuple(a) >= b;
}
/** @brief 左操作数 string 的 >= */
inline bool operator>=(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) >= b;
}
/** @brief 右操作数 int 的 >= */
inline bool operator>=(const HsTuple& a, int b)
{
    return a >= HsTuple(b);
}
/** @brief 右操作数 int64 的 >= */
inline bool operator>=(const HsTuple& a, int64_t b)
{
    return a >= HsTuple(b);
}
/** @brief 右操作数 double 的 >= */
inline bool operator>=(const HsTuple& a, double b)
{
    return a >= HsTuple(b);
}
/** @brief 右操作数 C 字符串的 >=。空指针当空串 */
inline bool operator>=(const HsTuple& a, const char* b)
{
    return a >= HsTuple(b);
}
/** @brief 右操作数 string 的 >= */
inline bool operator>=(const HsTuple& a, const std::string& b)
{
    return a >= HsTuple(b);
}

/** @brief 左操作数 int 的 +。先收成单元素元组 */
inline HsTuple operator+(int a, const HsTuple& b)
{
    return HsTuple(a) + b;
}
/** @brief 左操作数 int64 的 + */
inline HsTuple operator+(int64_t a, const HsTuple& b)
{
    return HsTuple(a) + b;
}
/** @brief 左操作数 double 的 + */
inline HsTuple operator+(double a, const HsTuple& b)
{
    return HsTuple(a) + b;
}
/** @brief 左操作数 C 字符串的 +。空指针当空串 */
inline HsTuple operator+(const char* a, const HsTuple& b)
{
    return HsTuple(a) + b;
}
/** @brief 左操作数 string 的 + */
inline HsTuple operator+(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) + b;
}
/** @brief 右操作数 int 的 + */
inline HsTuple operator+(const HsTuple& a, int b)
{
    return a + HsTuple(b);
}
/** @brief 右操作数 int64 的 + */
inline HsTuple operator+(const HsTuple& a, int64_t b)
{
    return a + HsTuple(b);
}
/** @brief 右操作数 double 的 + */
inline HsTuple operator+(const HsTuple& a, double b)
{
    return a + HsTuple(b);
}
/** @brief 右操作数 C 字符串的 +。空指针当空串 */
inline HsTuple operator+(const HsTuple& a, const char* b)
{
    return a + HsTuple(b);
}
/** @brief 右操作数 string 的 + */
inline HsTuple operator+(const HsTuple& a, const std::string& b)
{
    return a + HsTuple(b);
}

/** @brief 左操作数 int 的 - */
inline HsTuple operator-(int a, const HsTuple& b)
{
    return HsTuple(a) - b;
}
/** @brief 左操作数 int64 的 - */
inline HsTuple operator-(int64_t a, const HsTuple& b)
{
    return HsTuple(a) - b;
}
/** @brief 左操作数 double 的 - */
inline HsTuple operator-(double a, const HsTuple& b)
{
    return HsTuple(a) - b;
}
/** @brief 左操作数 C 字符串的 -。空指针当空串；字符串减数字会抛类型错 */
inline HsTuple operator-(const char* a, const HsTuple& b)
{
    return HsTuple(a) - b;
}
/** @brief 左操作数 string 的 - */
inline HsTuple operator-(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) - b;
}
/** @brief 右操作数 int 的 - */
inline HsTuple operator-(const HsTuple& a, int b)
{
    return a - HsTuple(b);
}
/** @brief 右操作数 int64 的 - */
inline HsTuple operator-(const HsTuple& a, int64_t b)
{
    return a - HsTuple(b);
}
/** @brief 右操作数 double 的 - */
inline HsTuple operator-(const HsTuple& a, double b)
{
    return a - HsTuple(b);
}
/** @brief 右操作数 C 字符串的 -。空指针当空串 */
inline HsTuple operator-(const HsTuple& a, const char* b)
{
    return a - HsTuple(b);
}
/** @brief 右操作数 string 的 - */
inline HsTuple operator-(const HsTuple& a, const std::string& b)
{
    return a - HsTuple(b);
}

/** @brief 左操作数 int 的 * */
inline HsTuple operator*(int a, const HsTuple& b)
{
    return HsTuple(a) * b;
}
/** @brief 左操作数 int64 的 * */
inline HsTuple operator*(int64_t a, const HsTuple& b)
{
    return HsTuple(a) * b;
}
/** @brief 左操作数 double 的 * */
inline HsTuple operator*(double a, const HsTuple& b)
{
    return HsTuple(a) * b;
}
/** @brief 左操作数 C 字符串的 *。空指针当空串；字符串乘数字会抛类型错 */
inline HsTuple operator*(const char* a, const HsTuple& b)
{
    return HsTuple(a) * b;
}
/** @brief 左操作数 string 的 * */
inline HsTuple operator*(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) * b;
}
/** @brief 右操作数 int 的 * */
inline HsTuple operator*(const HsTuple& a, int b)
{
    return a * HsTuple(b);
}
/** @brief 右操作数 int64 的 * */
inline HsTuple operator*(const HsTuple& a, int64_t b)
{
    return a * HsTuple(b);
}
/** @brief 右操作数 double 的 * */
inline HsTuple operator*(const HsTuple& a, double b)
{
    return a * HsTuple(b);
}
/** @brief 右操作数 C 字符串的 *。空指针当空串 */
inline HsTuple operator*(const HsTuple& a, const char* b)
{
    return a * HsTuple(b);
}
/** @brief 右操作数 string 的 * */
inline HsTuple operator*(const HsTuple& a, const std::string& b)
{
    return a * HsTuple(b);
}

/** @brief 左操作数 int 的 / */
inline HsTuple operator/(int a, const HsTuple& b)
{
    return HsTuple(a) / b;
}
/** @brief 左操作数 int64 的 / */
inline HsTuple operator/(int64_t a, const HsTuple& b)
{
    return HsTuple(a) / b;
}
/** @brief 左操作数 double 的 / */
inline HsTuple operator/(double a, const HsTuple& b)
{
    return HsTuple(a) / b;
}
/** @brief 左操作数 C 字符串的 /。空指针当空串；字符串除数字会抛类型错 */
inline HsTuple operator/(const char* a, const HsTuple& b)
{
    return HsTuple(a) / b;
}
/** @brief 左操作数 string 的 / */
inline HsTuple operator/(const std::string& a, const HsTuple& b)
{
    return HsTuple(a) / b;
}
/** @brief 右操作数 int 的 / */
inline HsTuple operator/(const HsTuple& a, int b)
{
    return a / HsTuple(b);
}
/** @brief 右操作数 int64 的 / */
inline HsTuple operator/(const HsTuple& a, int64_t b)
{
    return a / HsTuple(b);
}
/** @brief 右操作数 double 的 / */
inline HsTuple operator/(const HsTuple& a, double b)
{
    return a / HsTuple(b);
}
/** @brief 右操作数 C 字符串的 /。空指针当空串 */
inline HsTuple operator/(const HsTuple& a, const char* b)
{
    return a / HsTuple(b);
}
/** @brief 右操作数 string 的 / */
inline HsTuple operator/(const HsTuple& a, const std::string& b)
{
    return a / HsTuple(b);
}

}

#endif
