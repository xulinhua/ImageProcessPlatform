/**
 * @file RegionDisplay.cpp
 * @brief RegionDisplay实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RegionDisplay.h"

#include <QImage>
#include <algorithm>
#include <vector>

namespace HsCtrl {

// 每条弦一个 1px 高矩形，拼成填充 path
QPainterPath rleToPath(const HsCommData::HsRegion& rle)
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (size_t i = 0; i < rle.runCount(); ++i) {
        const HsCommData::Chord& c = rle.runAt(i);
        const int32_t len = c.length();
        if (len <= 0) {
            continue;
        }
        path.addRect(QRectF(c.cb, c.row, len, 1.0));
    }
    return path;
}

// 空显示缓存
RegionDisplay::RegionDisplay()
    : maskScale(0)
{
}

// 由 rle 生成填充 path
void RegionDisplay::rebuildPath()
{
    path = rleToPath(rle);
}

void RegionDisplay::rebuildOutline()
{
    outline = rleToPath(rle.boundary());
}

// 行程多才改画 mask。面积大但行程少（整幅矩形一行一条）仍走 path
bool RegionDisplay::preferMask(const HsCommData::HsRegion& rle)
{
    return rle.runCount() >= 4096;
}

// 由视口缩放得到 lod，夹到 [1/16, 1]
qreal RegionDisplay::clampLodScale(qreal viewScale)
{
    if (viewScale < 1e-6) {
        viewScale = 1e-6;
    }
    return qBound(1.0 / 16.0, viewScale * 1.25, 1.0);
}

// 按 lodScale 生成预乘 ARGB mask
void RegionDisplay::rebuildMask(const QColor& fill, qreal lodScale)
{
    path = QPainterPath();
    maskLod = QPixmap();
    maskScale = 0;
    if (rle.isEmpty() || HsCommData::isEmpty(rle.bounds)) {
        return;
    }

    lodScale = qBound(1.0 / 16.0, lodScale, 1.0);
    const HsCommData::IRect b = rle.bounds;
    const int bw = qMax(b.width, 1);
    const int bh = qMax(b.height, 1);
    const int mw = qMax(1, qRound(static_cast<qreal>(bw) * lodScale));
    const int mh = qMax(1, qRound(static_cast<qreal>(bh) * lodScale));

    QImage img(mw, mh, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);

    const int a = fill.alpha();
    const QRgb px = qRgba((fill.red() * a + 127) / 255,
                          (fill.green() * a + 127) / 255,
                          (fill.blue() * a + 127) / 255,
                          a);

    const int x0b = left(b);
    const int y0b = top(b);
    // 同一行先并成不重叠的区间，再涂一次
    struct Span { int cb; int ce; };
    std::vector<std::vector<Span>> rows(static_cast<size_t>(bh));
    for (size_t i = 0; i < rle.runCount(); ++i) {
        const HsCommData::Chord& c = rle.runAt(i);
        if (c.length() <= 0) {
            continue;
        }
        const int iy = c.row - y0b;
        if (iy < 0 || iy >= bh) {
            continue;
        }
        std::vector<Span>& row = rows[static_cast<size_t>(iy)];
        const int cb = c.cb;
        const int ce = c.ce;
        size_t k = 0;
        while (k < row.size() && row[k].ce + 1 < cb) {
            ++k;
        }
        if (k == row.size()) {
            Span s;
            s.cb = cb;
            s.ce = ce;
            row.push_back(s);
        } else if (ce + 1 < row[k].cb) {
            Span s;
            s.cb = cb;
            s.ce = ce;
            row.insert(row.begin() + static_cast<std::ptrdiff_t>(k), s);
        } else {
            if (cb < row[k].cb) {
                row[k].cb = cb;
            }
            if (ce > row[k].ce) {
                row[k].ce = ce;
            }
            size_t j = k + 1;
            while (j < row.size() && row[j].cb <= row[k].ce + 1) {
                if (row[j].ce > row[k].ce) {
                    row[k].ce = row[j].ce;
                }
                ++j;
            }
            if (j > k + 1) {
                row.erase(row.begin() + static_cast<std::ptrdiff_t>(k + 1), row.begin() + static_cast<std::ptrdiff_t>(j));
            }
        }
    }
    for (int iy = 0; iy < bh; ++iy) {
        const std::vector<Span>& row = rows[static_cast<size_t>(iy)];
        if (row.empty()) {
            continue;
        }
        int y0 = iy * mh / bh;
        int y1 = (iy + 1) * mh / bh - 1;
        if (y1 < y0) {
            y1 = y0;
        }
        if (y0 < 0) {
            y0 = 0;
        }
        if (y1 >= mh) {
            y1 = mh - 1;
        }
        if (y1 < y0) {
            continue;
        }
        for (size_t k = 0; k < row.size(); ++k) {
            int x0 = (row[k].cb - x0b) * mw / bw;
            int x1 = (row[k].ce - x0b + 1) * mw / bw - 1;
            if (x1 < x0) {
                x1 = x0;
            }
            if (x0 < 0) {
                x0 = 0;
            }
            if (x1 >= mw) {
                x1 = mw - 1;
            }
            if (x1 < x0) {
                continue;
            }
            for (int y = y0; y <= y1; ++y) {
                QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
                std::fill(line + x0, line + x1 + 1, px);
            }
        }
    }

    maskLod = QPixmap::fromImage(img);
    maskScale = lodScale;
}

// 无行程且无缓存
bool RegionDisplay::isEmpty() const
{
    return rle.isEmpty() && path.isEmpty() && maskLod.isNull();
}

}