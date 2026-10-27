/**
 * @file HsTuple.h
 * @brief HsTuple头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSTUPLE_H
#define HSCOMMDATA_HSTUPLE_H

#include <HsCommData/Geom.h>

#include <cstdint>
#include <memory>
#include <string>

namespace HsCommData {

/** @brief 元组的存储种类。混合之前只有一种数组 */
enum TupleType {
    TupleEmpty = 0, ///< 空
    TupleLong,      ///< 仅 int64
    TupleDouble,    ///< 仅 double
    TupleString,    ///< 仅字符串
    TupleMixed      ///< 元素类型可以不同
};

/** @brief 混合元组里一个元素的种类 */
enum TupleElemKind {
    ElemLong = 0, ///< 长整数
    ElemDouble,   ///< 浮点
    ElemString    ///< 字符串
};

/**
 * @class TupleElem
 * @brief 混合元组的一个元素。num 里整数和浮点共用存储，字符串另放
 */
struct HSCOMMDATA_EXPORT TupleElem {
    TupleElemKind kind; ///< 本元素种类
    union {
        int64_t i; ///< ElemLong
        double d;  ///< ElemDouble
    } num;
    std::string text; ///< ElemString 时有效

    TupleElem(); ///< 整数 0
};

/**
 * @class HsTuple
 * @brief 控制参数元组。拷贝共享缓冲，写入时才复制
 * 纯整数或纯浮点可拿连续指针。类型不符返回 0。可用整数、浮点或字符串直接构造和赋值，结果只含这一个
 */
class HSCOMMDATA_EXPORT HsTuple {
public:
    HsTuple(); ///< 空
    /** @brief 同类型拷贝，只共享缓冲 */
    HsTuple(const HsTuple& other);
    /** @brief 同类型赋值，只共享缓冲。深拷用 clone */
    HsTuple& operator=(const HsTuple& other);
    HsTuple(int64_t value); ///< 一个整数
    HsTuple(int value); ///< 一个 32 位整数，避免和浮点构造二义
    HsTuple(double value); ///< 一个浮点
    HsTuple(const char* value); ///< 一个字符串。空指针当空串
    HsTuple(const std::string& value); ///< 一个字符串
    HsTuple(const int64_t* data, int count); ///< 整数数组。count<=0 或 data 为空则空元组
    HsTuple(const double* data, int count); ///< 浮点数组

    /** @brief 换成这一个整数，不追加 */
    HsTuple& operator=(int value);
    /** @brief 换成这一个整数，不追加 */
    HsTuple& operator=(int64_t value);
    /** @brief 换成这一个浮点，不追加 */
    HsTuple& operator=(double value);
    /** @brief 换成这一个字符串。空指针当空串，不追加 */
    HsTuple& operator=(const char* value);
    /** @brief 换成这一个字符串，不追加 */
    HsTuple& operator=(const std::string& value);

    /**
     * @brief 取出一格，结果是新的单元素元组。越界抛 HsException
     * 改某一格用 setLong、setDouble、setString，写入前会拆开共享缓冲
     */
    HsTuple operator[](int index) const;

    /** @brief 第 0 个 32 位整数。类型不符、越界或超出 int 抛 HsException */
    int I() const;
    /** @brief 第 index 个 32 位整数。类型不符、越界或超出 int 抛 HsException */
    int I(int index) const;
    /** @brief 第 0 个长整数。类型不符或越界抛 HsException */
    int64_t L() const;
    /** @brief 第 index 个长整数。类型不符或越界抛 HsException */
    int64_t L(int index) const;
    /** @brief 第 0 个浮点。类型不符或越界抛 HsException */
    double D() const;
    /** @brief 第 index 个浮点。类型不符或越界抛 HsException */
    double D(int index) const;
    /** @brief 第 0 个字符串。类型不符或越界抛 HsException */
    std::string S() const;
    /** @brief 第 index 个字符串。类型不符或越界抛 HsException */
    std::string S(int index) const;
    /**
     * @brief 第 0 个字符串的 C 指针。类型不符或越界抛 HsException
     * 对象改动或析构后指针失效
     */
    const char* C() const;
    /**
     * @brief 第 index 个字符串的 C 指针。类型不符或越界抛 HsException
     * 对象改动或析构后指针失效
     */
    const char* C(int index) const;

    /** @brief 无元素 */
    bool isEmpty() const;
    /** @brief 元素个数 */
    int length() const;
    /** @brief 当前存储种类 */
    TupleType type() const;

    /** @brief 清空，未分配缓冲 */
    void clear();
    /** @brief 深拷全部元素 */
    HsTuple clone() const;

