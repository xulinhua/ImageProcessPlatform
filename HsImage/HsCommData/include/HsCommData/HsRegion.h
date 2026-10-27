/**
 * @file HsRegion.h
 * @brief HsRegion头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSREGION_H
#define HSCOMMDATA_HSREGION_H

#include <HsCommData/Geom.h>

#include <cstdint>
#include <vector>

namespace HsCommData {

/**
 * @class Chord
 * @brief 一行游程。row 上闭区间 [cb, ce] 为区域内像素，0-based
 */
struct HSCOMMDATA_EXPORT Chord {
    int32_t row; ///< 行号
    int32_t cb;  ///< 起始列，含端点
    int32_t ce;  ///< 结束列，含端点

    Chord(); ///< 全零
    Chord(int32_t row, int32_t cb, int32_t ce); ///< 指定行与闭区间

    /** @brief 列数。ce < cb 时为 0 */
    int32_t length() const;
};

/**
 * @class HsRegion
 * @brief 行程编码区域。可与 path/pixmap 互转
 */
class HSCOMMDATA_EXPORT HsRegion {
public:
    bool isCompl;            ///< true 表示补区域
    IRect bounds;            ///< 外接框。与 run 保持一致

    HsRegion(); ///< 空区域
    /** @brief 深拷游程。补标志一并拷贝 */
    HsRegion(const HsRegion& other);
    /** @brief 深拷赋值。补标志一并拷贝 */
    HsRegion& operator=(const HsRegion& other);
    /** @brief 释放弦缓冲 */
    ~HsRegion();
    /** @brief 条数 */
    size_t runCount() const;
    /** @brief 没有弦 */
    bool runsEmpty() const;
    /** @brief 第 i 条 */
    Chord& runAt(size_t i);
    /** @brief 第 i 条 */
    const Chord& runAt(size_t i) const;
    /** @brief 追加一条 */
    void pushRun(const Chord& chord);
    /** @brief 改条数。变长的新弦为空弦 */
    void resizeRuns(size_t n);
    /** @brief 预留容量，不改变条数 */
    void reserveRuns(size_t n);
    /** @brief 条数清零，容量保留 */
    void clearRuns();
    /** @brief 把 src 的弦整段接到末尾 */
    void appendRuns(const HsRegion& src);

    /** @brief 无游程且非补 */
    bool isEmpty() const;
    /** @brief 前景像素个数 */
    int64_t area() const;
    /**
     * @brief 点是否在区域内
     * @param row 行
     * @param col 列
     */
    bool contains(int row, int col) const;

    /** @brief 区域内与区域外相邻的像素。外圈和孔洞都在，结果仍是区域 */
    HsRegion boundary() const;

    /** @brief 由轴对齐矩形生成区域。不含补标志 */
    static HsRegion genRegion(const IRect& r);
    /** @brief 由多边形顶点生成区域。至少 3 点，按填充规则 */
    static HsRegion genRegion(const std::vector<Point2d>& pts);
private:
    /** @brief 按条数分配，不逐条构造 */
    void allocRuns(size_t n);
    /** @brief 容量不足时换一块更大的缓冲，旧弦整段搬过去 */
    void ensureRuns(size_t need);
    /** @brief 释放弦缓冲 */
    void releaseRuns();
    /** @brief 按条数整段复制 */
    void copyRuns(const Chord* src, size_t n);

    Chord* m_runs;     ///< 弦缓冲
    size_t m_runCount; ///< 条数
    size_t m_runCap;   ///< 容量
};

}

#endif
