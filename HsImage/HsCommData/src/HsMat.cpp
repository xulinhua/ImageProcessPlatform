/**
 * @file HsMat.cpp
 * @brief HsMat实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsMat.h>
#include <HsCommData/HsException.h>

#include <cstring>
#include <vector>

namespace HsCommData {

struct HsMat::Body {
    int rows;                    ///< 行数
    int cols;                    ///< 列数
    std::vector<double> values;  ///< 行优先，长度等于 rows * cols

    Body()
        : rows(0)
        , cols(0)
    {
    }
};

// 空矩阵
HsMat::HsMat()
{
}

// 共享缓冲
HsMat::HsMat(const HsMat& other)
    : m_body(other.m_body)
{
}

// 共享缓冲
HsMat& HsMat::operator=(const HsMat& other)
{
    if (this != &other) {
        m_body = other.m_body;
    }
    return *this;
}

// 全 0
HsMat::HsMat(int rows, int cols)
{
    ensure(rows, cols);
}

// 从外部数组拷入
HsMat::HsMat(int rows, int cols, const double* data)
{
    if (rows < 0 || cols < 0) {
        raiseException(HsException("HsMat", HS_ERR_TYPE));
    }
    if (rows == 0 || cols == 0) {
        if (rows != 0 || cols != 0) {
            raiseException(HsException("HsMat", HS_ERR_TYPE));
        }
        return;
    }
    if (!data) {
        raiseException(HsException("HsMat", HS_ERR_TYPE));
    }
    ensure(rows, cols);
    const int n = rows * cols;
    std::memcpy(&m_body->values[0], data, static_cast<size_t>(n) * sizeof(double));
}

// 单位阵
HsMat HsMat::eye(int n)
{
    if (n < 1) {
        raiseException(HsException("HsMat::eye", HS_ERR_TYPE));
    }
    HsMat m(n, n);
    double* p = m.data();
    for (int i = 0; i < n; ++i) {
        p[i * n + i] = 1.0;
    }
    return m;
}

// 元组还原
HsMat HsMat::fromTuple(const HsTuple& tuple, int rows, int cols)
{
    if (rows < 0 || cols < 0 || (rows == 0) != (cols == 0)) {
        raiseException(HsException("HsMat::fromTuple", HS_ERR_TYPE));
    }
    if (rows == 0) {
        if (!tuple.isEmpty()) {
            raiseException(HsException("HsMat::fromTuple", HS_ERR_TYPE));
        }
        return HsMat();
    }
    const int n = rows * cols;
    if (tuple.length() != n) {
        raiseException(HsException("HsMat::fromTuple", HS_ERR_TYPE));
    }
    HsMat m(rows, cols);
    double* p = m.data();
    for (int i = 0; i < n; ++i) {
        double d = 0.0;
        int64_t li = 0;
        if (tuple.getDouble(i, &d)) {
            p[i] = d;
        } else if (tuple.getLong(i, &li)) {
            p[i] = static_cast<double>(li);
        } else {
            raiseException(HsException("HsMat::fromTuple", HS_ERR_TYPE));
        }
    }
    return m;
}

// 无元素
bool HsMat::isEmpty() const
{
    return !m_body || m_body->values.empty();
}

// 行数
int HsMat::rows() const
{
    return m_body ? m_body->rows : 0;
}

// 列数
int HsMat::cols() const
{
    return m_body ? m_body->cols : 0;
}

// 元素个数
int HsMat::length() const
{
    return m_body ? static_cast<int>(m_body->values.size()) : 0;
}

// 读一格
double HsMat::at(int row, int col) const
{
    return m_body->values[static_cast<size_t>(indexOf(row, col))];
}

// 写一格
void HsMat::set(int row, int col, double value)
{
    const int i = indexOf(row, col);
    detach();
    m_body->values[static_cast<size_t>(i)] = value;
}

// 只读缓冲
const double* HsMat::data() const
{
    if (!m_body || m_body->values.empty()) {
        return 0;
    }
    return &m_body->values[0];
}

// 可写缓冲
double* HsMat::data()
{
    if (!m_body || m_body->values.empty()) {
        raiseException(HsException("HsMat::data", HS_ERR_TYPE));
    }
    detach();
    return &m_body->values[0];
}

// 深拷
HsMat HsMat::clone() const
{
    HsMat out;
    if (!m_body) {
        return out;
    }
    out.m_body.reset(new Body());
    out.m_body->rows = m_body->rows;
    out.m_body->cols = m_body->cols;
    out.m_body->values = m_body->values;
    return out;
}

// 排成浮点元组
HsTuple HsMat::toTuple() const
{
    if (!m_body || m_body->values.empty()) {
        return HsTuple();
    }
    return HsTuple(&m_body->values[0], static_cast<int>(m_body->values.size()));
}

// 行列和元素都相同
bool HsMat::operator==(const HsMat& other) const
{
    if (rows() != other.rows() || cols() != other.cols()) {
        return false;
    }
    if (length() == 0) {
        return true;
    }
    const double* a = data();
    const double* b = other.data();
    for (int i = 0; i < length(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

// 不等
bool HsMat::operator!=(const HsMat& other) const
{
    return !(*this == other);
}

// 分配全 0 缓冲
void HsMat::ensure(int rows, int cols)
{
    if (rows < 0 || cols < 0 || (rows == 0) != (cols == 0)) {
        raiseException(HsException("HsMat", HS_ERR_TYPE));
    }
    if (rows == 0) {
        m_body.reset();
        return;
    }
    m_body.reset(new Body());
    m_body->rows = rows;
    m_body->cols = cols;
    m_body->values.assign(static_cast<size_t>(rows) * static_cast<size_t>(cols), 0.0);
}

// 共享则拆开
void HsMat::detach()
{
    if (!m_body || m_body.use_count() <= 1) {
        return;
    }
    std::shared_ptr<Body> copy(new Body());
    copy->rows = m_body->rows;
    copy->cols = m_body->cols;
    copy->values = m_body->values;
    m_body = copy;
}

// 行列换成线性下标
int HsMat::indexOf(int row, int col) const
{
    if (!m_body || row < 0 || col < 0 || row >= m_body->rows || col >= m_body->cols) {
        raiseException(HsException("HsMat::at", HS_ERR_INDEX));
    }
    return row * m_body->cols + col;
}

}