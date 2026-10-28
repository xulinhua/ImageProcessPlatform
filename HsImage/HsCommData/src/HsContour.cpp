/**
 * @file HsContour.cpp
 * @brief HsContour实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsContour.h>
#include <cstring>
#include <HsCommData/HsInterface2D.h>
#include <HsCommData/HsException.h>
#include <HsCommData/GeomOps.h>

namespace HsCommData {

// 默认开折线
HsContour::HsContour()
    : closed(false)
    , m_pts(nullptr)
    , m_ptCount(0)
    , m_ptCap(0)
{
}

// 深拷：点列按个数整段复制，闭合标志和属性逐份拷贝
HsContour::HsContour(const HsContour& other)
    : closed(other.closed)
    , angle(other.angle)
    , response(other.response)
    , m_pts(nullptr)
    , m_ptCount(0)
    , m_ptCap(0)
{
    copyPts(other.m_pts, other.m_ptCount);
}

// 深拷赋值：点列按个数整段复制，闭合标志和属性逐份拷贝
HsContour& HsContour::operator=(const HsContour& other)
{
    if (this != &other) {
        closed = other.closed;
        angle = other.angle;
        response = other.response;
        copyPts(other.m_pts, other.m_ptCount);
    }
    return *this;
}
// 释放点缓冲
HsContour::~HsContour()
{
    releasePts();
}

// 点数
size_t HsContour::ptCount() const
{
    return m_ptCount;
}

// 没有点
bool HsContour::ptsEmpty() const
{
    return m_ptCount == 0;
}

// 第 i 个点
Point2d& HsContour::ptAt(size_t i)
{
    return m_pts[i];
}

// 第 i 个点
const Point2d& HsContour::ptAt(size_t i) const
{
    return m_pts[i];
}

// 空轮廓没有点。忽略或回调返回后给出静态原点，避免解引用空缓冲
static Point2d& emptyPoint()
{
    static Point2d point;
    point.x = 0;
    point.y = 0;
    return point;
}

// 第一个点。空轮廓交给系统异常处理
Point2d& HsContour::ptFront()
{
    if (m_ptCount == 0) {
        raiseException(HsException("HsContour::ptFront", HS_ERR_INDEX));
        return emptyPoint();
    }
    return m_pts[0];
}

// 第一个点。空轮廓交给系统异常处理
const Point2d& HsContour::ptFront() const
{
    if (m_ptCount == 0) {
        raiseException(HsException("HsContour::ptFront", HS_ERR_INDEX));
        return emptyPoint();
    }
    return m_pts[0];
}

// 最后一个点。空轮廓交给系统异常处理
Point2d& HsContour::ptBack()
{
    if (m_ptCount == 0) {
        raiseException(HsException("HsContour::ptBack", HS_ERR_INDEX));
        return emptyPoint();
    }
    return m_pts[m_ptCount - 1];
}

// 最后一个点。空轮廓交给系统异常处理
const Point2d& HsContour::ptBack() const
{
    if (m_ptCount == 0) {
        raiseException(HsException("HsContour::ptBack", HS_ERR_INDEX));
        return emptyPoint();
    }
    return m_pts[m_ptCount - 1];
}

// 追加一个点
void HsContour::pushPt(const Point2d& point)
{
    ensurePts(m_ptCount + 1);
    m_pts[m_ptCount] = point;
    ++m_ptCount;
}

// 改点数。变长的新点为原点
void HsContour::resizePts(size_t n)
{
    if (n > m_ptCount) {
        ensurePts(n);
        for (size_t i = m_ptCount; i < n; ++i) {
            m_pts[i].x = 0;
            m_pts[i].y = 0;
        }
    }
    m_ptCount = n;
}

// 用向量整段填入
void HsContour::setPts(const std::vector<Point2d>& src)
{
    copyPts(src.empty() ? nullptr : src.data(), src.size());
}

// 按个数分配，不逐点构造
void HsContour::allocPts(size_t n)
{
    if (n > m_ptCap) {
        Point2d* grown = static_cast<Point2d*>(::operator new(n * sizeof(Point2d)));
        if (m_pts) {
            ::operator delete(m_pts);
            m_pts = nullptr;
        }
        m_pts = grown;
        m_ptCap = n;
    }
    m_ptCount = n;
}

// 容量不足时换一块更大的缓冲，旧点整段搬过去
void HsContour::ensurePts(size_t need)
{
    if (need <= m_ptCap) {
        return;
    }
    size_t cap = m_ptCap == 0 ? 8 : m_ptCap;
    while (cap < need) {
        cap *= 2;
    }
    Point2d* grown = static_cast<Point2d*>(::operator new(cap * sizeof(Point2d)));
    if (m_ptCount > 0) {
        std::memcpy(grown, m_pts, m_ptCount * sizeof(Point2d));
    }
    if (m_pts) {
        ::operator delete(m_pts);
        m_pts = nullptr;
    }
    m_pts = grown;
    m_ptCap = cap;
}

// 释放点缓冲
void HsContour::releasePts()
{
    if (m_pts) {
        ::operator delete(m_pts);
        m_pts = nullptr;
    }
    m_ptCount = 0;
    m_ptCap = 0;
}

// 按个数整段复制
void HsContour::copyPts(const Point2d* src, size_t n)
{
    allocPts(n);
    if (n > 0) {
        std::memcpy(m_pts, src, n * sizeof(Point2d));
    }
}

// 无顶点则为空
bool HsContour::isEmpty() const
{
    return m_ptCount == 0;
}

// 顶点包围盒
Box2 HsContour::bounds() const
{
    if (m_ptCount == 0) {
        return Box2();
    }
    Box2 r(m_pts[0].x, m_pts[0].y, 0.0, 0.0);
    for (size_t i = 1; i < m_ptCount; ++i) {
        r = united(r, Box2(m_pts[i].x, m_pts[i].y, 0.0, 0.0));
    }
    return r;
}

// 角度或响应若给出，长度必须与点数相同
bool HsContour::attribsOk() const
{
    if (!angle.empty() && angle.size() != m_ptCount) {
        return false;
    }
    if (!response.empty() && response.size() != m_ptCount) {
        return false;
    }
    return true;
}
// 下标必须落在切向角数组内
static void checkAngleIndex(const HsContour& c, int index, const char* proc)
{
    if (index < 0 || index >= static_cast<int>(c.angle.size())) {
        throw HsException(proc, HS_ERR_INDEX);
    }
}

// 第 index 点的内部切向角
double HsContour::angRadAt(int index) const
{
    checkAngleIndex(*this, index, "HsContour::angRadAt");
    return angle[static_cast<size_t>(index)];
}

// 写入内部弧度
void HsContour::setAngRadAt(int index, double rad)
{
    checkAngleIndex(*this, index, "HsContour::setAngRadAt");
    angle[static_cast<size_t>(index)] = static_cast<float>(rad);
}

// 转角的度数，正方向与内部相同
double HsContour::angDegAt(int index) const
{
    return toDegree(angRadAt(index));
}

// 按度数写入，正方向与内部相同
void HsContour::setAngDegAt(int index, double deg)
{
    setAngRadAt(index, toRadian(deg));
}

// 按系统角度单位和正方向读取
double HsContour::angAt(int index) const
{
    return fromAngle(angRadAt(index));
}

// 按系统角度单位和正方向写入
void HsContour::setAngAt(int index, double external)
{
    setAngRadAt(index, toAngle(external));
}
}