    /** @brief 末尾追加一个整数。必要时升成混合 */
    void append(int64_t value);
    /** @brief 末尾追加一个浮点。必要时升成混合 */
    void append(double value);
    /** @brief 末尾追加一个字符串。空指针当空串 */
    void append(const char* value);
    /** @brief 末尾追加一个字符串 */
    void append(const std::string& value);
    /** @brief 末尾追加整个元组 */
    void append(const HsTuple& other);

    /** @brief 试取整数。失败返回 false，不抛 */
    bool getLong(int index, int64_t* out) const;
    /** @brief 试取浮点。失败返回 false，不抛 */
    bool getDouble(int index, double* out) const;
    /** @brief 试取字符串。失败返回 false，不抛 */
    bool getString(int index, std::string* out) const;

    /**
     * @brief 只读连续整数缓冲。非纯整数抛 HsException，空元组返回 0
     * 写入会拆开共享，指针失效
     */
    const int64_t* longData() const;
    /** @brief 可写连续整数缓冲。非纯整数或空抛 HsException */
    int64_t* longData();
    /** @brief 只读连续浮点缓冲。非纯浮点抛 HsException，空元组返回 0 */
    const double* doubleData() const;
    /** @brief 可写连续浮点缓冲。非纯浮点或空抛 HsException */
    double* doubleData();

    /** @brief 改第 index 个为整数。越界抛 HsException。必要时升混合 */
    bool setLong(int index, int64_t value);
    /** @brief 改第 index 个为浮点。越界抛 HsException */
    bool setDouble(int index, double value);
    /** @brief 改第 index 个为字符串。越界抛 HsException */
    bool setString(int index, const std::string& value);

    /**
     * @brief 整组相等，对应 Halcon tuple_equal / ==
     * 个数相同，且对应元素值相同。整数和浮点按数值比，1 与 1.0 相等。
     * 数字和字符串不是相等，不抛异常
     */
    bool operator==(const HsTuple& other) const;
    /** @brief 整组不等，对应 tuple_not_equal / != */
    bool operator!=(const HsTuple& other) const;
    /**
     * @brief 字典序小于，对应 tuple_less / <
     * 前缀相同则更短的更小。空元组小于非空。数字与字符串不能比，抛 HS_ERR_TYPE
     */
    bool operator<(const HsTuple& other) const;
    /** @brief 字典序大于，对应 tuple_greater / > */
    bool operator>(const HsTuple& other) const;
    /** @brief 字典序小于等于，对应 tuple_less_equal / <= */
    bool operator<=(const HsTuple& other) const;
    /** @brief 字典序大于等于，对应 tuple_greater_equal / >= */
    bool operator>=(const HsTuple& other) const;

    /**
     * @brief 逐元素相等，对应 tuple_equal_elem
     * 长度相同逐项比，一边只有 1 个则广播到另一边。
     * 数字和字符串不算相等，该项为 0，不抛异常。
     * 结果为 0/1 整数元组
     */
    HsTuple equalElem(const HsTuple& other) const;
    /** @brief 逐元素不等，对应 tuple_not_equal_elem。规则同 equalElem */
    HsTuple notEqualElem(const HsTuple& other) const;
    /**
     * @brief 逐元素小于，对应 tuple_less_elem
     * 类型不能比时抛 HS_ERR_TYPE。结果为 0/1 整数元组
     */
    HsTuple lessElem(const HsTuple& other) const;
    /** @brief 逐元素大于，对应 tuple_greater_elem */
    HsTuple greaterElem(const HsTuple& other) const;
    /** @brief 逐元素小于等于，对应 tuple_less_equal_elem */
    HsTuple lessEqualElem(const HsTuple& other) const;
    /** @brief 逐元素大于等于，对应 tuple_greater_equal_elem */
    HsTuple greaterEqualElem(const HsTuple& other) const;

    /**
     * @brief 逐元素加，对应 tuple_add / +
     * 长度相同逐项算，一边只有 1 个则广播。空与任意得空。
     * 整数与浮点按数值算，结果升到能容纳的类型。
     * 字符串参与加减乘除或类型不能算时抛 HS_ERR_TYPE
     */
    HsTuple operator+(const HsTuple& other) const;
    /**
     * @brief 逐元素减，对应 tuple_sub / -
     * 规则同加。类型不能算抛 HS_ERR_TYPE
     */
    HsTuple operator-(const HsTuple& other) const;
    /**
     * @brief 逐元素乘，对应 tuple_mult / *
     * 规则同加。类型不能算抛 HS_ERR_TYPE
     */
    HsTuple operator*(const HsTuple& other) const;
    /**
     * @brief 逐元素除，对应 tuple_div / /
     * 整数除整数向零取整。除数为 0 抛 HS_ERR_TYPE。字符串参与则抛
     */
    HsTuple operator/(const HsTuple& other) const;
    /** @brief 逐元素取负。字符串抛 HS_ERR_TYPE。空得空 */
    HsTuple operator-() const;

