/**
 * @file HsPoly.h
 * @brief HsPoly头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSPOLY_H
#define HSCOMMDATA_HSPOLY_H

#include <HsCommData/HsContour.h>

namespace HsCommData {

/**
 * @class PolySeg
 * @brief 逼近折线的一段。控制点，以及它盖住的原轮廓点下标
 */
struct HSCOMMDATA_EXPORT PolySeg {
    Point2d pt; ///< 本段控制点
    int first; ///< 原轮廓起点下标，含
    int last;  ///< 原轮廓终点下标，含

    PolySeg(); ///< 下标为 -1
    /** @brief 按控制点与原轮廓下标范围构造 */
    PolySeg(const Point2d& pt, int first, int last);
};

/**
 * @class HsPoly
 * @brief 一条 HsContour 的折线逼近。不内嵌原轮廓，first/last 回指调用方那条轮廓。段长和朝向由相邻控制点算出，不另存
 */
struct HSCOMMDATA_EXPORT HsPoly {
        bool closed;               ///< true 时控制点首尾相连

    HsPoly(); ///< 默认开折线
    /** @brief 控制段按段数整段复制。闭合标志一并拷贝 */
    HsPoly(const HsPoly& other);
    /** @brief 控制段按段数整段复制。闭合标志一并拷贝 */
    HsPoly& operator=(const HsPoly& other);
    /** @brief 释放段缓冲 */
    ~HsPoly();
    /** @brief 段数 */
    size_t segCount() const;
    /** @brief 没有控制段 */
    bool segsEmpty() const;
    /** @brief 第 i 段 */
    PolySeg& segAt(size_t i);
    /** @brief 第 i 段 */
    const PolySeg& segAt(size_t i) const;
    /** @brief 追加一段 */
    void pushSeg(const PolySeg& seg);
    /** @brief 改段数。变长的新段下标为 -1 */
    void resizeSegs(size_t n);

    /** @brief 无控制段则为空 */
    bool isEmpty() const;
    /** @brief 控制点包围盒 */
    Box2 bounds() const;
    /**
     * @brief 每段下标是否落在轮廓点数内，且 first 不大于 last
     * @return 没有控制段、原轮廓没点，或任一段越界时为 false
     */
    bool fits(const HsContour& contour) const;
private:
    /** @brief 按段数分配，不逐段构造 */
    void allocSegs(size_t n);
    /** @brief 容量不足时换一块更大的缓冲，旧段整段搬过去 */
    void ensureSegs(size_t need);
    /** @brief 释放段缓冲 */
    void releaseSegs();
    /** @brief 按段数整段复制 */
    void copySegs(const PolySeg* src, size_t n);

    PolySeg* m_segs;   ///< 段缓冲
    size_t m_segCount; ///< 段数
    size_t m_segCap;   ///< 容量
};

}

#endif
