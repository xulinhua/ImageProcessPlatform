/**
 * @file ConvertBench.cpp
 * @brief ConvertBench实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ConvertBench.h"

#include <QElapsedTimer>
#include <cstring>
#include <vector>

#ifdef HSIMAGE_HAS_HALCON
#define NOMINMAX
#include <HalconCpp.h>
#ifdef Chord
#undef Chord
#endif
using namespace HalconCpp;
#endif

namespace {

// 约 500 万像素：2448*2048 = 5013504
const int kDefaultW = 2448;
const int kDefaultH = 2048;

// 计时，毫秒
double elapsedMs(QElapsedTimer& t)
{
    return static_cast<double>(t.nsecsElapsed()) / 1000000.0;
}

} // namespace

// 本机构建是否带 Halcon
bool convertBenchHasHalcon()
{
#ifdef HSIMAGE_HAS_HALCON
    return true;
#else
    return false;
#endif
}

// 本机构建是否带 OpenCV
bool convertBenchHasOpenCv()
{
#ifdef HSIMAGE_HAS_OPENCV
    return true;
#else
    return false;
#endif
}

// 生成灰度测试图：棋盘块加渐变，避免全黑
QImage makeBenchGrayImage(int width, int height)
{
    if (width <= 0) {
        width = kDefaultW;
    }
    if (height <= 0) {
        height = kDefaultH;
    }
    QImage img(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        unsigned char* line = img.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const int v = ((x / 64) + (y / 64)) % 2 == 0
                ? ((x + y) & 255)
                : (255 - ((x * 3 + y) & 255));
            line[x] = static_cast<unsigned char>(v);
        }
    }
    return img;
}

// 生成彩色测试图，RGB 三通道各有规律
QImage makeBenchColorImage(int width, int height)
{
    if (width <= 0) {
        width = kDefaultW;
    }
    if (height <= 0) {
        height = kDefaultH;
    }
    QImage img(width, height, QImage::Format_RGB888);
    for (int y = 0; y < height; ++y) {
        unsigned char* line = img.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const int i = x * 3;
            line[i] = static_cast<unsigned char>(x & 255);
            line[i + 1] = static_cast<unsigned char>(y & 255);
            line[i + 2] = static_cast<unsigned char>((x + y) & 255);
        }
    }
    return img;
}

#ifdef HSIMAGE_HAS_OPENCV

// QImage 转 Mat。灰度直拷，彩色换成 BGR 再进 OpenCV
cv::Mat qImageToMat(const QImage& img)
{
    if (img.isNull()) {
        return cv::Mat();
    }
    if (img.format() == QImage::Format_Grayscale8) {
        cv::Mat m(img.height(), img.width(), CV_8UC1);
        for (int y = 0; y < img.height(); ++y) {
            memcpy(m.ptr(y), img.constScanLine(y), static_cast<size_t>(img.width()));
        }
        return m;
    }
    const QImage rgb = (img.format() == QImage::Format_RGB888)
        ? img
        : img.convertToFormat(QImage::Format_RGB888);
    cv::Mat m(rgb.height(), rgb.width(), CV_8UC3);
    for (int y = 0; y < rgb.height(); ++y) {
        const unsigned char* s = rgb.constScanLine(y);
        unsigned char* d = m.ptr(y);
        for (int x = 0; x < rgb.width(); ++x) {
            const int i = x * 3;
            d[i] = s[i + 2];
            d[i + 1] = s[i + 1];
            d[i + 2] = s[i];
        }
    }
    return m;
}

// Mat 转 QImage
QImage matToQImage(const cv::Mat& mat)
{
    if (mat.empty()) {
        return QImage();
    }
    if (mat.type() == CV_8UC1) {
        QImage img(mat.cols, mat.rows, QImage::Format_Grayscale8);
        for (int y = 0; y < mat.rows; ++y) {
            memcpy(img.scanLine(y), mat.ptr(y), static_cast<size_t>(mat.cols));
        }
        return img;
    }
    if (mat.type() == CV_8UC3) {
        QImage img(mat.cols, mat.rows, QImage::Format_RGB888);
        for (int y = 0; y < mat.rows; ++y) {
            const unsigned char* s = mat.ptr(y);
            unsigned char* d = img.scanLine(y);
            for (int x = 0; x < mat.cols; ++x) {
                const int i = x * 3;
                d[i] = s[i + 2];
                d[i + 1] = s[i + 1];
                d[i + 2] = s[i];
            }
        }
        return img;
    }
    return QImage();
}

#endif // HSIMAGE_HAS_OPENCV

#ifdef HSIMAGE_HAS_HALCON

// ---------- 灰度：GenImage1 + memcpy ----------

// 灰度 QImage 转 HObject。先统一成灰度缓冲，再按行拷进去
static bool grayQImageToHObject(const QImage& src, HObject* out, QString* err)
{
    if (!out) {
        return false;
    }
    const QImage g = (src.format() == QImage::Format_Grayscale8)
        ? src
        : src.convertToFormat(QImage::Format_Grayscale8);
    if (g.isNull()) {
        if (err) {
            *err = QString::fromLocal8Bit("图像为空");
        }
        return false;
    }
    try {
        const int w = g.width();
        const int h = g.height();
        HImage tmp;
        tmp.GenImageConst("byte", w, h);
        HString type;
        Hlong gw = 0;
        Hlong gh = 0;
        void* ptr = tmp.GetImagePointer1(&type, &gw, &gh);
        if (!ptr || gw != w || gh != h) {
            if (err) {
                *err = QString::fromLocal8Bit("GenImageConst 失败");
            }
            return false;
        }
        for (int y = 0; y < h; ++y) {
            memcpy(static_cast<unsigned char*>(ptr) + static_cast<size_t>(y) * static_cast<size_t>(w),
                   g.constScanLine(y), static_cast<size_t>(w));
        }
        *out = tmp;
        return true;
    } catch (const HException& e) {
        if (err) {
            *err = QString::fromLocal8Bit(e.ErrorMessage().Text());
        }
        return false;
    }
}

// HObject 灰度转 QImage（memcpy）
static bool grayHObjectToQImage(const HObject& himg, QImage* out, QString* err)
{
    if (!out) {
        return false;
    }
    try {
        HTuple ch;
        CountChannels(himg, &ch);
        if (ch.I() != 1) {
            if (err) {
                *err = QString::fromLocal8Bit("不是单通道图像");
            }
            return false;
        }
        HTuple pointer;
        HTuple type;
        HTuple w;
        HTuple h;
        GetImagePointer1(himg, &pointer, &type, &w, &h);
        const int width = w.I();
        const int height = h.I();
        QImage img(width, height, QImage::Format_Grayscale8);
        const void* src = reinterpret_cast<void*>(pointer.L());
        for (int y = 0; y < height; ++y) {
            memcpy(img.scanLine(y),
                   static_cast<const unsigned char*>(src) + static_cast<size_t>(y) * static_cast<size_t>(width),
                   static_cast<size_t>(width));
        }
        *out = img;
        return true;
    } catch (const HException& e) {
        if (err) {
            *err = QString::fromLocal8Bit(e.ErrorMessage().Text());
        }
        return false;
    }
}

// ---------- 彩色方案1：Pointer3 + for，往返都用 for 逐点 ----------

// H 到彩色 RGB，GetImagePointer3 + for
static bool colorForLoopFromH(const HObject& himg, QImage* out, QString* err)
{
    try {
        HTuple ch;
        CountChannels(himg, &ch);
        if (ch.I() < 3) {
            if (err) {
                *err = QString::fromLocal8Bit("不是三通道图像");
            }
            return false;
        }
        HTuple type;
        HTuple w;
        HTuple h;
        HTuple pr;
        HTuple pg;
        HTuple pb;
        GetImagePointer3(himg, &pr, &pg, &pb, &type, &w, &h);
        const int width = w.I();
        const int height = h.I();
        const unsigned char* r = reinterpret_cast<unsigned char*>(pr.L());
        const unsigned char* g = reinterpret_cast<unsigned char*>(pg.L());
        const unsigned char* b = reinterpret_cast<unsigned char*>(pb.L());
        QImage img(width, height, QImage::Format_RGB888);
        const int bpl = img.bytesPerLine();
        unsigned char* dst = img.bits();
        int idx = 0;
        for (int y = 0; y < height; ++y) {
            unsigned char* line = dst + y * bpl;
            for (int x = 0; x < width; ++x) {
                const int o = x * 3;
                line[o] = r[idx];
                line[o + 1] = g[idx];
                line[o + 2] = b[idx];
                ++idx;
            }
        }
        *out = img;
        return true;
    } catch (const HException& e) {
        if (err) {
            *err = QString::fromLocal8Bit(e.ErrorMessage().Text());
        }
        return false;
    }
}

// ---------- 彩色方案2：Interleave，避免逐点 ----------

#ifdef HSIMAGE_HAS_OPENCV

// Mat 转 Halcon，对齐工程 ConvertMat2Halcon。
static void convertMat2Halcon(cv::Mat* pMatImg, HObject* pHObject)
{
    if (pMatImg == 0 || pHObject == 0) {
        return;
    }
    GenEmptyObj(pHObject);
    if (pMatImg->channels() == 1) {
        const int height = pMatImg->rows;
        const int width = pMatImg->cols;
        GenImage1(pHObject, "byte", width, height, reinterpret_cast<Hlong>(pMatImg->data));
    } else if (pMatImg->channels() == 3) {
        const int height = pMatImg->rows;
        const int width = pMatImg->cols;
        GenImageInterleaved(pHObject, reinterpret_cast<Hlong>(pMatImg->data), "bgr",
                            width, height, 0, "byte", width, height, 0, 0, -1, 0);
    }
}

// Halcon 转 Mat，对齐工程 ConvertHalcon2Mat。彩色不走 RegionFeatures。
static void convertHalcon2Mat(HObject stHalconImg, cv::Mat* pMatImg)
{
    if (pMatImg == 0 || !stHalconImg.IsInitialized()) {
        return;
    }
    HTuple channel;
    CountChannels(stHalconImg, &channel);
    if (channel.I() == 1) {
        HTuple pointer;
        HTuple type;
        HTuple w;
        HTuple h;
        GetImagePointer1(stHalconImg, &pointer, &type, &w, &h);
        const int width = w.I();
        const int height = h.I();
        *pMatImg = cv::Mat(height, width, CV_8UC1);
        memcpy(pMatImg->data, reinterpret_cast<void*>(pointer.L()),
               static_cast<size_t>(width) * static_cast<size_t>(height));
    } else if (channel.I() == 3) {
        HObject img;
        HTuple ptr;
        HTuple type;
        HTuple strideT;
        HTuple heightT;
        HTuple w;
        HTuple h;
        // 先取像素宽高。Interleave 后 Pointer1 的 Width 是行字节，不能当像素宽。
        GetImageSize(stHalconImg, &w, &h);
        const int width = w.I();
        const int height = h.I();
        // 交织成 BGR 再交给 OpenCV Mat 使用。不要对每个像素写 for 循环。
        InterleaveChannels(stHalconImg, &img, "bgr", "match", 0);
        GetImagePointer1(img, &ptr, &type, &strideT, &heightT);
        *pMatImg = cv::Mat(height, width, CV_8UC3);
        // "match" 让行紧凑，后面 memcpy 才是整块。
        memcpy(pMatImg->data, reinterpret_cast<void*>(ptr.L()),
               static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
        (void)strideT;
        (void)heightT;
    }
}

#endif // HSIMAGE_HAS_OPENCV

// QImage RGB888 -> GenImageInterleaved("rgb")。行不紧凑时先收成连续缓冲。
static bool colorInterleaveToH(const QImage& srcRgb, HObject* out, std::vector<unsigned char>* keepAlive,
                               QString* err, bool deepCopy)
{
    const QImage rgb = (srcRgb.format() == QImage::Format_RGB888)
        ? srcRgb
        : srcRgb.convertToFormat(QImage::Format_RGB888);
    if (rgb.isNull() || !out || !keepAlive) {
        if (err) {
            *err = QString::fromLocal8Bit("输入无效");
        }
        return false;
    }
    try {
        const int w = rgb.width();
        const int h = rgb.height();
        const unsigned char* pixelPtr = nullptr;
        if (rgb.bytesPerLine() == w * 3) {
            // 行字节等于宽乘 3 时直接用 QImage 缓冲，不另拷
            pixelPtr = rgb.constBits();
            keepAlive->clear();
        } else {
            // 有对齐填充时 memcpy 收成连续，不交换 R/B
            keepAlive->resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 3);
            for (int y = 0; y < h; ++y) {
                memcpy(&(*keepAlive)[static_cast<size_t>(y) * static_cast<size_t>(w) * 3],
                       rgb.constScanLine(y), static_cast<size_t>(w) * 3);
            }
            pixelPtr = &(*keepAlive)[0];
        }
        HObject img;
        GenEmptyObj(&img);
        GenImageInterleaved(&img, reinterpret_cast<Hlong>(const_cast<unsigned char*>(pixelPtr)), "rgb",
                            w, h, 0, "byte", w, h, 0, 0, -1, 0);
        if (deepCopy) {
            HObject owned;
            CopyImage(img, &owned);
            *out = owned;
        } else {
            *out = img;
        }
        return true;
    } catch (const HException& e) {
        if (err) {
            *err = QString::fromLocal8Bit(e.ErrorMessage().Text());
        }
        return false;
    }
}

static bool colorInterleaveFromH(const HObject& himg, QImage* out, QString* err)
{
    try {
        HTuple channel;
        CountChannels(himg, &channel);
        if (channel.I() < 3) {
            if (err) {
                *err = QString::fromLocal8Bit("不是三通道图像");
            }
            return false;
        }
        // 先取像素宽高。Interleave 后 Pointer1 的 Width 是 stride，不能用它乘 3 当像素数
        HTuple pixelW;
        HTuple pixelH;
        GetImageSize(himg, &pixelW, &pixelH);
        const int width = pixelW.I();
        const int height = pixelH.I();

        HObject img;
        HTuple ptr;
        HTuple type;
        HTuple strideT;
        HTuple heightT;
        InterleaveChannels(himg, &img, "rgb", "match", 0);
        GetImagePointer1(img, &ptr, &type, &strideT, &heightT);

        QImage qimg(width, height, QImage::Format_RGB888);
        const size_t nbytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
        // "match" + RGB888 行紧凑时整块 memcpy，否则按行拷
        if (qimg.bytesPerLine() == width * 3 && strideT.I() == width * 3) {
            memcpy(qimg.bits(), reinterpret_cast<void*>(ptr.L()), nbytes);
        } else {
            const unsigned char* src = reinterpret_cast<unsigned char*>(ptr.L());
            const int stride = strideT.I();
            for (int y = 0; y < height; ++y) {
                memcpy(qimg.scanLine(y), src + static_cast<size_t>(y) * static_cast<size_t>(stride),
                       static_cast<size_t>(width) * 3);
            }
        }
        *out = qimg;
        return true;
    } catch (const HException& e) {
        if (err) {
            *err = QString::fromLocal8Bit(e.ErrorMessage().Text());
        }
        return false;
    }
}

// ---------- 彩色方案3：拆成三平面再 GenImage3，回来 Decompose3 + for 拼回 ----------

// 把 RGB 拆成三个平面（调用方持有缓冲）
static bool splitRgbToPlanes(const QImage& srcRgb, std::vector<unsigned char>* r,
                             std::vector<unsigned char>* g, std::vector<unsigned char>* b, QString* err)
{
    const QImage rgb = (srcRgb.format() == QImage::Format_RGB888)
        ? srcRgb
        : srcRgb.convertToFormat(QImage::Format_RGB888);
    if (rgb.isNull() || !r || !g || !b) {
        if (err) {
            *err = QString::fromLocal8Bit("图像为空");
        }
        return false;
    }
    const int w = rgb.width();
    const int h = rgb.height();
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    r->resize(n);
    g->resize(n);
    b->resize(n);
    size_t i = 0;
    for (int y = 0; y < h; ++y) {
        const unsigned char* line = rgb.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const int o = x * 3;
            (*r)[i] = line[o];
            (*g)[i] = line[o + 1];
            (*b)[i] = line[o + 2];
            ++i;
        }
    }
    return true;
}

// 三平面指针送进 GenImage3（调用方持有平面）
static bool colorGenImage3ToH(int w, int h, unsigned char* r, unsigned char* g, unsigned char* b,
                              HObject* out, QString* err)
{
    if (!out || !r || !g || !b || w <= 0 || h <= 0) {
        if (err) {
            *err = QString::fromLocal8Bit("输入无效");
        }
        return false;
    }
    try {
        HObject img;
        GenImage3(&img, "byte", w, h, reinterpret_cast<Hlong>(r), reinterpret_cast<Hlong>(g),
                  reinterpret_cast<Hlong>(b));
        *out = img;
        return true;
    } catch (const HException& e) {
        if (err) {
            *err = QString::fromLocal8Bit(e.ErrorMessage().Text());
        }
        return false;
    }
}

static bool colorDecompose3FromH(const HObject& himg, QImage* out, QString* err)
{
    try {
        HObject hr;
        HObject hg;
        HObject hb;
        Decompose3(himg, &hr, &hg, &hb);
        HTuple type;
        HTuple w;
        HTuple h;
        HTuple pr;
        HTuple pg;
        HTuple pb;
        GetImagePointer1(hr, &pr, &type, &w, &h);
        GetImagePointer1(hg, &pg, &type, &w, &h);
        GetImagePointer1(hb, &pb, &type, &w, &h);
        const int width = w.I();
        const int height = h.I();
        const unsigned char* r = reinterpret_cast<unsigned char*>(pr.L());
        const unsigned char* g = reinterpret_cast<unsigned char*>(pg.L());
        const unsigned char* b = reinterpret_cast<unsigned char*>(pb.L());
        QImage img(width, height, QImage::Format_RGB888);
        int idx = 0;
        for (int y = 0; y < height; ++y) {
            unsigned char* line = img.scanLine(y);
            for (int x = 0; x < width; ++x) {
                const int o = x * 3;
                line[o] = r[idx];
                line[o + 1] = g[idx];
                line[o + 2] = b[idx];
                ++idx;
            }
        }
        *out = img;
        return true;
    } catch (const HException& e) {
        if (err) {
            *err = QString::fromLocal8Bit(e.ErrorMessage().Text());
        }
        return false;
    }
}

#endif // HSIMAGE_HAS_HALCON

// gray/color bench: split buffer->H and H->buffer; exclude CopyImage ownership copy
ConvertBenchRst benchGrayRoundTrip(const QImage& src, bool toHalconOnly)
{
    ConvertBenchRst r;
#ifndef HSIMAGE_HAS_HALCON
    r.error = QString::fromLocal8Bit("未启用 Halcon");
    return r;
#else
    if (src.isNull()) {
        r.error = QString::fromLocal8Bit("图像为空");
        return r;
    }
    QString err;
    HObject himg;
    QElapsedTimer t;

    t.start();
    if (!grayQImageToHObject(src, &himg, &err)) {
        r.error = err;
        return r;
    }
    r.msToH = elapsedMs(t);

    if (!toHalconOnly) {
        QImage back;
        t.restart();
        if (!grayHObjectToQImage(himg, &back, &err)) {
            r.error = err;
            return r;
        }
        r.msFromH = elapsedMs(t);
        r.outImage = back;
    }

    r.ms = r.msToH + (r.msFromH > 0.0 ? r.msFromH : 0.0);
    r.ok = true;
    r.detail = QString::fromLocal8Bit("灰度 Pointer1+memcpy  %1x%2")
                   .arg(src.width())
                   .arg(src.height());
    return r;
#endif
}

// 彩色往返计时
// - 方案1 拆/合通道都走逐点 for；方案2 用 InterleaveChannels；方案3 用 Decompose3 拼回
// - 进 Halcon 用 GenImageInterleaved、GenImage3、Compose3、GetImagePointer*
// - 计时不含 GenImageConst，也不含 CopyImage 那种把 RGB888 拷成自有缓冲
ConvertBenchRst benchColorConvert(const QImage& src, ColorConvertMethod method, bool toHalconOnly)
{
    ConvertBenchRst r;
#ifndef HSIMAGE_HAS_HALCON
    r.error = QString::fromLocal8Bit("未启用 Halcon");
    return r;
#else
    if (src.isNull()) {
        r.error = QString::fromLocal8Bit("图像为空");
        return r;
    }
    QString err;
    HObject himg;
    QImage back;
    QElapsedTimer t;

    const char* methodName = "ForLoop";
    if (method == ColorMethodInterleave) {
        methodName = "InterleaveChannels";
    } else if (method == ColorMethodGenImage3) {
        methodName = "GenImage3/Decompose3";
    } else if (method == ColorMethodOpenCvMerge) {
        methodName = "OpenCV split/merge";
    }

    try {
        if (method == ColorMethodForLoop) {
            // 方案1：逐点拆通道写入再 Compose3，回来同样逐点拼
            const QImage rgb = (src.format() == QImage::Format_RGB888)
                ? src
                : src.convertToFormat(QImage::Format_RGB888);
            if (rgb.isNull()) {
                r.error = QString::fromLocal8Bit("图像为空");
                return r;
            }
            const int width = rgb.width();
            const int height = rgb.height();
            HImage hr;
            HImage hg;
            HImage hb;
            hr.GenImageConst("byte", width, height);
            hg.GenImageConst("byte", width, height);
            hb.GenImageConst("byte", width, height);
            HString type;
            Hlong gw = 0;
            Hlong gh = 0;
            unsigned char* pr = static_cast<unsigned char*>(hr.GetImagePointer1(&type, &gw, &gh));
            unsigned char* pg = static_cast<unsigned char*>(hg.GetImagePointer1(&type, &gw, &gh));
            unsigned char* pb = static_cast<unsigned char*>(hb.GetImagePointer1(&type, &gw, &gh));
            if (!pr || !pg || !pb || gw != width || gh != height) {
                r.error = QString::fromLocal8Bit("GenImageConst 失败");
                return r;
            }
            t.start();
            int idx = 0;
            for (int y = 0; y < height; ++y) {
                const unsigned char* line = rgb.constScanLine(y);
                for (int x = 0; x < width; ++x) {
                    const int o = x * 3;
                    pr[idx] = line[o];
                    pg[idx] = line[o + 1];
                    pb[idx] = line[o + 2];
                    ++idx;
                }
            }
            Compose3(hr, hg, hb, &himg);
            r.msToH = elapsedMs(t);
            if (!toHalconOnly) {
                t.restart();
                if (!colorForLoopFromH(himg, &back, &err)) {
                    r.error = err;
                    return r;
                }
                r.msFromH = elapsedMs(t);
                r.outImage = back;
            }
        } else if (method == ColorMethodInterleave) {
            // 方案2：行紧凑时直接交织，否则先收成连续缓冲
            const QImage rgb = (src.format() == QImage::Format_RGB888)
                ? src
                : src.convertToFormat(QImage::Format_RGB888);
            if (rgb.isNull()) {
                r.error = QString::fromLocal8Bit("图像为空");
                return r;
            }
            const int width = rgb.width();
            const int height = rgb.height();
            std::vector<unsigned char> keep;
            const unsigned char* pixelPtr = nullptr;
            double msTight = 0.0;
            if (rgb.bytesPerLine() == width * 3) {
                pixelPtr = rgb.constBits();
            } else {
                t.start();
                keep.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
                for (int y = 0; y < height; ++y) {
                    memcpy(&keep[static_cast<size_t>(y) * static_cast<size_t>(width) * 3],
                           rgb.constScanLine(y), static_cast<size_t>(width) * 3);
                }
                msTight = elapsedMs(t);
                pixelPtr = &keep[0];
            }

            t.start();
            GenEmptyObj(&himg);
            const double msEmpty = elapsedMs(t);

            t.start();
            GenImageInterleaved(&himg, reinterpret_cast<Hlong>(const_cast<unsigned char*>(pixelPtr)), "rgb",
                                width, height, 0, "byte", width, height, 0, 0, -1, 0);
            const double msGenInt = elapsedMs(t);
            r.msToH = msEmpty + msGenInt;

            double msInterleave = -1.0;
            double msPointer1 = -1.0;
            double msAllocQ = -1.0;
            double msMemcpy = -1.0;
            double msWrap = -1.0;
            if (!toHalconOnly) {
                HTuple channel;
                CountChannels(himg, &channel);
                if (channel.I() < 3) {
                    r.error = QString::fromLocal8Bit("不是三通道图像");
                    return r;
                }
                HTuple pixelW;
                HTuple pixelH;
                GetImageSize(himg, &pixelW, &pixelH);
                const int pw = pixelW.I();
                const int ph = pixelH.I();

                HObject interleaved;
                t.start();
                InterleaveChannels(himg, &interleaved, "rgb", "match", 0);
                msInterleave = elapsedMs(t);

                HTuple ptr;
                HTuple type;
                HTuple strideT;
                HTuple heightT;
                t.start();
                GetImagePointer1(interleaved, &ptr, &type, &strideT, &heightT);
                msPointer1 = elapsedMs(t);

                const unsigned char* srcPtr = reinterpret_cast<unsigned char*>(ptr.L());
                const int stride = strideT.I();

                // 只包一层不拷贝，必须保证 interleaved 活过这次 H->buf
                t.start();
                {
                    QImage wrapped(srcPtr, pw, ph, stride, QImage::Format_RGB888);
                    (void)wrapped.constBits();
                }
                msWrap = elapsedMs(t);

                // 分配 QImage 自己的缓冲。这段不计入 H 到缓冲的时间
                t.start();
                QImage qimg(pw, ph, QImage::Format_RGB888);
                msAllocQ = elapsedMs(t);

                const size_t nbytes = static_cast<size_t>(pw) * static_cast<size_t>(ph) * 3;
                t.start();
                if (qimg.bytesPerLine() == pw * 3 && stride == pw * 3) {
                    memcpy(qimg.bits(), srcPtr, nbytes);
                } else {
                    for (int y = 0; y < ph; ++y) {
                        memcpy(qimg.scanLine(y),
                               srcPtr + static_cast<size_t>(y) * static_cast<size_t>(stride),
                               static_cast<size_t>(pw) * 3);
                    }
                }
                msMemcpy = elapsedMs(t);

                // H 到缓冲只加交织、取指针和 memcpy，不含分配 QImage
                r.msFromH = msInterleave + msPointer1 + msMemcpy;
                back = qimg;
                r.outImage = back;
            }

            QString st;
            if (msTight > 0.0) {
                st += QString::fromLatin1("    tight_rows: %1 ms\n").arg(msTight, 0, 'f', 3);
            } else {
                st += QString::fromLatin1("    tight_rows: 0 (direct bits)\n");
            }
            st += QString::fromLatin1("    GenEmptyObj: %1 ms\n").arg(msEmpty, 0, 'f', 3);
            st += QString::fromLatin1("    GenImageInterleaved: %1 ms\n").arg(msGenInt, 0, 'f', 3);
            if (msInterleave >= 0.0) {
                st += QString::fromLatin1("    InterleaveChannels: %1 ms\n").arg(msInterleave, 0, 'f', 3);
                st += QString::fromLatin1("    GetImagePointer1: %1 ms\n").arg(msPointer1, 0, 'f', 3);
                st += QString::fromLatin1("    QImage_wrap_no_copy: %1 ms (ref only)\n").arg(msWrap, 0, 'f', 3);
                st += QString::fromLatin1("    QImage_alloc: %1 ms (not in H->buf sum)\n").arg(msAllocQ, 0, 'f', 3);
                st += QString::fromLatin1("    memcpy_to_QImage: %1 ms\n").arg(msMemcpy, 0, 'f', 3);
            }
            r.stages = st;
        } else if (method == ColorMethodOpenCvMerge) {
#ifdef HSIMAGE_HAS_OPENCV
            // 方案4：OpenCV split/merge。进去逐行拷进 Mat，merge 出来再逐行写回
            const QImage rgb = (src.format() == QImage::Format_RGB888)
                ? src
                : src.convertToFormat(QImage::Format_RGB888);
            if (rgb.isNull()) {
                r.error = QString::fromLocal8Bit("图像为空");
                return r;
            }
            const int width = rgb.width();
            const int height = rgb.height();
            cv::Mat mat(height, width, CV_8UC3);
            for (int y = 0; y < height; ++y) {
                memcpy(mat.ptr(y), rgb.constScanLine(y), static_cast<size_t>(width) * 3);
            }
            std::vector<cv::Mat> planes;
            t.start();
            cv::split(mat, planes);
            if (planes.size() != 3) {
                r.error = QString::fromLocal8Bit("cv::split 失败");
                return r;
            }
            GenImage3(&himg, "byte", width, height, reinterpret_cast<Hlong>(planes[0].data),
                      reinterpret_cast<Hlong>(planes[1].data), reinterpret_cast<Hlong>(planes[2].data));
            r.msToH = elapsedMs(t);
            if (!toHalconOnly) {
                HTuple type;
                HTuple w;
                HTuple h;
                HTuple pr;
                HTuple pg;
                HTuple pb;
                t.restart();
                GetImagePointer3(himg, &pr, &pg, &pb, &type, &w, &h);
                const int pw = w.I();
                const int ph = h.I();
                const size_t n = static_cast<size_t>(pw) * static_cast<size_t>(ph);
                cv::Mat mr(ph, pw, CV_8UC1);
                cv::Mat mg(ph, pw, CV_8UC1);
                cv::Mat mb(ph, pw, CV_8UC1);
                memcpy(mr.data, reinterpret_cast<void*>(pr.L()), n);
                memcpy(mg.data, reinterpret_cast<void*>(pg.L()), n);
                memcpy(mb.data, reinterpret_cast<void*>(pb.L()), n);
                std::vector<cv::Mat> outPlanes;
                outPlanes.push_back(mr);
                outPlanes.push_back(mg);
                outPlanes.push_back(mb);
                cv::Mat outMat;
                cv::merge(outPlanes, outMat);
                r.msFromH = elapsedMs(t);
                QImage qimg(pw, ph, QImage::Format_RGB888);
                for (int y = 0; y < ph; ++y) {
                    memcpy(qimg.scanLine(y), outMat.ptr(y), static_cast<size_t>(pw) * 3);
                }
                back = qimg;
                r.outImage = back;
            }
#else
            r.error = QString::fromLocal8Bit("未启用 OpenCV，不能测 split/merge");
            return r;
#endif
        } else {
            // 方案3：拆平面 + GenImage3 / Decompose3 + 逐点拼。平面缓冲由本函数持有
            std::vector<unsigned char> keepR;
            std::vector<unsigned char> keepG;
            std::vector<unsigned char> keepB;
            const QImage rgb = (src.format() == QImage::Format_RGB888)
                ? src
                : src.convertToFormat(QImage::Format_RGB888);
            if (rgb.isNull()) {
                r.error = QString::fromLocal8Bit("图像为空");
                return r;
            }
            const int width = rgb.width();
            const int height = rgb.height();
            t.start();
            if (!splitRgbToPlanes(rgb, &keepR, &keepG, &keepB, &err)) {
                r.error = err;
                return r;
            }
            if (!colorGenImage3ToH(width, height, &keepR[0], &keepG[0], &keepB[0], &himg, &err)) {
                r.error = err;
                return r;
            }
            r.msToH = elapsedMs(t);
            if (!toHalconOnly) {
                t.restart();
                if (!colorDecompose3FromH(himg, &back, &err)) {
                    r.error = err;
                    return r;
                }
                r.msFromH = elapsedMs(t);
                r.outImage = back;
            }
        }
        r.ms = r.msToH + (r.msFromH > 0.0 ? r.msFromH : 0.0);
        r.ok = true;
        r.detail = QString::fromLocal8Bit("彩色[%1]  %2x%3")
                       .arg(QString::fromLatin1(methodName))
                       .arg(src.width())
                       .arg(src.height());
        return r;
    } catch (const HException& e) {
        r.error = QString::fromLocal8Bit(e.ErrorMessage().Text());
        return r;
    }
#endif
}
