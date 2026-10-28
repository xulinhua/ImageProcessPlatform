/**
 * @file HsRegion.cpp
 * @brief HsRegion实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsRegion.h>
#include <cstring>
#include <HsCommData/GeomOps.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>

namespace HsCommData {

// 空弦：ce < cb，长度为 0
Chord::Chord()
    : row(0)
    , cb(0)
    , ce(-1)
{
}

// 指定行与闭区间列
Chord::Chord(int32_t r, int32_t b, int32_t e)
    : row(r)
    , cb(b)
    , ce(e)
{
}

// 像素个数；ce < cb 时为 0
int32_t Chord::length() const
{
    if (ce < cb) {
        return 0;
    }
    return ce - cb + 1;
}

// 空区域，非补集
HsRegion::HsRegion()
    : isCompl(false)
    , m_runs(nullptr)
    , m_runCount(0)
    , m_runCap(0)
{
}

// 深拷：标志和外接框逐字段，弦按条数分配后整段复制
HsRegion::HsRegion(const HsRegion& other)
    : isCompl(other.isCompl)
    , bounds(other.bounds)
    , m_runs(nullptr)
    , m_runCount(0)
    , m_runCap(0)
{
    copyRuns(other.m_runs, other.m_runCount);
}

// 深拷赋值：标志和外接框逐字段，弦按条数分配后整段复制
HsRegion& HsRegion::operator=(const HsRegion& other)
{
    if (this != &other) {
        isCompl = other.isCompl;
        bounds = other.bounds;
        copyRuns(other.m_runs, other.m_runCount);
    }
    return *this;
}
// 释放弦缓冲
HsRegion::~HsRegion()
{
    releaseRuns();
}
// 弦数
size_t HsRegion::runCount() const
{
    return m_runCount;
}

// 没有弦
bool HsRegion::runsEmpty() const
{
    return m_runCount == 0;
}

// 第 i 条弦
Chord& HsRegion::runAt(size_t i)
{
    return m_runs[i];
}

// 第 i 条弦
const Chord& HsRegion::runAt(size_t i) const
{
    return m_runs[i];
}


// 追加一条
void HsRegion::pushRun(const Chord& chord)
{
    ensureRuns(m_runCount + 1);
    m_runs[m_runCount] = chord;
    ++m_runCount;
}

// 改条数。变长的新弦为空弦
void HsRegion::resizeRuns(size_t n)
{
    if (n > m_runCount) {
        ensureRuns(n);
        for (size_t i = m_runCount; i < n; ++i) {
            m_runs[i].row = 0;
            m_runs[i].cb = 0;
            m_runs[i].ce = -1;
        }
    }
    m_runCount = n;
}

// 预留容量，不改变条数
void HsRegion::reserveRuns(size_t n)
{
    ensureRuns(n);
}

// 条数清零，容量保留
void HsRegion::clearRuns()
{
    m_runCount = 0;
}

// 把另一个区域的弦整段接到末尾
void HsRegion::appendRuns(const HsRegion& src)
{
    if (src.m_runCount == 0) {
        return;
    }
    if (this == &src) {
        const size_t n = m_runCount;
        ensureRuns(n * 2);
        std::memmove(m_runs + n, m_runs, n * sizeof(Chord));
        m_runCount = n * 2;
        return;
    }
    const size_t base = m_runCount;
    ensureRuns(base + src.m_runCount);
    std::memcpy(m_runs + base, src.m_runs, src.m_runCount * sizeof(Chord));
    m_runCount = base + src.m_runCount;
}

// 按条数分配，不逐条构造
void HsRegion::allocRuns(size_t n)
{
    if (n > m_runCap) {
        Chord* grown = static_cast<Chord*>(::operator new(n * sizeof(Chord)));
        if (m_runs) {
            ::operator delete(m_runs);
            m_runs = nullptr;
        }
        m_runs = grown;
        m_runCap = n;
    }
    m_runCount = n;
}

// 容量不足时换一块更大的缓冲，旧弦整段搬过去
void HsRegion::ensureRuns(size_t need)
{
    if (need <= m_runCap) {
        return;
    }
    size_t cap = m_runCap == 0 ? 8 : m_runCap;
    while (cap < need) {
        cap *= 2;
    }
    Chord* grown = static_cast<Chord*>(::operator new(cap * sizeof(Chord)));
    if (m_runCount > 0) {
        std::memcpy(grown, m_runs, m_runCount * sizeof(Chord));
    }
    if (m_runs) {
        ::operator delete(m_runs);
        m_runs = nullptr;
    }
    m_runs = grown;
    m_runCap = cap;
}

// 释放弦缓冲
void HsRegion::releaseRuns()
{
    if (m_runs) {
        ::operator delete(m_runs);
        m_runs = nullptr;
    }
    m_runCount = 0;
    m_runCap = 0;
}

// 按条数整段复制
void HsRegion::copyRuns(const Chord* src, size_t n)
{
    allocRuns(n);
    if (n > 0) {
        std::memcpy(m_runs, src, n * sizeof(Chord));
    }
}

// 无弦则为空
bool HsRegion::isEmpty() const
{
    return m_runCount == 0;
}

// 前景像素个数
int64_t HsRegion::area() const
{
    int64_t a = 0;
    for (size_t i = 0; i < m_runCount; ++i) {
        a += m_runs[i].length();
    }
    return a;
}

// 二分到该行后扫弦，判断点是否在区域内
bool HsRegion::contains(int row, int col) const
{
    int lo = 0;
    int hi = static_cast<int>(m_runCount);
    while (lo < hi) {
        const int mid = lo + (hi - lo) / 2;
        if (m_runs[static_cast<size_t>(mid)].row < row) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    for (int i = lo; i < static_cast<int>(m_runCount) && m_runs[static_cast<size_t>(i)].row == row; ++i) {
        if (col >= m_runs[static_cast<size_t>(i)].cb && col <= m_runs[static_cast<size_t>(i)].ce) {
            return !isCompl;
        }
    }
    return isCompl;
}

// 由轴对齐矩形逐行生成弦
HsRegion HsRegion::genRegion(const IRect& r)
{
    HsRegion out;
    if (HsCommData::isEmpty(r)) {
        return out;
    }
    out.bounds = r;
    out.reserveRuns(static_cast<size_t>(r.height));
    for (int y = top(r); y <= bottom(r); ++y) {
        out.pushRun(Chord(y, left(r), right(r)));
    }
    return out;
}

// 闭合多边形扫描转换为行程
HsRegion HsRegion::genRegion(const std::vector<Point2d>& pts)
{
    HsRegion out;
    if (pts.size() < 3) {
        return out;
    }

    double miny = pts[0].y;
    double maxy = pts[0].y;
    double minx = pts[0].x;
    double maxx = pts[0].x;
    for (size_t i = 1; i < pts.size(); ++i) {
        miny = (std::min)(miny, pts[i].y);
        maxy = (std::max)(maxy, pts[i].y);
        minx = (std::min)(minx, pts[i].x);
        maxx = (std::max)(maxx, pts[i].x);
    }

    const int y0 = static_cast<int>(std::floor(miny));
    const int y1 = static_cast<int>(std::floor(maxy));
    if (y1 < y0 || (y1 - y0) > 20000) {
        return out;
    }
    out.bounds = fromInclusive(static_cast<int>(std::floor(minx)), y0,
                                      static_cast<int>(std::ceil(maxx)), y1);

    const int n = static_cast<int>(pts.size());
    std::vector<double> xs;
    xs.reserve(static_cast<size_t>(n));

    // 逐行与边求交，奇数规则填弦
    for (int y = y0; y <= y1; ++y) {
        const double ys = static_cast<double>(y) + 0.5;
        xs.clear();
        for (int i = 0; i < n; ++i) {
            Point2d a = pts[static_cast<size_t>(i)];
            Point2d b = pts[static_cast<size_t>((i + 1) % n)];
            if (a.y > b.y) {
                const Point2d tmp = a;
                a = b;
                b = tmp;
            }
            if (ys < a.y || ys >= b.y) {
                continue;
            }
            if (std::fabs(a.y - b.y) < 1e-12) {
                continue;
            }
            const double t = (ys - a.y) / (b.y - a.y);
            xs.push_back(a.x + t * (b.x - a.x));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) {
            int cb = static_cast<int>(std::floor(xs[k] + 0.5));
            int ce = static_cast<int>(std::floor(xs[k + 1] + 0.5));
            if (ce < cb) {
                const int tmp = cb;
                cb = ce;
                ce = tmp;
            }
            if (ce >= cb) {
                out.pushRun(Chord(y, cb, ce));
            }
        }
    }
    return out;
}

HsRegion HsRegion::boundary() const
{
    HsRegion edge;
    if (runsEmpty()) {
        return edge;
    }
    std::map<int32_t, std::vector<Chord>> rows;
    for (size_t i = 0; i < runCount(); ++i) {
        const Chord& chord = runAt(i);
        if (chord.length() <= 0) {
            continue;
        }
        rows[chord.row].push_back(chord);
    }
    for (std::map<int32_t, std::vector<Chord>>::iterator it = rows.begin(); it != rows.end(); ++it) {
        std::vector<Chord>& chords = it->second;
        std::sort(chords.begin(), chords.end(), [](const Chord& a, const Chord& b) { return a.cb < b.cb; });
        std::vector<Chord> merged;
        for (size_t n = 0; n < chords.size(); ++n) {
            if (merged.empty() || chords[n].cb > merged.back().ce + 1) {
                merged.push_back(chords[n]);
            } else if (chords[n].ce > merged.back().ce) {
                merged.back().ce = chords[n].ce;
            }
        }
        chords.swap(merged);
    }
    // 端点以及上下没有被盖住的列才是边。按弦做区间差，不逐像素扫
    struct Span {
        int32_t cb;
        int32_t ce;
    };
    auto spansOf = [&rows](int32_t row, std::vector<Span>* out) {
        out->clear();
        std::map<int32_t, std::vector<Chord>>::const_iterator it = rows.find(row);
        if (it == rows.end()) {
            return;
        }
        out->reserve(it->second.size());
        for (size_t n = 0; n < it->second.size(); ++n) {
            Span s;
            s.cb = it->second[n].cb;
            s.ce = it->second[n].ce;
            out->push_back(s);
        }
    };
    auto clipBoth = [](const std::vector<Span>& above, const std::vector<Span>& below,
                        int32_t cb, int32_t ce, std::vector<Span>* inner) {
        inner->clear();
        if (ce <= cb + 1) {
            return;
        }
        const int32_t lo = cb + 1;
        const int32_t hi = ce - 1;
        size_t i = 0;
        size_t j = 0;
        while (i < above.size() && j < below.size()) {
            const int32_t a = above[i].cb > below[j].cb ? above[i].cb : below[j].cb;
            const int32_t b = above[i].ce < below[j].ce ? above[i].ce : below[j].ce;
            const int32_t c0 = a > lo ? a : lo;
            const int32_t c1 = b < hi ? b : hi;
            if (c0 <= c1) {
                Span s;
                s.cb = c0;
                s.ce = c1;
                inner->push_back(s);
            }
            if (above[i].ce < below[j].ce) {
                ++i;
            } else {
                ++j;
            }
        }
    };
    int32_t minR = 0, maxR = 0, minC = 0, maxC = 0;
    bool any = false;
    std::vector<Span> above;
    std::vector<Span> below;
    std::vector<Span> inner;
    for (std::map<int32_t, std::vector<Chord>>::const_iterator it = rows.begin(); it != rows.end(); ++it) {
        const int32_t row = it->first;
        spansOf(row - 1, &above);
        spansOf(row + 1, &below);
        for (size_t n = 0; n < it->second.size(); ++n) {
            const Chord& chord = it->second[n];
            clipBoth(above, below, chord.cb, chord.ce, &inner);
            int32_t cursor = chord.cb;
            for (size_t k = 0; k < inner.size(); ++k) {
                if (inner[k].cb > cursor) {
                    edge.pushRun(Chord(row, cursor, inner[k].cb - 1));
                }
                if (inner[k].ce + 1 > cursor) {
                    cursor = inner[k].ce + 1;
                }
            }
            if (cursor <= chord.ce) {
                edge.pushRun(Chord(row, cursor, chord.ce));
            }
        }
    }
    for (size_t n = 0; n < edge.runCount(); ++n) {
        const Chord& chord = edge.runAt(n);
        if (!any) {
            minR = maxR = chord.row;
            minC = chord.cb;
            maxC = chord.ce;
            any = true;
        } else {
            if (chord.row < minR) minR = chord.row;
            if (chord.row > maxR) maxR = chord.row;
            if (chord.cb < minC) minC = chord.cb;
            if (chord.ce > maxC) maxC = chord.ce;
        }
    }
    if (any) {
        edge.bounds = fromInclusive(minC, minR, maxC, maxR);
    }
    return edge;
}
}
