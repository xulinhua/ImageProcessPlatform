/**
 * @file ObjectBench.cpp
 * @brief ObjectBench实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ObjectBench.h"

#include <HsCommData/HsException.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

namespace {

const int kCanvasW = 960;
const int kCanvasH = 640;
const int64_t kMaxBytes = 800LL * 1024 * 1024;

struct Cell {
    int x;
    int y;
    int w;
    int h;
};

// 把第 index 个对象放进画布格子，便于 Stack 能看见
Cell cellAt(int index, int count)
{
    int cols = 1;
    while (cols * cols < count) {
        ++cols;
    }
    const int rows = (count + cols - 1) / cols;
    const int margin = 8;
    int cw = (kCanvasW - margin * 2) / cols;
    int ch = (kCanvasH - margin * 2) / rows;
    if (cw < 2) {
        cw = 2;
    }
    if (ch < 2) {
        ch = 2;
    }
    Cell c;
    c.x = margin + (index % cols) * cw;
    c.y = margin + (index / cols) * ch;
    c.w = cw > 2 ? cw - 1 : 1;
    c.h = ch > 2 ? ch - 1 : 1;
    return c;
}

int64_t estimateBytes(int kind, int count, int payload)
{
    const int64_t n = count;
    const int64_t p = payload;
    if (kind == SampleRegion) {
        return n * p * static_cast<int64_t>(sizeof(HsCommData::Chord));
    }
    if (kind == SampleContour) {
        return n * p * static_cast<int64_t>(sizeof(HsCommData::Point2d));
    }
    if (kind == SamplePoly) {
        return n * p * static_cast<int64_t>(sizeof(HsCommData::PolySeg));
    }
    if (kind == SampleCircle) {
        return n * static_cast<int64_t>(sizeof(HsCommData::CircleP));
    }
    const int64_t heavy = static_cast<int64_t>(sizeof(HsCommData::Chord));
    return n * p * heavy;
}

HsCommData::HsRegion makeRegion(const Cell& cell, int payload)
{
    HsCommData::HsRegion r;
    r.isCompl = false;
    r.resizeRuns(static_cast<size_t>(payload));
    const int x1 = cell.x + cell.w - 1;
    for (int k = 0; k < payload; ++k) {
        const int row = cell.y + (k % cell.h);
        r.runAt(static_cast<size_t>(k)) = HsCommData::Chord(row, cell.x, x1);
    }
    r.bounds = HsCommData::IRect(cell.x, cell.y, cell.w, cell.h);
    return r;
}

HsCommData::HsContour makeContour(const Cell& cell, int payload)
{
    HsCommData::HsContour c;
    c.closed = true;
    c.resizePts(static_cast<size_t>(payload));
    const double cx = cell.x + cell.w * 0.5;
    const double cy = cell.y + cell.h * 0.5;
    const double rx = cell.w * 0.45;
    const double ry = cell.h * 0.45;
    for (int k = 0; k < payload; ++k) {
        const double t = 6.283185307179586 * k / payload;
        c.ptAt(static_cast<size_t>(k)) = HsCommData::Point2d(cx + std::cos(t) * rx, cy + std::sin(t) * ry);
    }
    return c;
}

HsCommData::HsPoly makePoly(const Cell& cell, int payload)
{
    HsCommData::HsPoly poly;
    poly.closed = false;
    poly.resizeSegs(static_cast<size_t>(payload));
    for (int k = 0; k < payload; ++k) {
        const double u = payload <= 1 ? 0.0 : static_cast<double>(k) / (payload - 1);
        const HsCommData::Point2d pt(cell.x + u * cell.w, cell.y + (k % 2) * (cell.h - 1));
        poly.segAt(static_cast<size_t>(k)) = HsCommData::PolySeg(pt, k, k);
    }
    return poly;
}

HsCommData::CircleP makeCircle(const Cell& cell, int payload)
{
    HsCommData::CircleP c;
    c.c = HsCommData::Point2d(cell.x + cell.w * 0.5, cell.y + cell.h * 0.5);
    double r = payload;
    const double lim = (cell.w < cell.h ? cell.w : cell.h) * 0.45;
    if (r > lim) {
        r = lim;
    }
    if (r < 1.0) {
        r = 1.0;
    }
    c.r = r;
    return c;
}

HsCommData::LineP makeLine(const Cell& cell)
{
    HsCommData::LineP line;
    line.a = HsCommData::Point2d(cell.x, cell.y);
    line.b = HsCommData::Point2d(cell.x + cell.w - 1, cell.y + cell.h - 1);
    return line;
}

int64_t sig0(const HsCommData::HsObject& o)
{
    if (o.isEmpty()) {
        return 0;
    }
    const HsCommData::ObjectKind k = o.kindAt(0);
    if (k == HsCommData::ObjRegion) {
        return static_cast<int64_t>(o.regionAt(0)->runCount());
    }
    if (k == HsCommData::ObjContour) {
        return static_cast<int64_t>(o.contourAt(0)->ptCount());
    }
    if (k == HsCommData::ObjPoly) {
        return static_cast<int64_t>(o.polyAt(0)->segCount());
    }
    if (k == HsCommData::ObjCircle) {
        return static_cast<int64_t>(o.circleAt(0)->r * 1000.0);
    }
    return o.count();
}

double elapsedMs(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

double timeElements(const HsCommData::HsObject& src, int loops, int64_t* touch)
{
    const HsCommData::ObjectKind k = src.kind();
    const int n = src.count();
    const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    if (k == HsCommData::ObjRegion) {
        for (int L = 0; L < loops; ++L) {
            for (int i = 0; i < n; ++i) {
                const HsCommData::HsRegion c = *src.regionAt(i);
                *touch += static_cast<int64_t>(c.runCount());
            }
        }
    } else if (k == HsCommData::ObjContour) {
        for (int L = 0; L < loops; ++L) {
            for (int i = 0; i < n; ++i) {
                const HsCommData::HsContour c = *src.contourAt(i);
                *touch += static_cast<int64_t>(c.ptCount());
            }
        }
    } else if (k == HsCommData::ObjPoly) {
        for (int L = 0; L < loops; ++L) {
            for (int i = 0; i < n; ++i) {
                const HsCommData::HsPoly c = *src.polyAt(i);
                *touch += static_cast<int64_t>(c.segCount());
            }
        }
    } else if (k == HsCommData::ObjCircle) {
        for (int L = 0; L < loops; ++L) {
            for (int i = 0; i < n; ++i) {
                const HsCommData::CircleP c = *src.circleAt(i);
                *touch += static_cast<int64_t>(c.r);
            }
        }
    } else {
        return -1.0;
    }
    return elapsedMs(t0) / loops;
}

} // namespace

ObjectSampleStats fillSample(HsCommData::HsObject* dst, int kind, int count, int payload)
{
    ObjectSampleStats st;
    st.imageW = kCanvasW;
    st.imageH = kCanvasH;
    if (!dst || count < 1 || payload < 1 || kind < SampleRegion || kind > SampleMixed) {
        st.fail = 1;
        return st;
    }
    const int64_t bytes = estimateBytes(kind, count, payload);
    if (bytes > kMaxBytes) {
        st.fail = 2;
        st.bytes = bytes;
        return st;
    }
    try {
        HsCommData::HsObject built;
        int64_t units = 0;
        for (int i = 0; i < count; ++i) {
            const Cell cell = cellAt(i, count);
            int slot = kind;
            if (kind == SampleMixed) {
                const int mix[4] = { SampleRegion, SampleContour, SampleCircle, -1 };
                slot = mix[i % 4];
            }
            if (slot == SampleRegion) {
                built.append(HsCommData::HsObject(makeRegion(cell, payload)));
                units += payload;
            } else if (slot == SampleContour) {
                built.append(HsCommData::HsObject(makeContour(cell, payload)));
                units += payload;
            } else if (slot == SamplePoly) {
                built.append(HsCommData::HsObject(makePoly(cell, payload)));
                units += payload;
            } else if (slot == SampleCircle) {
                built.append(HsCommData::HsObject(makeCircle(cell, payload)));
                units += 1;
            } else {
                built.append(HsCommData::HsObject(makeLine(cell)));
                units += 1;
            }
        }
        *dst = built;
        st.ok = true;
        st.fail = 0;
        st.count = built.count();
        st.payloadUnits = units;
        st.bytes = bytes;
    } catch (const HsCommData::HsException& e) {
        st.fail = 3;
        st.errorCode = static_cast<int>(e.code());
    } catch (...) {
        st.fail = 3;
    }
    return st;
}

CopyTimings benchCopy(const HsCommData::HsObject& src, int loops)
{
    CopyTimings out;
    out.loops = loops;
    if (src.isEmpty() || loops < 1) {
        out.fail = 1;
        return out;
    }
    try {
        int64_t touch = 0;
        const std::chrono::steady_clock::time_point tHandle = std::chrono::steady_clock::now();
        for (int i = 0; i < loops; ++i) {
            const HsCommData::HsObject h = src;
            touch += h.count();
        }
        out.msHandle = elapsedMs(tHandle) / loops;
        out.msElement = timeElements(src, loops, &touch);

        const std::chrono::steady_clock::time_point tClone = std::chrono::steady_clock::now();
        HsCommData::HsObject last;
        for (int i = 0; i < loops; ++i) {
            last = src.clone();
            touch += last.count();
        }
        out.msClone = elapsedMs(tClone) / loops;

        HsCommData::HsObject a = src;
        HsCommData::HsObject b = a.clone();
        const int c0 = b.count();
        const int64_t s0 = sig0(b);
        if (!a.isEmpty()) {
            const HsCommData::ObjectKind k = a.kindAt(0);
            if (k == HsCommData::ObjRegion) {
                a.regionAt(0)->pushRun(HsCommData::Chord(-1, 0, 0));
            } else if (k == HsCommData::ObjContour) {
                a.contourAt(0)->pushPt(HsCommData::Point2d(-1, -1));
            } else if (k == HsCommData::ObjPoly) {
                a.polyAt(0)->pushSeg(HsCommData::PolySeg(HsCommData::Point2d(-1, -1), 0, 0));
            }
        }
        HsCommData::CircleP extra;
        extra.r = 1;
        a.append(HsCommData::HsObject(extra));
        out.isolated = (b.count() == c0 && sig0(b) == s0);
        out.ok = true;
        out.fail = 0;
        (void)touch;
    } catch (const HsCommData::HsException& e) {
        out.fail = 2;
        out.errorCode = static_cast<int>(e.code());
    } catch (...) {
        out.fail = 2;
    }
    return out;
}

LockTimings benchLock(HsCommData::HsObject* shared, int readers, int writers, int opsEach)
{
    LockTimings out;
    if (!shared || shared->isEmpty() || readers < 1 || writers < 0 || opsEach < 1) {
        return out;
    }
    out.startCount = shared->count();
    const int64_t baseSig = sig0(*shared);
    const int start = out.startCount;
    const int cap = start + writers * opsEach;
    std::atomic<int> errors(0);
    std::atomic<int> readerOps(0);
    std::atomic<int> writerOps(0);
    std::atomic<int> cloneBad(0);

    const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    pool.reserve(static_cast<size_t>(readers + writers));
    for (int r = 0; r < readers; ++r) {
        pool.push_back(std::thread([shared, opsEach, start, cap, baseSig, &errors, &readerOps, &cloneBad]() {
            for (int i = 0; i < opsEach; ++i) {
                try {
                    const HsCommData::HsObject h = *shared;
                    const int n = h.count();
                    if (n < start || n > cap) {
                        errors.fetch_add(1);
                    }
                    (void)h.kind();
                    if ((i % 32) == 0 && n > 0) {
                        const HsCommData::HsObject one = h.select(0).clone();
                        if (sig0(one) != baseSig) {
                            cloneBad.fetch_add(1);
                        }
                    }
                    readerOps.fetch_add(1);
                } catch (...) {
                    errors.fetch_add(1);
                }
            }
        }));
    }
    for (int w = 0; w < writers; ++w) {
        pool.push_back(std::thread([shared, opsEach, w, &errors, &writerOps]() {
            for (int i = 0; i < opsEach; ++i) {
                try {
                    HsCommData::CircleP c;
                    c.c = HsCommData::Point2d(w, i % 64);
                    c.r = 2;
                    shared->append(HsCommData::HsObject(c));
                    writerOps.fetch_add(1);
                } catch (...) {
                    errors.fetch_add(1);
                }
            }
        }));
    }
    for (size_t i = 0; i < pool.size(); ++i) {
        pool[i].join();
    }
    out.ms = elapsedMs(t0);
    out.errors = errors.load();
    out.readerOps = readerOps.load();
    out.writerOps = writerOps.load();
    out.finalCount = shared->count();
    out.expectedCount = start + out.writerOps;
    out.countOk = (out.finalCount == out.expectedCount);
    out.cloneStable = (cloneBad.load() == 0);
    out.ok = (out.errors == 0 && out.countOk && out.cloneStable);
    return out;
}
