/**
 * @file HsContour.h
 * @brief HsContour头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSCONTOUR_H
#define HSCOMMDATA_HSCONTOUR_H

#include <vector>

#include <HsCommData/Geom.h>

namespace HsCommData {

/**
 * @class HsContour
 * @brief 一条轮廓。点为图像坐标，可亚像素。角度和响应可选，空则没有
 */
struct HSCOMMDATA_EXPORT HsContour {
    bool closed;             ///< true 时首尾相连
    std::vector<float> angle;    ///< 每点切向角，内部弧度。正方向从 +x 转向 +y。空表示没有。对外用 angAt
    std::vector<float> response; ///< 每点边缘响应。空表示没有

    HsContour(); ///< 默认开折线
    /** @brief 点列按个数整段复制。闭合标志和属性一并拷贝 */
    HsContour(const HsContour& other);
    /** @brief 点列按个数整段复制。闭合标志和属性一并拷贝 */
    HsContour& operator=(const HsContour& other);
    /** @brief 释放点缓冲 */
    ~HsContour();
    /** @brief 点数 */
    size_t ptCount() const;
    /** @brief 没有点 */
    bool ptsEmpty() const;
    /** @brief 第 i 个点 */
    Point2d& ptAt(size_t i);
    /** @brief 第 i 个点 */
    const Point2d& ptAt(size_t i) const;
    /** @brief 第一个点。空轮廓交给系统异常处理 */
    Point2d& ptFront();
    /** @brief 第一个点。空轮廓交给系统异常处理 */
    const Point2d& ptFront() const;
    /** @brief 最后一个点。空轮廓交给系统异常处理 */
    Point2d& ptBack();
    /** @brief 最后一个点。空轮廓交给系统异常处理 */
    const Point2d& ptBack() const;
    /** @brief 追加一个点 */
    void pushPt(const Point2d& point);
    /** @brief 改点数。变长的新点为原点 */
    void resizePts(size_t n);
    /** @brief 用向量整段填入 */
    void setPts(const std::vector<Point2d>& src);

    /** @brief 无顶点则为空 */
    bool isEmpty() const;
    /** @brief 顶点包围盒 */
    Box2 bounds() const;
    /** @brief 内部转角，弧度。越界抛 HsException */
    double angRadAt(int index) const;
    /** @brief 写入内部弧度 */
    void setAngRadAt(int index, double rad);
    /** @brief 转角的度数，正方向与内部相同 */
    double angDegAt(int index) const;
    /** @brief 按度数写入，正方向与内部相同 */
    void setAngDegAt(int index, double deg);
    /** @brief 按系统角度单位和正方向读取 */
    double angAt(int index) const;
    /** @brief 按系统角度单位和正方向写入 */
    void setAngAt(int index, double external);
    /** @brief 角度或响应非空时，长度必须与点数相同 */
    bool attribsOk() const;
private:
    /** @brief 按个数分配，不逐点构造 */
    void allocPts(size_t n);
    /** @brief 容量不足时换一块更大的缓冲，旧点整段搬过去 */
    void ensurePts(size_t need);
    /** @brief 释放点缓冲 */
    void releasePts();
    /** @brief 按个数整段复制 */
    void copyPts(const Point2d* src, size_t n);

    Point2d* m_pts;     ///< 点缓冲
    size_t m_ptCount;  ///< 个数
    size_t m_ptCap;    ///< 容量
};

}

#endif
