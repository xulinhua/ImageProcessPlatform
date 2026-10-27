/**
 * @file HsMat.h
 * @brief HsMat头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSMAT_H
#define HSCOMMDATA_HSMAT_H

#include <HsCommData/HsCommDataExport.h>
#include <HsCommData/HsTuple.h>

#include <cstdint>
#include <memory>

namespace HsCommData {

/**
 * @class HsMat
 * @brief 双精度矩阵。行优先、紧凑存放。拷贝共享缓冲，写入时才复制
 * 不是 HsTuple 的元素。要进元组口时用 toTuple，读回用 fromTuple。
 */
class HSCOMMDATA_EXPORT HsMat {
public:
    HsMat(); ///< 空矩阵，0 行 0 列
    /** @brief 同类型拷贝，只共享缓冲 */
    HsMat(const HsMat& other);
    /** @brief 同类型赋值，只共享缓冲。深拷用 clone */
    HsMat& operator=(const HsMat& other);
    /**
     * @brief rows 行 cols 列，元素为 0
     * 任一边小于 0 抛 HsException
     */
    HsMat(int rows, int cols);
    /**
     * @brief 从行优先数组拷入。data 为空且行列都为 0 则空矩阵
     * 个数须为 rows * cols。任一边小于 0，或 data 为空但元素个数大于 0，抛 HsException
     */
    HsMat(int rows, int cols, const double* data);

    /** @brief n 阶单位阵。n 小于 1 抛 HsException */
    static HsMat eye(int n);
    /**
     * @brief 从纯数字元组还原。长度须等于 rows * cols，按行优先
     * 含字符串或长度不符抛 HsException
     */
    static HsMat fromTuple(const HsTuple& tuple, int rows, int cols);

    /** @brief 无元素 */
    bool isEmpty() const;
    /** @brief 行数。空矩阵为 0 */
    int rows() const;
    /** @brief 列数。空矩阵为 0 */
    int cols() const;
    /** @brief 元素个数，等于 rows * cols */
    int length() const;

    /**
     * @brief 第 row 行第 col 列。越界抛 HsException
     */
    double at(int row, int col) const;
    /**
     * @brief 改一格。越界抛 HsException。共享时先拆开
     */
    void set(int row, int col, double value);

    /**
     * @brief 只读行优先缓冲。空矩阵返回 0
     * 写入会拆开共享，指针失效
     */
    const double* data() const;
    /**
     * @brief 可写行优先缓冲。空矩阵抛 HsException
     * 共享时先拆开
     */
    double* data();

    /** @brief 深拷全部元素 */
    HsMat clone() const;
    /** @brief 排成 TupleDouble，行优先。空矩阵得到空元组 */
    HsTuple toTuple() const;

    /** @brief 行列与对应元素都相同 */
    bool operator==(const HsMat& other) const;
    /** @brief 与 operator== 相反 */
    bool operator!=(const HsMat& other) const;

private:
    struct Body;
    /** @brief 确保已分配缓冲 */
    void ensure(int rows, int cols);
    /** @brief 若被共享则拆开拷贝 */
    void detach();
    /** @brief 下标收到线性位置。越界抛 */
    int indexOf(int row, int col) const;

    std::shared_ptr<Body> m_body; ///< 元素缓冲。空指针表示空矩阵
};

}

#endif