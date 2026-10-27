/**
 * @file HsPoly.cpp
 * @brief HsPoly实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsPoly.h>
#include <cstring>
#include <HsCommData/GeomOps.h>

namespace HsCommData {

// 下标未指定
PolySeg::PolySeg()
    : first(-1)
    , last(-1)
{
}

// 指定顶点与轮廓点串上的闭区间下标
PolySeg::PolySeg(const Point2d& pt_, int first_, int last_)
    : pt(pt_)
    , first(first_)
    , last(last_)
{
}

// 空多边形，不闭合
HsPoly::HsPoly()
    : closed(false)
    , m_segs(nullptr)
    , m_segCount(0)
    , m_segCap(0)
{
}

// 深拷贝段表与闭合标志
HsPoly::HsPoly(const HsPoly& other)
    : closed(other.closed)
    , m_segs(nullptr)
    , m_segCount(0)
    , m_segCap(0)
{
    copySegs(other.m_segs, other.m_segCount);
}

// 深拷赋值
HsPoly& HsPoly::operator=(const HsPoly& other)
{
    if (this != &other) {
        closed = other.closed;
        copySegs(other.m_segs, other.m_segCount);
    }
    return *this;
}
// 释放段缓冲
HsPoly::~HsPoly()
{
    releaseSegs();
}
// 段数
size_t HsPoly::segCount() const
{
    return m_segCount;
}

// 没有控制段
bool HsPoly::segsEmpty() const
{
    return m_segCount == 0;
}

// 第 i 段
PolySeg& HsPoly::segAt(size_t i)
{
    return m_segs[i];
}

// 第 i 段
const PolySeg& HsPoly::segAt(size_t i) const
{
    return m_segs[i];
}


// 追加一段
void HsPoly::pushSeg(const PolySeg& seg)
{
    ensureSegs(m_segCount + 1);
    m_segs[m_segCount] = seg;
    ++m_segCount;
}

// 改段数。变长的新段下标为 -1
void HsPoly::resizeSegs(size_t n)
{
    if (n > m_segCount) {
        ensureSegs(n);
        for (size_t i = m_segCount; i < n; ++i) {
            m_segs[i].pt.x = 0;
            m_segs[i].pt.y = 0;
            m_segs[i].first = -1;
            m_segs[i].last = -1;
        }
    }
    m_segCount = n;
}

// 按段数分配，不逐段构造
void HsPoly::allocSegs(size_t n)
{
    if (n > m_segCap) {
        PolySeg* grown = static_cast<PolySeg*>(::operator new(n * sizeof(PolySeg)));
        if (m_segs) {
            ::operator delete(m_segs);
            m_segs = nullptr;
        }
        m_segs = grown;
        m_segCap = n;
    }
    m_segCount = n;
}

// 容量不足时换一块更大的缓冲，旧段整段搬过去
void HsPoly::ensureSegs(size_t need)
{
    if (need <= m_segCap) {
        return;
    }
    size_t cap = m_segCap == 0 ? 8 : m_segCap;
    while (cap < need) {
        cap *= 2;
    }
    PolySeg* grown = static_cast<PolySeg*>(::operator new(cap * sizeof(PolySeg)));
    if (m_segCount > 0) {
        std::memcpy(grown, m_segs, m_segCount * sizeof(PolySeg));
    }
    if (m_segs) {
        ::operator delete(m_segs);
        m_segs = nullptr;
    }
    m_segs = grown;
    m_segCap = cap;
}

// 释放段缓冲
void HsPoly::releaseSegs()
{
    if (m_segs) {
        ::operator delete(m_segs);
        m_segs = nullptr;
    }
    m_segCount = 0;
    m_segCap = 0;
}

// 按段数整段复制
void HsPoly::copySegs(const PolySeg* src, size_t n)
{
    allocSegs(n);
    if (n > 0) {
        std::memcpy(m_segs, src, n * sizeof(PolySeg));
    }
}

// 无顶点段
bool HsPoly::isEmpty() const
{
    return m_segCount == 0;
}

// 轴对齐包围盒
Box2 HsPoly::bounds() const
{
    if (m_segCount == 0) {
        return Box2();
    }
    Box2 r(m_segs[0].pt.x, m_segs[0].pt.y, 0.0, 0.0);
    for (size_t i = 1; i < m_segCount; ++i) {
        r = united(r, Box2(m_segs[i].pt.x, m_segs[i].pt.y, 0.0, 0.0));
    }
    return r;
}

// 各段下标是否落在轮廓点数范围内
bool HsPoly::fits(const HsContour& contour) const
{
    const int n = static_cast<int>(contour.ptCount());
    if (m_segCount == 0 || n <= 0) {
        return false;
    }
    for (size_t i = 0; i < m_segCount; ++i) {
        const int a = m_segs[i].first;
        const int b = m_segs[i].last;
        if (a < 0 || b < a || b >= n) {
            return false;
        }
    }
    return true;
}

}
