/**
 * @file HalconAlgo.cpp
 * @brief HalconAlgo实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HalconAlgo.h"

#define NOMINMAX
#ifdef HSIMAGE_HAS_HALCON
#include <HalconCpp.h>
#endif

#include <QElapsedTimer>
#include <QtMath>
#include <cstring>

#ifdef HSIMAGE_HAS_HALCON
#ifdef Chord
#undef Chord
#endif
using namespace HalconCpp;
static HImage g_himg;
static bool g_himgReady = false;
#endif

static QImage g_gray;
static bool g_grayReady = false;

// BT.601 亮度，与控件 HUD 取样一致
static int luma8(int r, int g, int b)
{
    return qBound(0, qRound(0.299 * r + 0.587 * g + 0.114 * b), 255);
}

// 转 8bit 灰度（不用 QImage::convertToFormat，其 qGray 与 HUD 差 1～2）
static QImage toLuma8(const QImage& src)
{
    if (src.isNull()) {
        return QImage();
    }
    if (src.format() == QImage::Format_Grayscale8) {
        return src;
    }
    const QImage rgb = (src.format() == QImage::Format_RGB32
                        || src.format() == QImage::Format_ARGB32
                        || src.format() == QImage::Format_ARGB32_Premultiplied)
        ? src
        : src.convertToFormat(QImage::Format_RGB32);
    QImage gray(rgb.width(), rgb.height(), QImage::Format_Grayscale8);
    if (gray.isNull()) {
        return QImage();
    }
    for (int y = 0; y < rgb.height(); ++y) {
        const QRgb* in = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        uchar* out = gray.scanLine(y);
        for (int x = 0; x < rgb.width(); ++x) {
            out[x] = static_cast<uchar>(luma8(qRed(in[x]), qGreen(in[x]), qBlue(in[x])));
        }
    }
    return gray;
}

// CPU 扫灰度图生成行程
static HsCommData::HsRegion rleFromGrayCpu(const QImage& gray, int tMin, int tMax)
{
    HsCommData::HsRegion out;
    if (gray.isNull() || gray.format() != QImage::Format_Grayscale8) {
        return out;
    }
    const int w = gray.width();
    const int h = gray.height();
    const int lo = qBound(0, tMin, 255);
    const int hi = qBound(0, tMax, 255);
    out.reserveRuns(h);
    int minx = w;
    int maxx = -1;
    int miny = h;
    int maxy = -1;
    for (int y = 0; y < h; ++y) {
        const uchar* line = gray.constScanLine(y);
        int runStart = -1;
        for (int x = 0; x <= w; ++x) {
            const bool on = (x < w) && (line[x] >= lo) && (line[x] <= hi);
            if (on) {
                if (runStart < 0) {
                    runStart = x;
                }
            } else if (runStart >= 0) {
                out.pushRun(HsCommData::Chord(y, runStart, x - 1));
                if (runStart < minx) {
                    minx = runStart;
                }
                if (x - 1 > maxx) {
                    maxx = x - 1;
                }
                if (y < miny) {
                    miny = y;
                }
                if (y > maxy) {
                    maxy = y;
                }
                runStart = -1;
            }
        }
    }
    if (maxx >= minx && maxy >= miny) {
        out.bounds = HsCommData::fromInclusive(minx, miny, maxx, maxy);
    }
    return out;
}

// 本进程是否链接了 Halcon
bool hasHalconEngine()
{
#ifdef HSIMAGE_HAS_HALCON
    return true;
#else
    return false;
#endif
}

// 引擎显示名（中文走 fromLocal8Bit，避免 QStringLiteral 在 MSVC 下乱码）
QString halconEngineName()
{
#ifdef HSIMAGE_HAS_HALCON
    return QString::fromLocal8Bit("Halcon 17.12");
#else
    return QString::fromLocal8Bit("本地扫描（未链接 Halcon）");
#endif
}

// 准备阈值输入图（BT.601 灰度，可选 HImage）
bool prepareThresholdImage(const QImage& src, QString* error)
{
    g_grayReady = false;
#ifdef HSIMAGE_HAS_HALCON
    g_himgReady = false;
#endif
    if (src.isNull()) {
        if (error) {
            *error = QString::fromLocal8Bit("图像为空");
        }
        return false;
    }

    // 不用 QImage::convertToFormat(Grayscale8)，其 qGray 与 HUD 亮度差 1～2。
    g_gray = toLuma8(src);
    g_grayReady = !g_gray.isNull();
    if (!g_grayReady) {
        if (error) {
            *error = QString::fromLocal8Bit("无法转为 8bit 灰度");
        }
        return false;
    }

#ifdef HSIMAGE_HAS_HALCON
    try {
        const int w = g_gray.width();
        const int h = g_gray.height();
        QByteArray packed;
        packed.resize(w * h);
        for (int y = 0; y < h; ++y) {
            memcpy(packed.data() + y * w, g_gray.constScanLine(y), static_cast<size_t>(w));
        }
        g_himg.GenImage1("byte", w, h, packed.data());
        g_himgReady = true;
    } catch (const HException& ex) {
        g_himgReady = false;
        if (error) {
            *error = QString::fromLocal8Bit(ex.ErrorMessage().Text());
        }
        return false;
    }
#endif
    return true;
}

#ifdef HSIMAGE_HAS_HALCON
// Halcon HsCommData::HsRegion 转 HsCommData::HsRegion（0-based 弦）
static HsCommData::HsRegion rleFromHalconRegion(const HRegion& region)
{
    // GetRegionRuns 按 0-based 像素写入 HsCommData::Chord；若实测整幅偏 1 像素再改此处，不改渲染。
    HsCommData::HsRegion out;
    HTuple rows;
    HTuple cb;
    HTuple ce;
    region.GetRegionRuns(&rows, &cb, &ce);
    const Hlong n = rows.Length();
    if (n <= 0) {
        return out;
    }
    const Hlong* pr = rows.LArr();
    const Hlong* pb = cb.LArr();
    const Hlong* pe = ce.LArr();
    out.reserveRuns(static_cast<int>(n));
    for (Hlong i = 0; i < n; ++i) {
        out.pushRun(HsCommData::Chord(static_cast<qint32>(pr[i]),
                                 static_cast<qint32>(pb[i]),
                                 static_cast<qint32>(pe[i])));
    }
    Hlong r1 = 0;
    Hlong c1 = 0;
    Hlong r2 = 0;
    Hlong c2 = 0;
    region.SmallestRectangle1(&r1, &c1, &r2, &c2);
    out.bounds = HsCommData::fromInclusive(static_cast<int>(c1), static_cast<int>(r1),
                                      static_cast<int>(c2), static_cast<int>(r2));
    return out;
}
#endif

// 对已准备的灰度图做阈值
ThresholdRst runGrayThreshold(int tMin, int tMax)
{
    ThresholdRst r;
    r.ok = false;
    r.usedHalcon = false;
    r.thresholdUs = 0;
    r.convertUs = 0;
    if (!g_grayReady) {
        r.error = QString::fromLocal8Bit("尚未加载测试图像");
        return r;
    }

    const int lo = qBound(0, tMin, 255);
    const int hi = qBound(0, tMax, 255);
    QElapsedTimer clock;

#ifdef HSIMAGE_HAS_HALCON
    if (g_himgReady) {
        try {
            clock.start();
            const HRegion region = g_himg.Threshold(static_cast<double>(lo), static_cast<double>(hi));
            r.thresholdUs = clock.nsecsElapsed() / 1000;
            clock.restart();
            r.rle = rleFromHalconRegion(region);
            r.convertUs = clock.nsecsElapsed() / 1000;
            r.ok = true;
            r.usedHalcon = true;
            return r;
        } catch (const HException& ex) {
            r.error = QString::fromLocal8Bit(ex.ErrorMessage().Text());
            return r;
        }
    }
#endif

    clock.start();
    r.rle = rleFromGrayCpu(g_gray, lo, hi);
    r.thresholdUs = clock.nsecsElapsed() / 1000;
    r.convertUs = 0;
    r.ok = true;
    r.usedHalcon = false;
    return r;
}