    /**
     * @brief 等差整数序列，对应 tuple_gen_sequence
     * start 到 end 含端点。step 为 0 抛 HS_ERR_TYPE。方向与 step 不符则为空
     */
    static HsTuple genSequence(int64_t start, int64_t end, int64_t step = 1);
    /**
     * @brief 重复同一整数 count 次，对应 tuple_gen_const。count<=0 为空
     */
    static HsTuple genConst(int count, int64_t value);
    /** @brief 重复同一浮点 count 次 */
    static HsTuple genConst(int count, double value);
    /** @brief 重复同一字符串 count 次。空指针当空串 */
    static HsTuple genConst(int count, const char* value);
    /** @brief 重复同一字符串 count 次 */
    static HsTuple genConst(int count, const std::string& value);

    /**
     * @brief 按下标挑选，对应 tuple_select。indices 元素须为整数下标
     */
    HsTuple select(const HsTuple& indices) const;
    /**
     * @brief 按 0/1 掩码挑选。掩码长度须一致，非 0 保留
     * equalElem 的结果可直接当掩码
     */
    HsTuple selectMask(const HsTuple& mask) const;
    /**
     * @brief 按下标删除。indices 元素须为整数下标，下标从大到小处理
     */
    void remove(const HsTuple& indices);
    /**
     * @brief 按掩码删除。非 0 位置删除
     */
    void removeMask(const HsTuple& mask);
    /**
     * @brief 在 index 前插入 values。index 等于 length() 则追加
     */
    void insert(int index, const HsTuple& values);
    /**
     * @brief 用 values 替换从 index 起。values 有几格就换几格
     */
    void replace(int index, const HsTuple& values);

    /**
     * @brief 求和，对应 tuple_sum。空为 0。字符串抛 HS_ERR_TYPE
     * 整数与浮点按数值累加，结果类型能容纳即可
     */
    HsTuple sum() const;
    /**
     * @brief 最小值，对应 tuple_min。空抛 HS_ERR_INDEX。字符串抛 HS_ERR_TYPE
     */
    HsTuple min() const;
    /**
     * @brief 最大值，对应 tuple_max。规则同 min
     */
    HsTuple max() const;
    /**
     * @brief 平均值，对应 tuple_mean。空或含字符串抛异常
     */
    HsTuple mean() const;

    /**
     * @brief 转整数元组，对应 tuple_int。向零取整。字符串抛 HS_ERR_TYPE
     */
    HsTuple toLong() const;
    /**
     * @brief 转浮点元组，对应 tuple_real。字符串抛 HS_ERR_TYPE
     */
    HsTuple toDouble() const;

    /**
     * @brief 逐元素绝对值，对应 tuple_abs。字符串抛 HS_ERR_TYPE
     */
    HsTuple abs() const;
    /**
     * @brief 逐元素取模，对应 tuple_mod / %
     * 规则同加。除数为 0 抛 HS_ERR_TYPE。广播同 +
     */
    HsTuple operator%(const HsTuple& other) const;

    /**
     * @brief 写出元组文件，对应 write_tuple
     * @param fileName 路径。空或打不开时抛 HsException
     */
    void writeTuple(const char* fileName) const;
    /**
     * @brief 读入元组文件，对应 read_tuple。覆盖本对象
     * @param fileName 路径
     */
    void readTuple(const char* fileName);

private:
    /** @brief 负下标收到 [0, length())。非法则抛 */
    int resolveIndex(int index) const;
    struct Body;

    /** @brief 确保已分配缓冲 */
    void ensure();
    /** @brief 若被共享则拆开拷贝 */
    void detach();
    /** @brief 纯数组升成混合 */
    static void promote(Body* body);
    /** @brief 追加一个混合元素 */
    static void pushMixed(Body* body, const TupleElem& elem);

    std::shared_ptr<Body> m_body; ///< 元素缓冲。空指针表示空元组
};

/**
 * @brief 写出元组文件，对应 write_tuple
 * @param tuple 待写元组
 * @param fileName 路径。空或打不开时抛 HsException
 */
HSCOMMDATA_EXPORT void writeTuple(const HsTuple& tuple, const char* fileName);
/**
 * @brief 读入元组文件，对应 read_tuple。写入 tuple
 * @param tuple 目标。空指针抛 HsException
 * @param fileName 路径
 */
HSCOMMDATA_EXPORT void readTuple(HsTuple* tuple, const char* fileName);

}

#include <HsCommData/HsTupleOps.h>

#endif
